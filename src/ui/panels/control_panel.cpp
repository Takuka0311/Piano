#include "piano/ui/panels/control_panel.h"

#include <array>
#include <cstdio>

#include "imgui.h"

namespace piano::ui::panels {
namespace {

const char* StateToText(app::PlaybackState state) {
  switch (state) {
    case app::PlaybackState::kIdle:
      return "idle";
    case app::PlaybackState::kPlaying:
      return "playing";
    case app::PlaybackState::kError:
      return "error";
    default:
      return "unknown";
  }
}

}  // namespace

ControlPanelActions RenderControlPanel(platform::UiConfig* config,
                                       const app::PlaybackSnapshot& snapshot,
                                       const std::string& status_message) {
  ControlPanelActions actions;
  ImGui::Begin("Control");

  char keyboard_path[512];
  char score_path[512];
  std::snprintf(keyboard_path, sizeof(keyboard_path), "%s", config->keyboard_path.c_str());
  std::snprintf(score_path, sizeof(score_path), "%s", config->score_path.c_str());

  if (ImGui::InputText("Keyboard File", keyboard_path, sizeof(keyboard_path))) {
    config->keyboard_path = keyboard_path;
  }
  ImGui::SameLine();
  if (ImGui::Button("Browse##keyboard")) {
    actions.browse_keyboard_requested = true;
  }

  if (ImGui::InputText("Score File", score_path, sizeof(score_path))) {
    config->score_path = score_path;
  }
  ImGui::SameLine();
  if (ImGui::Button("Browse##score")) {
    actions.browse_score_requested = true;
  }

  const std::array<const char*, 2> backends = {"wasapi", "log"};
  int backend_idx = config->audio_backend == "log" ? 1 : 0;
  if (ImGui::Combo("Audio Backend", &backend_idx, backends.data(), static_cast<int>(backends.size()))) {
    config->audio_backend = backends[backend_idx];
  }

  ImGui::InputInt("Sample Rate", &config->sample_rate);
  ImGui::InputInt("Buffer (ms)", &config->buffer_ms);
  if (config->sample_rate < 1) config->sample_rate = 1;
  if (config->buffer_ms < 1) config->buffer_ms = 1;

  if (ImGui::Button("Play")) {
    actions.play_requested = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop")) {
    actions.stop_requested = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Save Config")) {
    actions.save_requested = true;
  }

  ImGui::Text("State: %s", StateToText(snapshot.state));
  ImGui::TextWrapped("Status: %s", snapshot.message.empty() ? status_message.c_str() : snapshot.message.c_str());
  ImGui::End();
  return actions;
}

}  // namespace piano::ui::panels
