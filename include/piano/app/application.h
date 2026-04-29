#pragma once

#include <string>

namespace piano::app {

struct AppOptions {
  std::string keyboard_map_path;
  std::string score_path;
  std::string probe_key;
  std::string audio_backend = "wasapi";
  int sample_rate = 48000;
  int buffer_ms = 40;
};

class Application {
 public:
  int Run(const AppOptions& options) const;
};

}  // namespace piano::app
