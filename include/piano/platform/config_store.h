#pragma once

#include <string>

namespace piano::platform {

struct UiConfig {
  std::string keyboard_path = "assets/default.keyboard";
  std::string score_path = "assets/demo.in";
  std::string output_mode = "wasapi";
  std::string audio_backend = "wasapi";
  std::string backend_priority = "vsti,midiout,wasapi,dsound,log";
  std::string midi_out_device;
  std::string vsti_plugin_path = "assets/mdaPiano.dll";
  std::string export_wav_path = "output.wav";
  std::string recent_keyboard_path;
  std::string recent_score_path;
  int sample_rate = 48000;
  int buffer_ms = 40;
};

class ConfigStore {
 public:
  explicit ConfigStore(std::string path);

  UiConfig Load() const;
  bool Save(const UiConfig& config, std::string* error_message) const;
  const std::string& Path() const;

 private:
  std::string path_;
};

}  // namespace piano::platform
