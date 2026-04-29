#include "piano/ui/panels/keyboard_panel.h"

#include <string>

#include "imgui.h"

namespace piano::ui::panels {

void RenderKeyboardPanel(const app::PlaybackSnapshot& snapshot) {
  ImGui::Begin("Keyboard");
  for (int midi = 48; midi <= 84; ++midi) {
    const bool active = snapshot.active_notes.count(midi) > 0;
    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(80, 180, 255, 255));
    }
    const std::string label = std::to_string(midi);
    ImGui::Button(label.c_str(), ImVec2(36, 24));
    if (active) {
      ImGui::PopStyleColor();
    }
    if ((midi - 48 + 1) % 12 != 0) {
      ImGui::SameLine();
    }
  }
  ImGui::End();
}

}  // namespace piano::ui::panels
