#pragma once

#include <string>

#include "piano/app/playback_service.h"
#include "piano/platform/config_store.h"

namespace piano::ui::panels {

struct ControlPanelActions {
  bool play_requested = false;
  bool stop_requested = false;
  bool save_requested = false;
  bool browse_keyboard_requested = false;
  bool browse_score_requested = false;
};

ControlPanelActions RenderControlPanel(platform::UiConfig* config,
                                       const app::PlaybackSnapshot& snapshot,
                                       const std::string& status_message);

}  // namespace piano::ui::panels
