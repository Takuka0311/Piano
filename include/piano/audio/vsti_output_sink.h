#pragma once

#include <atomic>
#include <mutex>
#include <string>

#include <windows.h>

#include "piano/audio/midi_output_sink.h"

namespace piano::audio {

class VstiOutputSink : public OutputSink {
 public:
  VstiOutputSink(std::string plugin_path, std::string midi_device_name);
  ~VstiOutputSink() override;

  bool Start(std::string* error_message) override;
  void Stop() override;
  void Emit(const engine::ScheduledEvent& event) override;
  bool IsHealthy(std::string* error_message) const override;

 private:
  void MarkUnhealthy(const std::string& error);

  std::string plugin_path_;
  std::atomic<bool> started_{false};
  std::atomic<bool> healthy_{true};
  HMODULE plugin_module_ = nullptr;
  MidiOutputSink midi_fallback_sink_;
  mutable std::mutex state_mutex_;
  std::string last_error_;
};

}  // namespace piano::audio
