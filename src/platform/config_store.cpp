#include "piano/platform/config_store.h"

#include <fstream>
#include <string>

namespace piano::platform {

ConfigStore::ConfigStore(std::string path) : path_(std::move(path)) {}

UiConfig ConfigStore::Load() const {
  UiConfig config;
  std::ifstream in(path_);
  if (!in.is_open()) {
    return config;
  }

  std::string line;
  while (std::getline(in, line)) {
    const auto pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, pos);
    const std::string value = line.substr(pos + 1);
    try {
      if (key == "keyboard_path") {
        config.keyboard_path = value;
      } else if (key == "score_path") {
        config.score_path = value;
      } else if (key == "output_mode") {
        config.output_mode = value;
      } else if (key == "audio_backend") {
        config.audio_backend = value;
      } else if (key == "backend_priority") {
        config.backend_priority = value;
      } else if (key == "midi_out_device") {
        config.midi_out_device = value;
      } else if (key == "vsti_plugin_path") {
        config.vsti_plugin_path = value;
      } else if (key == "recent_keyboard_path") {
        config.recent_keyboard_path = value;
      } else if (key == "recent_score_path") {
        config.recent_score_path = value;
      } else if (key == "sample_rate") {
        config.sample_rate = std::stoi(value);
      } else if (key == "buffer_ms") {
        config.buffer_ms = std::stoi(value);
      }
    } catch (...) {
      // Keep defaults when parsing fails.
    }
  }
  return config;
}

bool ConfigStore::Save(const UiConfig& config, std::string* error_message) const {
  std::ofstream out(path_, std::ios::trunc);
  if (!out.is_open()) {
    if (error_message != nullptr) {
      *error_message = "failed to open config file for write: " + path_;
    }
    return false;
  }

  out << "keyboard_path=" << config.keyboard_path << '\n';
  out << "score_path=" << config.score_path << '\n';
  out << "output_mode=" << config.output_mode << '\n';
  out << "audio_backend=" << config.audio_backend << '\n';
  out << "backend_priority=" << config.backend_priority << '\n';
  out << "midi_out_device=" << config.midi_out_device << '\n';
  out << "vsti_plugin_path=" << config.vsti_plugin_path << '\n';
  out << "recent_keyboard_path=" << config.recent_keyboard_path << '\n';
  out << "recent_score_path=" << config.recent_score_path << '\n';
  out << "sample_rate=" << config.sample_rate << '\n';
  out << "buffer_ms=" << config.buffer_ms << '\n';
  return true;
}

const std::string& ConfigStore::Path() const {
  return path_;
}

}  // namespace piano::platform
