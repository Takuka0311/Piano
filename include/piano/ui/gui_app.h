#pragma once

#include <string>

#include "piano/app/playback_service.h"
#include "piano/platform/config_store.h"

namespace piano::ui {

class GuiApp {
 public:
  GuiApp();
  ~GuiApp();

  platform::UiConfig Config() const;
  void SetConfig(const platform::UiConfig& config);

  bool StartPlayback(std::string* error_message);
  void StopPlayback();
  bool ExportWav(std::string* error_message);
  app::PlaybackSnapshot Snapshot() const;
  bool SaveConfig(std::string* error_message) const;

 private:
  platform::ConfigStore config_store_;
  platform::UiConfig config_;
  app::PlaybackService playback_service_;
};

}  // namespace piano::ui
