#pragma once

#include <string>

namespace piano::app {

struct AppOptions {
  std::string keyboard_map_path;
  std::string score_path;
  std::string probe_key;
  std::string output_mode = "wasapi";
  std::string audio_backend = "wasapi";
  std::string backend_priority = "vsti,midiout,wasapi,dsound,log";
  std::string midi_out_device;
  std::string vsti_plugin_path;
  int sample_rate = 48000;
  int buffer_ms = 40;
};

class Application {
 public:
  int Run(const AppOptions& options) const;
};

}  // namespace piano::app
