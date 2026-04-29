#include "piano/ui/gui_app.h"

namespace piano::ui {

GuiApp::GuiApp() : config_store_("piano-ui-config.ini"), config_(config_store_.Load()) {}

GuiApp::~GuiApp() {
  StopPlayback();
  std::string ignored_error;
  (void)SaveConfig(&ignored_error);
}

platform::UiConfig GuiApp::Config() const {
  return config_;
}

void GuiApp::SetConfig(const platform::UiConfig& config) {
  config_ = config;
}

bool GuiApp::StartPlayback(std::string* error_message) {
  StopPlayback();
  app::AppOptions options;
  options.keyboard_map_path = config_.keyboard_path;
  options.score_path = config_.score_path;
  options.audio_backend = config_.audio_backend;
  options.backend_priority = config_.backend_priority;
  options.sample_rate = config_.sample_rate;
  options.buffer_ms = config_.buffer_ms;
  config_.recent_keyboard_path = config_.keyboard_path;
  config_.recent_score_path = config_.score_path;
  return playback_service_.Start(options, error_message);
}

void GuiApp::StopPlayback() {
  playback_service_.Stop();
}

app::PlaybackSnapshot GuiApp::Snapshot() const {
  return playback_service_.Snapshot();
}

bool GuiApp::SaveConfig(std::string* error_message) const {
  return config_store_.Save(config_, error_message);
}

}  // namespace piano::ui
