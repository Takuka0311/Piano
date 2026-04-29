#include "piano/ui/gui_app.h"

#include "piano/export/wav_exporter.h"

namespace piano::ui {

GuiApp::GuiApp() : config_store_("piano-ui-config.ini"), config_(config_store_.Load()) {
  if (config_.output_mode.empty()) {
    config_.output_mode = config_.audio_backend.empty() ? "wasapi" : config_.audio_backend;
  }
  if (config_.audio_backend.empty()) {
    config_.audio_backend = config_.output_mode;
  }
  if (config_.export_wav_path.empty()) {
    config_.export_wav_path = "output.wav";
  }
  if (config_.keyboard_path.empty() ||
      config_.keyboard_path.find("reference/Piano/Piano/init.keyboard") != std::string::npos) {
    config_.keyboard_path = "assets/default.keyboard";
  }
  if (config_.vsti_plugin_path.empty() ||
      config_.vsti_plugin_path.find("reference/Piano/Piano/mdaPiano.dll") != std::string::npos) {
    config_.vsti_plugin_path = "assets/mdaPiano.dll";
  }
}

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
  options.output_mode = config_.output_mode;
  options.audio_backend = config_.audio_backend;
  options.backend_priority = config_.backend_priority;
  options.midi_out_device = config_.midi_out_device;
  options.vsti_plugin_path = config_.vsti_plugin_path;
  options.sample_rate = config_.sample_rate;
  options.buffer_ms = config_.buffer_ms;
  config_.recent_keyboard_path = config_.keyboard_path;
  config_.recent_score_path = config_.score_path;
  return playback_service_.Start(options, error_message);
}

void GuiApp::StopPlayback() {
  playback_service_.Stop();
}

bool GuiApp::ExportWav(std::string* error_message) {
  app::AppOptions options;
  options.keyboard_map_path = config_.keyboard_path;
  options.score_path = config_.score_path;
  options.output_mode = config_.output_mode;
  options.audio_backend = config_.audio_backend;
  options.backend_priority = config_.backend_priority;
  options.midi_out_device = config_.midi_out_device;
  options.vsti_plugin_path = config_.vsti_plugin_path;
  options.sample_rate = config_.sample_rate;
  options.buffer_ms = config_.buffer_ms;
  options.export_wav_path = config_.export_wav_path;
  return exporter::ExportScoreToWav(options, config_.export_wav_path, error_message);
}

app::PlaybackSnapshot GuiApp::Snapshot() const {
  return playback_service_.Snapshot();
}

bool GuiApp::SaveConfig(std::string* error_message) const {
  return config_store_.Save(config_, error_message);
}

}  // namespace piano::ui
