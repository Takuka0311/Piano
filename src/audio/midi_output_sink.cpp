#include "piano/audio/midi_output_sink.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace piano::audio {

MidiOutputSink::MidiOutputSink(std::string device_name) : device_name_(std::move(device_name)) {}

MidiOutputSink::~MidiOutputSink() {
  Stop();
}

bool MidiOutputSink::Start(std::string* error_message) {
  if (started_.load()) {
    return true;
  }

  healthy_.store(true);
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_.clear();
  }

  UINT device_id = MIDI_MAPPER;
  if (!device_name_.empty() && !ParseDeviceIndex(device_name_, &device_id)) {
    // Fall back to system mapper when configured device is unavailable.
    device_id = MIDI_MAPPER;
  }

  MMRESULT result = midiOutOpen(&midi_out_, device_id, 0, 0, CALLBACK_NULL);
  if (result != MMSYSERR_NOERROR || midi_out_ == nullptr) {
    result = midiOutOpen(&midi_out_, MIDI_MAPPER, 0, 0, CALLBACK_NULL);
  }
  if (result != MMSYSERR_NOERROR || midi_out_ == nullptr) {
    if (error_message != nullptr) {
      *error_message = "MIDI out open failed";
    }
    healthy_.store(false);
    return false;
  }

  started_.store(true);
  return true;
}

void MidiOutputSink::Stop() {
  if (!started_.exchange(false)) {
    return;
  }
  if (midi_out_ != nullptr) {
    midiOutReset(midi_out_);
    midiOutClose(midi_out_);
    midi_out_ = nullptr;
  }
}

void MidiOutputSink::Emit(const engine::ScheduledEvent& event) {
  if (!started_.load() || midi_out_ == nullptr) {
    return;
  }
  if (event.midi_key < 0 || event.midi_key > 127) {
    return;
  }
  if (event.type == engine::EventType::kNoteOn) {
    const BYTE velocity = static_cast<BYTE>(std::clamp(event.value, 1, 127));
    std::string ignored;
    (void)SendShortMessage(0x90, static_cast<BYTE>(event.midi_key), velocity, &ignored);
  } else if (event.type == engine::EventType::kNoteOff) {
    std::string ignored;
    (void)SendShortMessage(0x80, static_cast<BYTE>(event.midi_key), 0, &ignored);
  }
}

bool MidiOutputSink::IsHealthy(std::string* error_message) const {
  const bool ok = healthy_.load();
  if (error_message != nullptr) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    *error_message = ok ? std::string() : last_error_;
  }
  return ok;
}

std::vector<std::string> MidiOutputSink::ListOutputDevices() {
  std::vector<std::string> devices;
  const UINT count = midiOutGetNumDevs();
  devices.reserve(count);
  for (UINT i = 0; i < count; ++i) {
    MIDIOUTCAPSA caps = {};
    if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
      devices.push_back(caps.szPname);
    }
  }
  return devices;
}

bool MidiOutputSink::ParseDeviceIndex(const std::string& name_or_index, UINT* index) {
  if (index == nullptr) {
    return false;
  }

  char* end_ptr = nullptr;
  const long parsed = std::strtol(name_or_index.c_str(), &end_ptr, 10);
  if (end_ptr != nullptr && *end_ptr == '\0' && parsed >= 0) {
    *index = static_cast<UINT>(parsed);
    return true;
  }

  const UINT count = midiOutGetNumDevs();
  for (UINT i = 0; i < count; ++i) {
    MIDIOUTCAPSA caps = {};
    if (midiOutGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) {
      continue;
    }
    if (name_or_index == caps.szPname) {
      *index = i;
      return true;
    }
  }
  return false;
}

DWORD MidiOutputSink::BuildShortMessage(BYTE status, BYTE data1, BYTE data2) {
  return static_cast<DWORD>(status) | (static_cast<DWORD>(data1) << 8u) | (static_cast<DWORD>(data2) << 16u);
}

bool MidiOutputSink::SendShortMessage(BYTE status, BYTE data1, BYTE data2, std::string* error_message) {
  if (midi_out_ == nullptr) {
    if (error_message != nullptr) {
      *error_message = "MIDI out handle is null";
    }
    return false;
  }
  const MMRESULT result = midiOutShortMsg(midi_out_, BuildShortMessage(status, data1, data2));
  if (result == MMSYSERR_NOERROR) {
    return true;
  }
  if (error_message != nullptr) {
    *error_message = "MIDI short message send failed";
  }
  MarkUnhealthy("MIDI short message send failed");
  return false;
}

void MidiOutputSink::MarkUnhealthy(const std::string& error) {
  healthy_.store(false);
  std::lock_guard<std::mutex> lock(state_mutex_);
  last_error_ = error;
}

}  // namespace piano::audio
