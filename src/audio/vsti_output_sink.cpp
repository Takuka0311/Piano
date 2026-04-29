#include "piano/audio/vsti_output_sink.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <utility>

#include "../../reference/Piano/Piano/vst/aeffectx.h"

namespace piano::audio {
namespace {

constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;
constexpr int kVstSampleRate = 44100;
constexpr int kVstBlockFrames = 256;

using PluginEntryProc = ::AEffect* (*)(audioMasterCallback);

VstIntPtr VSTCALLBACK HostCallback(::AEffect*,
                                   VstInt32 opcode,
                                   VstInt32,
                                   VstIntPtr,
                                   void*,
                                   float) {
  switch (opcode) {
    case audioMasterVersion:
      return kVstVersion;
    case audioMasterGetSampleRate:
      return kVstSampleRate;
    case audioMasterGetBlockSize:
      return kVstBlockFrames;
    case audioMasterGetCurrentProcessLevel:
      return kVstProcessLevelRealtime;
    case DECLARE_VST_DEPRECATED(audioMasterWantMidi):
      return 1;
    case DECLARE_VST_DEPRECATED(audioMasterNeedIdle):
      return 1;
    default:
      return 0;
  }
}

}  // namespace

VstiOutputSink::VstiOutputSink(std::string plugin_path,
                               std::string midi_device_name,
                               int sample_rate,
                               int buffer_ms)
    : plugin_path_(std::move(plugin_path)),
      midi_device_name_(std::move(midi_device_name)),
      requested_sample_rate_(sample_rate),
      requested_buffer_ms_(buffer_ms) {}

VstiOutputSink::~VstiOutputSink() {
  Stop();
}

bool VstiOutputSink::Start(std::string* error_message) {
  if (started_.load()) {
    return true;
  }

  healthy_.store(true);
  running_.store(false);
  stream_sample_rate_ = kVstSampleRate;
  block_frames_ = kVstBlockFrames;
  write_cursor_ = 0;

  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_.clear();
    midi_queue_.clear();
    active_notes_.clear();
  }

  if (!InitializePlugin(error_message) || !InitializeAudio(error_message)) {
    ShutdownAudio();
    ShutdownPlugin();
    healthy_.store(false);
    return false;
  }

  left_buffer_.assign(static_cast<std::size_t>(block_frames_), 0.0f);
  right_buffer_.assign(static_cast<std::size_t>(block_frames_), 0.0f);
  temp_buffer_.assign(static_cast<std::size_t>(block_frames_), 0.0f);
  pcm_buffer_.assign(static_cast<std::size_t>(block_frames_ * kChannels), 0);

  running_.store(true);
  started_.store(true);
  render_thread_ = std::thread(&VstiOutputSink::RenderLoop, this);
  return true;
}

void VstiOutputSink::Stop() {
  if (!started_.exchange(false)) {
    return;
  }
  running_.store(false);
  if (render_thread_.joinable()) {
    render_thread_.join();
  }
  ShutdownAudio();
  ShutdownPlugin();
}

void VstiOutputSink::Emit(const engine::ScheduledEvent& event) {
  if (!started_.load()) {
    return;
  }
  if (event.midi_key < 0 || event.midi_key > 127) {
    return;
  }
  if (event.type == engine::EventType::kNoteOn) {
    QueueMidi(0x90, event.midi_key, (std::max)(1, (std::min)(127, event.value)));
  } else if (event.type == engine::EventType::kNoteOff) {
    QueueMidi(0x80, event.midi_key, 0);
  }
}

bool VstiOutputSink::IsHealthy(std::string* error_message) const {
  const bool ok = healthy_.load() && started_.load();
  if (error_message != nullptr) {
    if (ok) {
      *error_message = std::string();
    } else {
      std::lock_guard<std::mutex> lock(state_mutex_);
      *error_message = last_error_;
    }
  }
  return ok;
}

bool VstiOutputSink::InitializePlugin(std::string* error_message) {
  if (plugin_path_.empty()) {
    if (error_message != nullptr) {
      *error_message = "VSTi plugin path is empty";
    }
    return false;
  }

  plugin_module_ = LoadLibraryA(plugin_path_.c_str());
  if (plugin_module_ == nullptr) {
    if (error_message != nullptr) {
      *error_message = "VSTi load failed: " + plugin_path_;
    }
    return false;
  }

  auto* entry = reinterpret_cast<PluginEntryProc>(GetProcAddress(plugin_module_, "VSTPluginMain"));
  if (entry == nullptr) {
    entry = reinterpret_cast<PluginEntryProc>(GetProcAddress(plugin_module_, "main"));
  }
  if (entry == nullptr) {
    if (error_message != nullptr) {
      *error_message = "VSTi entry point not found (VSTPluginMain/main)";
    }
    return false;
  }

  effect_ = entry(HostCallback);
  if (effect_ == nullptr || effect_->magic != kEffectMagic) {
    if (error_message != nullptr) {
      *error_message = "VSTi create effect failed";
    }
    effect_ = nullptr;
    return false;
  }

  effect_->dispatcher(effect_, effOpen, 0, 0, nullptr, 0.0f);
  effect_->dispatcher(effect_, effBeginSetProgram, 0, 0, nullptr, 0.0f);
  effect_->dispatcher(effect_, effSetProgram, 0, 0, nullptr, 0.0f);
  effect_->dispatcher(effect_, effEndSetProgram, 0, 0, nullptr, 0.0f);
  effect_->dispatcher(effect_, effSetSampleRate, 0, 0, nullptr, static_cast<float>(kVstSampleRate));
  effect_->dispatcher(effect_, effSetBlockSize, 0, kVstBlockFrames, nullptr, 0.0f);
  effect_->dispatcher(effect_, effMainsChanged, 0, 1, nullptr, 0.0f);
  return true;
}

bool VstiOutputSink::InitializeAudio(std::string* error_message) {
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
  primary_desc.dwSize = sizeof(primary_desc);
  primary_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
  hr = dsound_->CreateSoundBuffer(&primary_desc, &primary_buffer_, nullptr);
  if (FAILED(hr) || primary_buffer_ == nullptr) {
    if (error_message != nullptr) *error_message = "DirectSound create primary buffer failed";
    return false;
  }

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
  secondary_desc.dwSize = sizeof(secondary_desc);
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
  return true;
}

void VstiOutputSink::ShutdownPlugin() {
  if (effect_ != nullptr) {
    effect_->dispatcher(effect_, effMainsChanged, 0, 0, nullptr, 0.0f);
    effect_->dispatcher(effect_, effClose, 0, 0, nullptr, 0.0f);
    effect_ = nullptr;
  }
  if (plugin_module_ != nullptr) {
    FreeLibrary(plugin_module_);
    plugin_module_ = nullptr;
  }
}

void VstiOutputSink::ShutdownAudio() {
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

void VstiOutputSink::RenderLoop() {
  const DWORD block_align = (kChannels * kBitsPerSample) / 8;
  const DWORD chunk_bytes = static_cast<DWORD>(block_frames_) * block_align;

  while (running_.load()) {
    if (secondary_buffer_ == nullptr || effect_ == nullptr) {
      MarkUnhealthy("VSTi render state invalid");
      break;
    }

    DWORD play_cursor = 0;
    DWORD write_cursor = 0;
    HRESULT hr = secondary_buffer_->GetCurrentPosition(&play_cursor, &write_cursor);
    if (FAILED(hr)) {
      MarkUnhealthy("VSTi GetCurrentPosition failed");
      break;
    }

    DWORD distance = (write_cursor_ <= play_cursor)
                         ? (play_cursor - write_cursor_)
                         : (buffer_bytes_ - write_cursor_ + play_cursor);
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
      MarkUnhealthy("VSTi buffer lock failed");
      break;
    }

    RenderFrames(pcm_buffer_.data(), block_frames_);
    if (p1 != nullptr && b1 > 0) {
      std::memcpy(p1, pcm_buffer_.data(), b1);
    }
    if (p2 != nullptr && b2 > 0) {
      std::memcpy(p2, reinterpret_cast<const char*>(pcm_buffer_.data()) + b1, b2);
    }

    hr = secondary_buffer_->Unlock(p1, b1, p2, b2);
    if (FAILED(hr)) {
      MarkUnhealthy("VSTi buffer unlock failed");
      break;
    }

    write_cursor_ = (write_cursor_ + chunk_bytes) % buffer_bytes_;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  running_.store(false);
}

void VstiOutputSink::RenderFrames(int16_t* interleaved, int frames) {
  if (effect_ == nullptr || interleaved == nullptr || frames <= 0) {
    return;
  }

  struct MidiEventsBlock {
    VstInt32 numEvents;
    VstIntPtr reserved;
    VstEvent* events[256];
  } event_buffer = {};
  VstMidiEvent midi_events[256] = {};
  int midi_count = 0;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    while (!midi_queue_.empty() && midi_count < 256) {
      const auto queued = midi_queue_.front();
      midi_queue_.pop_front();
      auto& event = midi_events[midi_count];
      event.type = kVstMidiType;
      event.byteSize = sizeof(VstMidiEvent);
      event.deltaFrames = 0;
      event.flags = kVstMidiEventIsRealtime;
      event.noteLength = 0;
      event.noteOffset = 0;
      event.midiData[0] = static_cast<char>(queued.status);
      event.midiData[1] = static_cast<char>(queued.data1);
      event.midiData[2] = static_cast<char>(queued.data2);
      event.midiData[3] = 0;
      event_buffer.events[midi_count] = reinterpret_cast<VstEvent*>(&event);
      ++midi_count;
    }
  }

  if (midi_count > 0) {
    event_buffer.numEvents = midi_count;
    event_buffer.reserved = 0;
    effect_->dispatcher(effect_, effCanDo, 0, 0, reinterpret_cast<void*>(const_cast<char*>("receiveVstMidiEvent")), 0.0f);
    effect_->dispatcher(effect_, effProcessEvents, 0, 0, reinterpret_cast<VstEvents*>(&event_buffer), 0.0f);
  }

  std::fill(left_buffer_.begin(), left_buffer_.begin() + frames, 0.0f);
  std::fill(right_buffer_.begin(), right_buffer_.begin() + frames, 0.0f);
  std::fill(temp_buffer_.begin(), temp_buffer_.begin() + frames, 0.0f);

  float* outputs[64] = {};
  float* inputs[64] = {};
  outputs[0] = left_buffer_.data();
  outputs[1] = right_buffer_.data();
  for (int i = 2; i < 64; ++i) {
    outputs[i] = temp_buffer_.data();
  }
  for (int i = 0; i < 64; ++i) {
    inputs[i] = temp_buffer_.data();
  }

  effect_->processReplacing(effect_, inputs, outputs, frames);

  for (int i = 0; i < frames; ++i) {
    const float l = (std::max)(-1.0f, (std::min)(1.0f, left_buffer_[i]));
    const float r = (std::max)(-1.0f, (std::min)(1.0f, right_buffer_[i]));
    interleaved[i * 2] = static_cast<int16_t>(l * 32767.0f);
    interleaved[i * 2 + 1] = static_cast<int16_t>(r * 32767.0f);
  }
}

void VstiOutputSink::QueueMidi(unsigned char status, int midi_key, int velocity) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  const bool note_on = (status & 0xF0) == 0x90 && velocity > 0;
  const bool note_off = ((status & 0xF0) == 0x80) || (((status & 0xF0) == 0x90) && velocity == 0);
  if (note_on) {
    if (active_notes_.count(midi_key) > 0) {
      return;
    }
    active_notes_.insert(midi_key);
  } else if (note_off) {
    if (active_notes_.count(midi_key) == 0) {
      return;
    }
    active_notes_.erase(midi_key);
  }
  midi_queue_.push_back({status,
                         static_cast<unsigned char>(midi_key & 0x7F),
                         static_cast<unsigned char>(velocity & 0x7F)});
}

void VstiOutputSink::MarkUnhealthy(const std::string& error) {
  healthy_.store(false);
  std::lock_guard<std::mutex> lock(state_mutex_);
  last_error_ = error;
}

}  // namespace piano::audio
