#include "piano/audio/dsound_output_sink.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

#include <windows.h>

namespace piano::audio {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;

}  // namespace

DsoundOutputSink::DsoundOutputSink(int sample_rate, int buffer_ms)
    : requested_sample_rate_(sample_rate), requested_buffer_ms_(buffer_ms) {}

DsoundOutputSink::~DsoundOutputSink() {
  Stop();
}

bool DsoundOutputSink::Start(std::string* error_message) {
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

  if (!InitializeDevice(error_message)) {
    ShutdownDevice();
    healthy_.store(false);
    return false;
  }

  running_.store(true);
  started_.store(true);
  render_thread_ = std::thread(&DsoundOutputSink::RenderLoop, this);
  return true;
}

void DsoundOutputSink::Stop() {
  if (!started_.exchange(false)) {
    return;
  }
  running_.store(false);
  if (render_thread_.joinable()) {
    render_thread_.join();
  }
  ShutdownDevice();
}

void DsoundOutputSink::Emit(const engine::ScheduledEvent& event) {
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

bool DsoundOutputSink::IsHealthy(std::string* error_message) const {
  const bool ok = healthy_.load();
  if (error_message != nullptr) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    *error_message = ok ? std::string() : last_error_;
  }
  return ok;
}

bool DsoundOutputSink::InitializeDevice(std::string* error_message) {
  const HWND hwnd = GetDesktopWindow();
  HRESULT hr = DirectSoundCreate8(nullptr, &dsound_, nullptr);
  if (FAILED(hr) || dsound_ == nullptr) {
    if (error_message != nullptr) *error_message = "DirectSoundCreate8 failed";
    return false;
  }
  hr = dsound_->SetCooperativeLevel(hwnd, DSSCL_PRIORITY);
  if (FAILED(hr)) {
    if (error_message != nullptr) *error_message = "DirectSound SetCooperativeLevel failed";
    return false;
  }

  DSBUFFERDESC primary_desc = {};
  primary_desc.dwSize = sizeof(DSBUFFERDESC);
  primary_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
  hr = dsound_->CreateSoundBuffer(&primary_desc, &primary_buffer_, nullptr);
  if (FAILED(hr) || primary_buffer_ == nullptr) {
    if (error_message != nullptr) *error_message = "DirectSound create primary buffer failed";
    return false;
  }

  stream_sample_rate_ = requested_sample_rate_ > 0 ? requested_sample_rate_ : 48000;
  WAVEFORMATEX format = {};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = kChannels;
  format.nSamplesPerSec = static_cast<DWORD>(stream_sample_rate_);
  format.wBitsPerSample = kBitsPerSample;
  format.nBlockAlign = static_cast<WORD>((format.nChannels * format.wBitsPerSample) / 8);
  format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

  hr = primary_buffer_->SetFormat(&format);
  if (FAILED(hr)) {
    if (error_message != nullptr) *error_message = "DirectSound set primary format failed";
    return false;
  }

  const int buffer_ms = requested_buffer_ms_ > 0 ? requested_buffer_ms_ : 40;
  buffer_bytes_ = static_cast<DWORD>((format.nAvgBytesPerSec * (std::max)(buffer_ms * 4, 200)) / 1000);
  buffer_bytes_ = (buffer_bytes_ / format.nBlockAlign) * format.nBlockAlign;

  DSBUFFERDESC secondary_desc = {};
  secondary_desc.dwSize = sizeof(DSBUFFERDESC);
  secondary_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
  secondary_desc.dwBufferBytes = buffer_bytes_;
  secondary_desc.lpwfxFormat = &format;
  hr = dsound_->CreateSoundBuffer(&secondary_desc, &secondary_buffer_, nullptr);
  if (FAILED(hr) || secondary_buffer_ == nullptr) {
    if (error_message != nullptr) *error_message = "DirectSound create secondary buffer failed";
    return false;
  }

  void* p1 = nullptr;
  void* p2 = nullptr;
  DWORD b1 = 0;
  DWORD b2 = 0;
  hr = secondary_buffer_->Lock(0, buffer_bytes_, &p1, &b1, &p2, &b2, 0);
  if (SUCCEEDED(hr)) {
    if (p1 != nullptr) std::memset(p1, 0, b1);
    if (p2 != nullptr) std::memset(p2, 0, b2);
    secondary_buffer_->Unlock(p1, b1, p2, b2);
  }

  hr = secondary_buffer_->Play(0, 0, DSBPLAY_LOOPING);
  if (FAILED(hr)) {
    if (error_message != nullptr) *error_message = "DirectSound play failed";
    return false;
  }
  write_cursor_ = 0;
  return true;
}

void DsoundOutputSink::ShutdownDevice() {
  if (secondary_buffer_ != nullptr) {
    secondary_buffer_->Stop();
    secondary_buffer_->Release();
    secondary_buffer_ = nullptr;
  }
  if (primary_buffer_ != nullptr) {
    primary_buffer_->Release();
    primary_buffer_ = nullptr;
  }
  if (dsound_ != nullptr) {
    dsound_->Release();
    dsound_ = nullptr;
  }
}

void DsoundOutputSink::RenderLoop() {
  const DWORD block_align = (kChannels * kBitsPerSample) / 8;
  const DWORD chunk_bytes = static_cast<DWORD>((std::max)(stream_sample_rate_ / 100, 64) *
                                                static_cast<int>(block_align));

  while (running_.load()) {
    if (secondary_buffer_ == nullptr) {
      MarkUnhealthy("DirectSound: secondary buffer missing");
      break;
    }

    DWORD play_cursor = 0;
    DWORD write_cursor = 0;
    HRESULT hr = secondary_buffer_->GetCurrentPosition(&play_cursor, &write_cursor);
    if (FAILED(hr)) {
      MarkUnhealthy("DirectSound: GetCurrentPosition failed");
      break;
    }

    DWORD distance = (write_cursor_ <= play_cursor) ? (play_cursor - write_cursor_) : (buffer_bytes_ - write_cursor_ + play_cursor);
    if (distance < chunk_bytes) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      continue;
    }

    void* p1 = nullptr;
    void* p2 = nullptr;
    DWORD b1 = 0;
    DWORD b2 = 0;
    hr = secondary_buffer_->Lock(write_cursor_, chunk_bytes, &p1, &b1, &p2, &b2, 0);
    if (FAILED(hr)) {
      MarkUnhealthy("DirectSound: Lock failed");
      break;
    }

    ApplyQueuedEvents();
    auto fill = [&](void* ptr, DWORD bytes) {
      if (ptr == nullptr || bytes == 0) return;
      auto* samples = reinterpret_cast<int16_t*>(ptr);
      const DWORD sample_count = bytes / sizeof(int16_t);
      for (DWORD i = 0; i < sample_count; i += kChannels) {
        const int16_t sample = MixOneSample();
        for (int ch = 0; ch < kChannels; ++ch) {
          samples[i + ch] = sample;
        }
      }
    };
    fill(p1, b1);
    fill(p2, b2);

    hr = secondary_buffer_->Unlock(p1, b1, p2, b2);
    if (FAILED(hr)) {
      MarkUnhealthy("DirectSound: Unlock failed");
      break;
    }

    write_cursor_ = (write_cursor_ + chunk_bytes) % buffer_bytes_;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  running_.store(false);
}

void DsoundOutputSink::ApplyQueuedEvents() {
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

int16_t DsoundOutputSink::MixOneSample() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  if (active_notes_.empty()) {
    return 0;
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
  return static_cast<int16_t>(mixed * 32767.0);
}

void DsoundOutputSink::MarkUnhealthy(const std::string& message) {
  healthy_.store(false);
  std::lock_guard<std::mutex> lock(state_mutex_);
  last_error_ = message;
}

float DsoundOutputSink::MidiToFrequency(int midi_key) {
  return 440.0f * std::pow(2.0f, static_cast<float>(midi_key - 69) / 12.0f);
}

}  // namespace piano::audio
