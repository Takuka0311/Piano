#include "piano/audio/wasapi_output_sink.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <thread>

#include <combaseapi.h>
#include <ks.h>
#include <ksmedia.h>

namespace piano::audio {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr REFERENCE_TIME kHnsPerMs = 10000;

}  // namespace

WasapiOutputSink::WasapiOutputSink(int sample_rate, int buffer_ms)
    : requested_sample_rate_(sample_rate), requested_buffer_ms_(buffer_ms) {}

WasapiOutputSink::~WasapiOutputSink() {
  Stop();
}

bool WasapiOutputSink::Start(std::string* error_message) {
  if (started_.load()) {
    return true;
  }

  healthy_.store(true);
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_.clear();
    active_notes_.clear();
    event_queue_.clear();
  }

  const HRESULT init_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(init_result) && init_result != RPC_E_CHANGED_MODE) {
    if (error_message != nullptr) {
      *error_message = "WASAPI: CoInitializeEx failed";
    }
    healthy_.store(false);
    return false;
  }
  com_initialized_ = (init_result == S_OK || init_result == S_FALSE);

  if (!InitializeDevice(error_message)) {
    ShutdownDevice();
    if (com_initialized_) {
      CoUninitialize();
      com_initialized_ = false;
    }
    healthy_.store(false);
    return false;
  }

  if (FAILED(audio_client_->Start())) {
    if (error_message != nullptr) {
      *error_message = "WASAPI: IAudioClient::Start failed";
    }
    ShutdownDevice();
    if (com_initialized_) {
      CoUninitialize();
      com_initialized_ = false;
    }
    healthy_.store(false);
    return false;
  }

  running_.store(true);
  started_.store(true);
  render_thread_ = std::thread(&WasapiOutputSink::RenderLoop, this);
  return true;
}

void WasapiOutputSink::Stop() {
  if (!started_.exchange(false)) {
    return;
  }
  running_.store(false);

  if (render_thread_.joinable()) {
    render_thread_.join();
  }

  if (audio_client_ != nullptr) {
    audio_client_->Stop();
  }
  ShutdownDevice();
  if (com_initialized_) {
    CoUninitialize();
    com_initialized_ = false;
  }
}

void WasapiOutputSink::Emit(const engine::ScheduledEvent& event) {
  if (!started_.load()) {
    return;
  }
  if (event.midi_key < 0 || event.midi_key > 127) {
    return;
  }
  if (event.type != engine::EventType::kNoteOn && event.type != engine::EventType::kNoteOff) {
    return;
  }

  std::lock_guard<std::mutex> lock(state_mutex_);
  event_queue_.push_back({event.type, event.midi_key, event.value});
}

bool WasapiOutputSink::IsHealthy(std::string* error_message) const {
  const bool ok = healthy_.load();
  if (error_message != nullptr) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    *error_message = ok ? std::string() : last_error_;
  }
  return ok;
}

bool WasapiOutputSink::InitializeDevice(std::string* error_message) {
  IMMDeviceEnumerator* enumerator = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
  if (FAILED(hr)) {
    if (error_message != nullptr) {
      *error_message = "WASAPI: failed to create device enumerator";
    }
    return false;
  }

  hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
  enumerator->Release();
  if (FAILED(hr) || device_ == nullptr) {
    if (error_message != nullptr) {
      *error_message = "WASAPI: failed to open default render endpoint";
    }
    return false;
  }

  hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                         reinterpret_cast<void**>(&audio_client_));
  if (FAILED(hr) || audio_client_ == nullptr) {
    if (error_message != nullptr) {
      *error_message = "WASAPI: failed to activate IAudioClient";
    }
    return false;
  }

  hr = audio_client_->GetMixFormat(&mix_format_);
  if (FAILED(hr) || mix_format_ == nullptr) {
    if (error_message != nullptr) {
      *error_message = "WASAPI: failed to get mix format";
    }
    return false;
  }

  if (mix_format_->nSamplesPerSec > 0) {
    stream_sample_rate_ = static_cast<int>(mix_format_->nSamplesPerSec);
  } else if (requested_sample_rate_ > 0) {
    stream_sample_rate_ = requested_sample_rate_;
  }
  stream_channels_ = mix_format_->nChannels > 0 ? mix_format_->nChannels : 2;

  stream_is_float_ = false;
  if (mix_format_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT && mix_format_->wBitsPerSample == 32) {
    stream_is_float_ = true;
  } else if (mix_format_->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
    const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(mix_format_);
    stream_is_float_ =
        (extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) && mix_format_->wBitsPerSample == 32;
  }

  const int buffer_ms = requested_buffer_ms_ > 0 ? requested_buffer_ms_ : 40;
  const REFERENCE_TIME buffer_duration = static_cast<REFERENCE_TIME>(buffer_ms) * kHnsPerMs;
  hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, buffer_duration, 0, mix_format_, nullptr);
  if (FAILED(hr)) {
    if (error_message != nullptr) {
      *error_message = "WASAPI: IAudioClient::Initialize failed";
    }
    return false;
  }

  hr = audio_client_->GetBufferSize(&buffer_frames_);
  if (FAILED(hr)) {
    if (error_message != nullptr) {
      *error_message = "WASAPI: failed to get buffer size";
    }
    return false;
  }

  hr = audio_client_->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render_client_));
  if (FAILED(hr) || render_client_ == nullptr) {
    if (error_message != nullptr) {
      *error_message = "WASAPI: failed to get render service";
    }
    return false;
  }

  BYTE* data = nullptr;
  hr = render_client_->GetBuffer(buffer_frames_, &data);
  if (SUCCEEDED(hr) && data != nullptr) {
    std::memset(data, 0, buffer_frames_ * mix_format_->nBlockAlign);
    render_client_->ReleaseBuffer(buffer_frames_, 0);
  }

  return true;
}

void WasapiOutputSink::ShutdownDevice() {
  if (render_client_ != nullptr) {
    render_client_->Release();
    render_client_ = nullptr;
  }
  if (mix_format_ != nullptr) {
    CoTaskMemFree(mix_format_);
    mix_format_ = nullptr;
  }
  if (audio_client_ != nullptr) {
    audio_client_->Release();
    audio_client_ = nullptr;
  }
  if (device_ != nullptr) {
    device_->Release();
    device_ = nullptr;
  }
}

void WasapiOutputSink::RenderLoop() {
  while (running_.load()) {
    if (render_client_ == nullptr || audio_client_ == nullptr) {
      MarkUnhealthy("WASAPI: render loop invalid client state");
      running_.store(false);
      break;
    }

    UINT32 padding = 0;
    HRESULT hr = audio_client_->GetCurrentPadding(&padding);
    if (FAILED(hr)) {
      MarkUnhealthy("WASAPI: GetCurrentPadding failed");
      running_.store(false);
      break;
    }
    if (padding >= buffer_frames_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    const UINT32 writable_frames = buffer_frames_ - padding;
    if (writable_frames == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    BYTE* data = nullptr;
    hr = render_client_->GetBuffer(writable_frames, &data);
    if (FAILED(hr) || data == nullptr) {
      MarkUnhealthy("WASAPI: GetBuffer failed");
      running_.store(false);
      break;
    }

    ApplyQueuedEvents();
    const int channels = stream_channels_;
    for (UINT32 frame = 0; frame < writable_frames; ++frame) {
      const float sample = MixOneSample();
      if (stream_is_float_) {
        auto* out = reinterpret_cast<float*>(data);
        for (int ch = 0; ch < channels; ++ch) {
          out[frame * channels + ch] = sample;
        }
      } else {
        auto* out = reinterpret_cast<int16_t*>(data);
        const int16_t pcm = static_cast<int16_t>(std::clamp(sample * 32767.0f, -32768.0f, 32767.0f));
        for (int ch = 0; ch < channels; ++ch) {
          out[frame * channels + ch] = pcm;
        }
      }
    }

    hr = render_client_->ReleaseBuffer(writable_frames, 0);
    if (FAILED(hr)) {
      MarkUnhealthy("WASAPI: ReleaseBuffer failed");
      running_.store(false);
      break;
    }
  }
}

void WasapiOutputSink::ApplyQueuedEvents() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  while (!event_queue_.empty()) {
    const auto event = event_queue_.front();
    event_queue_.pop_front();

    if (event.type == engine::EventType::kNoteOn) {
      ActiveNote note;
      note.frequency_hz = MidiToFrequency(event.midi_key);
      note.amplitude = static_cast<float>(std::clamp(event.value, 1, 127)) / 127.0f * 0.2f;
      note.phase = 0.0;
      active_notes_[event.midi_key] = note;
    } else if (event.type == engine::EventType::kNoteOff) {
      active_notes_.erase(event.midi_key);
    }
  }
}

float WasapiOutputSink::MixOneSample() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (active_notes_.empty()) {
    return 0.0f;
  }

  double mixed = 0.0;
  const double dt = 1.0 / static_cast<double>(stream_sample_rate_);
  for (auto& [midi, note] : active_notes_) {
    (void)midi;
    mixed += std::sin(note.phase) * note.amplitude;
    note.phase += 2.0 * kPi * note.frequency_hz * dt;
    if (note.phase > 2.0 * kPi) {
      note.phase = std::fmod(note.phase, 2.0 * kPi);
    }
  }

  mixed = std::clamp(mixed, -1.0, 1.0);
  return static_cast<float>(mixed);
}

void WasapiOutputSink::MarkUnhealthy(const std::string& message) {
  healthy_.store(false);
  std::lock_guard<std::mutex> lock(state_mutex_);
  last_error_ = message;
}

float WasapiOutputSink::MidiToFrequency(int midi_key) {
  return 440.0f * std::pow(2.0f, static_cast<float>(midi_key - 69) / 12.0f);
}

}  // namespace piano::audio
