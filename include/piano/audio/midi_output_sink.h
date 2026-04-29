#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>
#include <mmsystem.h>

#include "piano/audio/output_sink.h"

namespace piano::audio {

class MidiOutputSink : public OutputSink {
 public:
  explicit MidiOutputSink(std::string device_name);
  ~MidiOutputSink() override;

  bool Start(std::string* error_message) override;
  void Stop() override;
  void Emit(const engine::ScheduledEvent& event) override;
  bool IsHealthy(std::string* error_message) const override;

  static std::vector<std::string> ListOutputDevices();

 private:
  static bool ParseDeviceIndex(const std::string& name_or_index, UINT* index);
  static DWORD BuildShortMessage(BYTE status, BYTE data1, BYTE data2);
  bool SendShortMessage(BYTE status, BYTE data1, BYTE data2, std::string* error_message);
  void MarkUnhealthy(const std::string& error);

  std::string device_name_;
  std::atomic<bool> started_{false};
  std::atomic<bool> healthy_{true};
  HMIDIOUT midi_out_ = nullptr;
  mutable std::mutex state_mutex_;
  std::string last_error_;
};

}  // namespace piano::audio
