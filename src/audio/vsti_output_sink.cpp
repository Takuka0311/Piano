#include "piano/audio/vsti_output_sink.h"

#include <utility>

namespace piano::audio {

VstiOutputSink::VstiOutputSink(std::string plugin_path, std::string midi_device_name)
    : plugin_path_(std::move(plugin_path)), midi_fallback_sink_(std::move(midi_device_name)) {}

VstiOutputSink::~VstiOutputSink() {
  Stop();
}

bool VstiOutputSink::Start(std::string* error_message) {
  if (started_.load()) {
    return true;
  }
  healthy_.store(true);
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    last_error_.clear();
  }

  if (plugin_path_.empty()) {
    if (error_message != nullptr) {
      *error_message = "VSTi plugin path is empty";
    }
    healthy_.store(false);
    return false;
  }

  plugin_module_ = LoadLibraryA(plugin_path_.c_str());
  if (plugin_module_ == nullptr) {
    if (error_message != nullptr) {
      *error_message = "VSTi load failed: " + plugin_path_;
    }
    healthy_.store(false);
    return false;
  }

  auto* entry1 = GetProcAddress(plugin_module_, "VSTPluginMain");
  auto* entry2 = GetProcAddress(plugin_module_, "main");
  if (entry1 == nullptr && entry2 == nullptr) {
    if (error_message != nullptr) {
      *error_message = "VSTi entry point not found (VSTPluginMain/main)";
    }
    MarkUnhealthy("VSTi entry point missing");
    FreeLibrary(plugin_module_);
    plugin_module_ = nullptr;
    return false;
  }

  std::string midi_error;
  if (!midi_fallback_sink_.Start(&midi_error)) {
    if (error_message != nullptr) {
      *error_message = "VSTi midi fallback start failed: " + midi_error;
    }
    MarkUnhealthy("VSTi midi fallback start failed");
    FreeLibrary(plugin_module_);
    plugin_module_ = nullptr;
    return false;
  }

  started_.store(true);
  return true;
}

void VstiOutputSink::Stop() {
  if (!started_.exchange(false)) {
    return;
  }

  midi_fallback_sink_.Stop();
  if (plugin_module_ != nullptr) {
    FreeLibrary(plugin_module_);
    plugin_module_ = nullptr;
  }
}

void VstiOutputSink::Emit(const engine::ScheduledEvent& event) {
  if (!started_.load()) {
    return;
  }
  midi_fallback_sink_.Emit(event);
}

bool VstiOutputSink::IsHealthy(std::string* error_message) const {
  const bool local_ok = healthy_.load();
  std::string midi_error;
  const bool midi_ok = midi_fallback_sink_.IsHealthy(&midi_error);
  const bool ok = local_ok && midi_ok;
  if (error_message != nullptr) {
    if (ok) {
      *error_message = std::string();
    } else if (!local_ok) {
      std::lock_guard<std::mutex> lock(state_mutex_);
      *error_message = last_error_;
    } else {
      *error_message = midi_error;
    }
  }
  return ok;
}

void VstiOutputSink::MarkUnhealthy(const std::string& error) {
  healthy_.store(false);
  std::lock_guard<std::mutex> lock(state_mutex_);
  last_error_ = error;
}

}  // namespace piano::audio
