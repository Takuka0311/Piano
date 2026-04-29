#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "piano/app/audio_backend_factory.h"
#include "piano/app/application.h"
#include "piano/audio/output_sink.h"
#include "piano/input/keyboard_map.h"
#include "piano/ui/gui_app.h"

namespace {

enum : int {
  kTimerId = 1001,
  kEditKeyboard = 2001,
  kEditScore = 2002,
  kComboBackend = 2003,
  kEditSampleRate = 2004,
  kEditBufferMs = 2005,
  kEditMidiDevice = 2006,
  kEditVstiPlugin = 2007,
  kBtnPlay = 2008,
  kBtnStop = 2009,
  kBtnSave = 2010,
  kBtnBrowseKeyboard = 2011,
  kBtnBrowseScore = 2012,
  kBtnBrowseVsti = 2013,
  kBtnExportWav = 2014,
  kEditExportWav = 2015,
  kBtnBrowseExportWav = 2016,
  kListActive = 2017,
  kLabelState = 2018,
  kLabelStatus = 2019,
  kLabelDiagnostics = 2020,
};

constexpr int kComputerKeyboardLeft = 8;
constexpr int kComputerKeyboardTop = 8;
constexpr int kComputerKeyboardWidth = 748;
constexpr int kComputerKeyboardHeight = 200;
constexpr int kPianoLeft = 8;
constexpr int kPianoTop = 218;
constexpr int kPianoWidth = 748;
constexpr int kPianoHeight = 70;
constexpr int kBottomPanelTop = 298;
constexpr int kBottomPanelHeight = 214;

struct GuiContext {
  piano::ui::GuiApp app;
  piano::app::PlaybackSnapshot last_snapshot;
  piano::input::KeyboardMap live_keyboard_map;
  std::unordered_map<int, int> vk_to_midi;
  std::unordered_map<std::string, int> key_to_midi;
  std::set<std::string> active_key_names;
  std::string mouse_pressed_key;
  int mouse_pressed_midi = -1;
  std::unique_ptr<piano::audio::OutputSink> live_sink;
  std::string live_sink_mode;
  std::string live_sink_backend;
  int smoke_exit_ms = 0;
  std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();
};

struct VisualKey {
  std::string label;
  RECT rect;
};

struct PianoVisualKey {
  RECT rect;
  bool black = false;
  int midi = -1;
};

piano::platform::UiConfig ReadConfigFromControls(HWND hwnd);
void SetText(HWND hwnd, int control_id, const std::string& value);

std::vector<std::string> SupportedKeyNames() {
  return {"Esc", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
          "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Backspace",
          "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "\\",
          "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "Return",
          "Z", "X", "C", "V", "B", "N", "M", ",", ".", "/", "Space"};
}

std::vector<VisualKey> BuildVisualKeyboard() {
  std::vector<VisualKey> keys;
  const int key_w = 34;
  const int key_h = 24;
  const int gap = 4;
  const std::vector<std::vector<std::string>> rows = {
      {"Esc", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"},
      {"`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Backspace"},
      {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "[", "]", "\\"},
      {"A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "'", "Return"},
      {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "/"},
      {"Space"},
  };

  int y = kComputerKeyboardTop + 8;
  for (std::size_t row_idx = 0; row_idx < rows.size(); ++row_idx) {
    int x = kComputerKeyboardLeft + 8;
    for (const auto& label : rows[row_idx]) {
      int width = key_w;
      if (label == "Backspace" || label == "Return") {
        width = key_w * 2 + gap;
      }
      if (label == "Space") {
        width = key_w * 7 + gap * 6;
      }
      keys.push_back({label, {x, y, x + width, y + key_h}});
      x += width + gap;
    }
    y += key_h + gap;
  }

  const int pad_x = kComputerKeyboardLeft + 560;
  const int pad_y = kComputerKeyboardTop + 44;
  const int pad_w = 42;
  const int pad_h = 28;
  const int pad_gap = 6;
  const std::vector<std::vector<std::string>> numpad = {
      {"7", "8", "9"},
      {"4", "5", "6"},
      {"1", "2", "3"},
      {"0", ".", "Enter"},
  };
  int ny = pad_y;
  for (const auto& row : numpad) {
    int nx = pad_x;
    for (const auto& label : row) {
      int width = (label == "0") ? pad_w * 2 + pad_gap : pad_w;
      keys.push_back({label, {nx, ny, nx + width, ny + pad_h}});
      nx += width + pad_gap;
    }
    ny += pad_h + pad_gap;
  }
  return keys;
}

std::vector<PianoVisualKey> BuildPianoKeys() {
  std::vector<PianoVisualKey> keys;
  constexpr int white_count = 28;
  const int white_w = kPianoWidth / white_count;
  const int white_h = kPianoHeight - 6;
  const int black_w = white_w * 2 / 3;
  const int black_h = white_h * 2 / 3;
  int midi = 48;
  for (int i = 0; i < white_count; ++i) {
    RECT white = {kPianoLeft + i * white_w, kPianoTop + 6, kPianoLeft + (i + 1) * white_w, kPianoTop + 6 + white_h};
    keys.push_back({white, false, midi++});
  }
  const std::vector<int> black_offsets = {0, 1, 3, 4, 5};
  for (int octave = 0; octave < 4; ++octave) {
    int base = octave * 7;
    for (int offset : black_offsets) {
      int i = base + offset;
      if (i + 1 >= white_count) {
        continue;
      }
      int left = kPianoLeft + (i + 1) * white_w - black_w / 2;
      RECT black = {left, kPianoTop + 6, left + black_w, kPianoTop + 6 + black_h};
      keys.push_back({black, true, 49 + octave * 12 + offset * 2 - (offset > 2 ? 1 : 0)});
    }
  }
  return keys;
}

std::string VkToKeyName(WPARAM w_param) {
  if (w_param >= 'A' && w_param <= 'Z') {
    return std::string(1, static_cast<char>(w_param));
  }
  if (w_param >= '0' && w_param <= '9') {
    return std::string(1, static_cast<char>(w_param));
  }
  switch (w_param) {
    case VK_ESCAPE:
      return "Esc";
    case VK_F1:
      return "F1";
    case VK_F2:
      return "F2";
    case VK_F3:
      return "F3";
    case VK_F4:
      return "F4";
    case VK_F5:
      return "F5";
    case VK_F6:
      return "F6";
    case VK_F7:
      return "F7";
    case VK_F8:
      return "F8";
    case VK_F9:
      return "F9";
    case VK_F10:
      return "F10";
    case VK_F11:
      return "F11";
    case VK_F12:
      return "F12";
    case VK_SPACE:
      return "Space";
    case VK_RETURN:
      return "Return";
    case VK_BACK:
      return "Backspace";
    case VK_OEM_3:
      return "`";
    case VK_OEM_MINUS:
      return "-";
    case VK_OEM_PLUS:
      return "=";
    case VK_OEM_4:
      return "[";
    case VK_OEM_6:
      return "]";
    case VK_OEM_1:
      return ";";
    case VK_OEM_7:
      return "'";
    case VK_OEM_COMMA:
      return ",";
    case VK_OEM_PERIOD:
      return ".";
    case VK_OEM_2:
      return "/";
    case VK_OEM_5:
      return "\\";
    default:
      return {};
  }
}

int KeyNameToVirtualKey(const std::string& key_name) {
  if (key_name.size() == 1 && ((key_name[0] >= 'A' && key_name[0] <= 'Z') || (key_name[0] >= '0' && key_name[0] <= '9'))) {
    return static_cast<int>(key_name[0]);
  }
  if (key_name == "Space") return VK_SPACE;
  if (key_name == "Return") return VK_RETURN;
  if (key_name == "Backspace") return VK_BACK;
  if (key_name == "Esc") return VK_ESCAPE;
  if (key_name == "F1") return VK_F1;
  if (key_name == "F2") return VK_F2;
  if (key_name == "F3") return VK_F3;
  if (key_name == "F4") return VK_F4;
  if (key_name == "F5") return VK_F5;
  if (key_name == "F6") return VK_F6;
  if (key_name == "F7") return VK_F7;
  if (key_name == "F8") return VK_F8;
  if (key_name == "F9") return VK_F9;
  if (key_name == "F10") return VK_F10;
  if (key_name == "F11") return VK_F11;
  if (key_name == "F12") return VK_F12;
  if (key_name == "`") return VK_OEM_3;
  if (key_name == "-") return VK_OEM_MINUS;
  if (key_name == "=") return VK_OEM_PLUS;
  if (key_name == "[") return VK_OEM_4;
  if (key_name == "]") return VK_OEM_6;
  if (key_name == ";") return VK_OEM_1;
  if (key_name == "'") return VK_OEM_7;
  if (key_name == ",") return VK_OEM_COMMA;
  if (key_name == ".") return VK_OEM_PERIOD;
  if (key_name == "/") return VK_OEM_2;
  if (key_name == "\\") return VK_OEM_5;
  return 0;
}

std::string MidiToJianpu(int midi) {
  static const char* kTable[12] = {"1", "#1", "2", "#2", "3", "4", "#4", "5", "#5", "6", "#6", "7"};
  if (midi < 0) {
    return "?";
  }
  std::string out = kTable[midi % 12];
  const int octave = midi / 12 - 1;
  if (octave > 4) {
    out.append(static_cast<std::size_t>(octave - 4), '+');
  } else if (octave < 4) {
    out.append(static_cast<std::size_t>(4 - octave), '-');
  }
  return out;
}

void RefreshLiveKeyboardMap(GuiContext* ctx, HWND hwnd) {
  if (ctx == nullptr) {
    return;
  }
  ctx->vk_to_midi.clear();
  ctx->key_to_midi.clear();
  std::string error;
  const auto config = ReadConfigFromControls(hwnd);
  if (!ctx->live_keyboard_map.LoadFromFile(config.keyboard_path, &error)) {
    return;
  }
  for (const auto& key : SupportedKeyNames()) {
    const auto midi = ctx->live_keyboard_map.LookupMidiKey(key);
    if (!midi.has_value()) {
      continue;
    }
    const int vk = KeyNameToVirtualKey(key);
    ctx->key_to_midi[key] = *midi;
    if (vk != 0) {
      ctx->vk_to_midi[vk] = *midi;
    }
  }
}

bool EnsureLiveSink(GuiContext* ctx, HWND hwnd, std::string* error_message) {
  if (ctx == nullptr) {
    return false;
  }
  const auto config = ReadConfigFromControls(hwnd);
  const std::string preferred_backend = config.output_mode.empty() ? "wasapi" : config.output_mode;
  if (ctx->live_sink != nullptr && ctx->live_sink_mode == preferred_backend) {
    std::string error;
    if (ctx->live_sink->IsHealthy(&error)) {
      return true;
    }
    ctx->live_sink->Stop();
    ctx->live_sink.reset();
  }

  piano::app::AppOptions options;
  options.output_mode = preferred_backend;
  options.audio_backend = preferred_backend;
  options.backend_priority = config.backend_priority.empty() ? "vsti,midiout,wasapi,dsound,log" : config.backend_priority;
  options.sample_rate = config.sample_rate;
  options.buffer_ms = config.buffer_ms;
  options.midi_out_device = config.midi_out_device;
  options.vsti_plugin_path = config.vsti_plugin_path;

  const auto preferred = piano::app::ResolveBackendOrder(options);
  std::unique_ptr<piano::audio::OutputSink> sink;
  std::string active_backend;
  if (!piano::app::BuildAndStartSink(options, preferred, &sink, &active_backend, error_message)) {
    return false;
  }
  ctx->live_sink = std::move(sink);
  ctx->live_sink_mode = preferred_backend;
  ctx->live_sink_backend = active_backend;
  SetText(hwnd, kLabelDiagnostics,
          "backend: " + ctx->live_sink_backend +
              "  rec: " + std::to_string(ctx->last_snapshot.recoveries) +
              "  err: " + std::to_string(ctx->last_snapshot.errors));
  return true;
}

void SendLiveNote(GuiContext* ctx, HWND hwnd, int midi_key, bool on) {
  std::string error;
  if (!EnsureLiveSink(ctx, hwnd, &error)) {
    SetText(hwnd, kLabelStatus, "status: " + error);
    return;
  }
  piano::engine::ScheduledEvent event;
  event.type = on ? piano::engine::EventType::kNoteOn : piano::engine::EventType::kNoteOff;
  event.midi_key = midi_key;
  event.value = 90;
  event.source_token = "live";
  ctx->live_sink->Emit(event);
}

const VisualKey* FindVisualKeyAtPoint(int x, int y) {
  static const auto keys = BuildVisualKeyboard();
  for (const auto& key : keys) {
    if (x >= key.rect.left && x <= key.rect.right && y >= key.rect.top && y <= key.rect.bottom) {
      return &key;
    }
  }
  return nullptr;
}

int FindPianoMidiAtPoint(int x, int y) {
  const auto keys = BuildPianoKeys();
  for (const auto& key : keys) {
    if (!key.black) {
      continue;
    }
    if (x >= key.rect.left && x <= key.rect.right && y >= key.rect.top && y <= key.rect.bottom) {
      return key.midi;
    }
  }
  for (const auto& key : keys) {
    if (key.black) {
      continue;
    }
    if (x >= key.rect.left && x <= key.rect.right && y >= key.rect.top && y <= key.rect.bottom) {
      return key.midi;
    }
  }
  return -1;
}

void HandleLiveKeyPress(GuiContext* ctx, HWND hwnd, const std::string& key_name) {
  if (ctx == nullptr || key_name.empty()) {
    return;
  }
  const auto iter = ctx->key_to_midi.find(key_name);
  if (iter == ctx->key_to_midi.end()) {
    return;
  }
  ctx->active_key_names.insert(key_name);
  SendLiveNote(ctx, hwnd, iter->second, true);
  RECT keyboard_rect = {kComputerKeyboardLeft, kComputerKeyboardTop,
                        kComputerKeyboardLeft + kComputerKeyboardWidth, kComputerKeyboardTop + kComputerKeyboardHeight};
  InvalidateRect(hwnd, &keyboard_rect, FALSE);
}

void HandleLiveKeyRelease(GuiContext* ctx, HWND hwnd, const std::string& key_name) {
  if (ctx == nullptr || key_name.empty()) {
    return;
  }
  const auto iter = ctx->key_to_midi.find(key_name);
  if (iter == ctx->key_to_midi.end()) {
    return;
  }
  ctx->active_key_names.erase(key_name);
  SendLiveNote(ctx, hwnd, iter->second, false);
  RECT keyboard_rect = {kComputerKeyboardLeft, kComputerKeyboardTop,
                        kComputerKeyboardLeft + kComputerKeyboardWidth, kComputerKeyboardTop + kComputerKeyboardHeight};
  InvalidateRect(hwnd, &keyboard_rect, FALSE);
}

void DrawKeyboard(HWND hwnd, const GuiContext* ctx) {
  PAINTSTRUCT ps = {};
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT paint_rect = ps.rcPaint;
  HDC mem_hdc = CreateCompatibleDC(hdc);
  HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, paint_rect.right - paint_rect.left, paint_rect.bottom - paint_rect.top);
  HGDIOBJ old_bmp = SelectObject(mem_hdc, mem_bmp);
  SetViewportOrgEx(mem_hdc, -paint_rect.left, -paint_rect.top, nullptr);
  RECT client = {};
  GetClientRect(hwnd, &client);
  HBRUSH bg_brush = CreateSolidBrush(RGB(28, 30, 36));
  FillRect(mem_hdc, &client, bg_brush);
  DeleteObject(bg_brush);

  RECT top_panel = {kComputerKeyboardLeft, kComputerKeyboardTop,
                    kComputerKeyboardLeft + kComputerKeyboardWidth, kComputerKeyboardTop + kComputerKeyboardHeight};
  HBRUSH top_brush = CreateSolidBrush(RGB(214, 217, 224));
  FillRect(mem_hdc, &top_panel, top_brush);
  DeleteObject(top_brush);
  Rectangle(mem_hdc, top_panel.left, top_panel.top, top_panel.right, top_panel.bottom);

  RECT piano_panel = {kPianoLeft, kPianoTop, kPianoLeft + kPianoWidth, kPianoTop + kPianoHeight};
  HBRUSH piano_bg = CreateSolidBrush(RGB(34, 36, 42));
  FillRect(mem_hdc, &piano_panel, piano_bg);
  DeleteObject(piano_bg);
  Rectangle(mem_hdc, piano_panel.left, piano_panel.top, piano_panel.right, piano_panel.bottom);

  RECT bottom_panel = {kPianoLeft, kBottomPanelTop, kPianoLeft + kPianoWidth, kBottomPanelTop + kBottomPanelHeight};
  HBRUSH bottom_bg = CreateSolidBrush(RGB(20, 22, 28));
  FillRect(mem_hdc, &bottom_panel, bottom_bg);
  DeleteObject(bottom_bg);

  const auto keys = BuildVisualKeyboard();
  for (const auto& key : keys) {
    int mapped_midi = -1;
    bool has_mapping = false;
    bool playing_active = false;
    if (ctx != nullptr) {
      const auto it = ctx->key_to_midi.find(key.label);
      if (it != ctx->key_to_midi.end()) {
        has_mapping = true;
        mapped_midi = it->second;
        playing_active = ctx->last_snapshot.active_notes.count(mapped_midi) > 0;
      }
    }
    const bool active = (ctx != nullptr && ctx->active_key_names.count(key.label) > 0) || playing_active;
    const COLORREF color = active ? RGB(124, 190, 255) : (has_mapping ? RGB(246, 246, 250) : RGB(195, 198, 205));
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(mem_hdc, &key.rect, brush);
    DeleteObject(brush);
    Rectangle(mem_hdc, key.rect.left, key.rect.top, key.rect.right, key.rect.bottom);
    SetBkMode(mem_hdc, TRANSPARENT);
    SetTextColor(mem_hdc, RGB(40, 46, 58));
    const std::string text = has_mapping ? MidiToJianpu(mapped_midi) : std::string();
    if (!text.empty()) {
      TextOutA(mem_hdc, key.rect.left + 10, key.rect.top + 7, text.c_str(), static_cast<int>(text.size()));
    }
  }

  const auto piano_keys = BuildPianoKeys();
  for (const auto& key : piano_keys) {
    bool active = false;
    if (ctx != nullptr && key.midi >= 0) {
      active = ctx->last_snapshot.active_notes.count(key.midi) > 0;
    }
    COLORREF color = key.black ? RGB(28, 28, 30) : RGB(248, 248, 248);
    if (active) {
      color = key.black ? RGB(60, 140, 210) : RGB(180, 225, 255);
    }
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(mem_hdc, &key.rect, brush);
    DeleteObject(brush);
    Rectangle(mem_hdc, key.rect.left, key.rect.top, key.rect.right, key.rect.bottom);
  }
  BitBlt(hdc, paint_rect.left, paint_rect.top, paint_rect.right - paint_rect.left, paint_rect.bottom - paint_rect.top,
         mem_hdc, paint_rect.left, paint_rect.top, SRCCOPY);
  SelectObject(mem_hdc, old_bmp);
  DeleteObject(mem_bmp);
  DeleteDC(mem_hdc);
  EndPaint(hwnd, &ps);
}


std::string PickFile(HWND owner, const char* filter) {
  OPENFILENAMEA ofn = {};
  char file_path[MAX_PATH] = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFile = file_path;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = filter;
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
  if (GetOpenFileNameA(&ofn) == TRUE) {
    return std::string(file_path);
  }
  return {};
}

std::string PickSaveFile(HWND owner, const char* filter, const char* default_ext) {
  OPENFILENAMEA ofn = {};
  char file_path[MAX_PATH] = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;
  ofn.lpstrFile = file_path;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = filter;
  ofn.nFilterIndex = 1;
  ofn.lpstrDefExt = default_ext;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
  if (GetSaveFileNameA(&ofn) == TRUE) {
    return std::string(file_path);
  }
  return {};
}

void SetText(HWND hwnd, int control_id, const std::string& value) {
  SetWindowTextA(GetDlgItem(hwnd, control_id), value.c_str());
}

std::string GetText(HWND hwnd, int control_id) {
  char buffer[512] = {};
  GetWindowTextA(GetDlgItem(hwnd, control_id), buffer, sizeof(buffer));
  return std::string(buffer);
}

int ParsePositiveInt(HWND hwnd, int control_id, int fallback, int min_value, int max_value) {
  try {
    int parsed = std::stoi(GetText(hwnd, control_id));
    parsed = (std::max)(min_value, parsed);
    parsed = (std::min)(max_value, parsed);
    return parsed;
  } catch (...) {
    SetText(hwnd, control_id, std::to_string(fallback));
    return fallback;
  }
}

void ApplyConfigToControls(HWND hwnd, const piano::platform::UiConfig& config) {
  SetText(hwnd, kEditKeyboard, config.keyboard_path);
  SetText(hwnd, kEditScore, config.score_path);
  SetText(hwnd, kEditSampleRate, std::to_string(config.sample_rate));
  SetText(hwnd, kEditBufferMs, std::to_string(config.buffer_ms));
  SetText(hwnd, kEditMidiDevice, config.midi_out_device);
  SetText(hwnd, kEditVstiPlugin, config.vsti_plugin_path);
  SetText(hwnd, kEditExportWav, config.export_wav_path);
  const std::string mode = config.output_mode.empty() ? config.audio_backend : config.output_mode;
  int idx = 0;
  if (mode == "midiout") idx = 1;
  if (mode == "dsound") idx = 2;
  if (mode == "log") idx = 3;
  if (mode == "vsti") idx = 4;
  SendMessageA(GetDlgItem(hwnd, kComboBackend), CB_SETCURSEL, idx, 0);
}

piano::platform::UiConfig ReadConfigFromControls(HWND hwnd) {
  piano::platform::UiConfig config;
  config.keyboard_path = GetText(hwnd, kEditKeyboard);
  config.score_path = GetText(hwnd, kEditScore);
  config.sample_rate = ParsePositiveInt(hwnd, kEditSampleRate, 48000, 8000, 192000);
  config.buffer_ms = ParsePositiveInt(hwnd, kEditBufferMs, 40, 1, 300);
  config.midi_out_device = GetText(hwnd, kEditMidiDevice);
  config.vsti_plugin_path = GetText(hwnd, kEditVstiPlugin);
  config.export_wav_path = GetText(hwnd, kEditExportWav);
  if (config.export_wav_path.empty()) {
    config.export_wav_path = "output.wav";
  }
  const LRESULT backend_idx = SendMessageA(GetDlgItem(hwnd, kComboBackend), CB_GETCURSEL, 0, 0);
  if (backend_idx == 1) {
    config.output_mode = "midiout";
  } else if (backend_idx == 2) {
    config.output_mode = "dsound";
  } else if (backend_idx == 3) {
    config.output_mode = "log";
  } else if (backend_idx == 4) {
    config.output_mode = "vsti";
  } else {
    config.output_mode = "wasapi";
  }
  config.audio_backend = config.output_mode;
  config.backend_priority = "vsti,midiout,wasapi,dsound,log";
  config.recent_keyboard_path = config.keyboard_path;
  config.recent_score_path = config.score_path;
  return config;
}

void UpdateSnapshotUI(HWND hwnd, const piano::app::PlaybackSnapshot& snapshot) {
  std::string state_text = "state: ";
  if (snapshot.state == piano::app::PlaybackState::kIdle) {
    state_text += "idle";
  } else if (snapshot.state == piano::app::PlaybackState::kPlaying) {
    state_text += "playing";
  } else {
    state_text += "error";
  }
  SetText(hwnd, kLabelState, state_text);
  SetText(hwnd, kLabelStatus, "status: " + snapshot.message);
  SetText(hwnd, kLabelDiagnostics,
          "backend: " + snapshot.active_backend +
              "  rec: " + std::to_string(snapshot.recoveries) +
              "  err: " + std::to_string(snapshot.errors));

  HWND list = GetDlgItem(hwnd, kListActive);
  SendMessageA(list, LB_RESETCONTENT, 0, 0);
  if (snapshot.active_notes.empty()) {
    const char* idle = "(none)";
    SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(idle));
    return;
  }
  for (int midi : snapshot.active_notes) {
    std::string line = MidiToJianpu(midi) + " [ON]";
    SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
  }
}

void CreateControls(HWND hwnd) {
  CreateWindowA("STATIC", "Keyboard:", WS_CHILD | WS_VISIBLE, 14, 306, 70, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 84, 304, 420, 22, hwnd,
                reinterpret_cast<HMENU>(kEditKeyboard), nullptr, nullptr);
  CreateWindowA("BUTTON", "...", WS_CHILD | WS_VISIBLE, 510, 304, 32, 22, hwnd,
                reinterpret_cast<HMENU>(kBtnBrowseKeyboard), nullptr, nullptr);

  CreateWindowA("STATIC", "Score:", WS_CHILD | WS_VISIBLE, 14, 332, 70, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 84, 330, 420, 22, hwnd,
                reinterpret_cast<HMENU>(kEditScore), nullptr, nullptr);
  CreateWindowA("BUTTON", "...", WS_CHILD | WS_VISIBLE, 510, 330, 32, 22, hwnd,
                reinterpret_cast<HMENU>(kBtnBrowseScore), nullptr, nullptr);

  CreateWindowA("STATIC", "Output:", WS_CHILD | WS_VISIBLE, 14, 358, 70, 20, hwnd, nullptr, nullptr, nullptr);
  HWND combo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 84, 356, 100, 180, hwnd,
                             reinterpret_cast<HMENU>(kComboBackend), nullptr, nullptr);
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("wasapi"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("midiout"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("dsound"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("log"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("vsti"));

  CreateWindowA("STATIC", "Sample:", WS_CHILD | WS_VISIBLE, 196, 358, 54, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 252, 356, 74, 22, hwnd,
                reinterpret_cast<HMENU>(kEditSampleRate), nullptr, nullptr);
  CreateWindowA("STATIC", "Buffer:", WS_CHILD | WS_VISIBLE, 334, 358, 52, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 388, 356, 66, 22, hwnd,
                reinterpret_cast<HMENU>(kEditBufferMs), nullptr, nullptr);

  CreateWindowA("STATIC", "MIDI:", WS_CHILD | WS_VISIBLE, 14, 384, 70, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 84, 382, 370, 22, hwnd,
                reinterpret_cast<HMENU>(kEditMidiDevice), nullptr, nullptr);

  CreateWindowA("STATIC", "VSTi:", WS_CHILD | WS_VISIBLE, 14, 410, 70, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 84, 408, 420, 22, hwnd,
                reinterpret_cast<HMENU>(kEditVstiPlugin), nullptr, nullptr);
  CreateWindowA("BUTTON", "...", WS_CHILD | WS_VISIBLE, 510, 408, 32, 22, hwnd,
                reinterpret_cast<HMENU>(kBtnBrowseVsti), nullptr, nullptr);

  CreateWindowA("STATIC", "Export:", WS_CHILD | WS_VISIBLE, 14, 436, 70, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 84, 434, 420, 22, hwnd,
                reinterpret_cast<HMENU>(kEditExportWav), nullptr, nullptr);
  CreateWindowA("BUTTON", "...", WS_CHILD | WS_VISIBLE, 510, 434, 32, 22, hwnd,
                reinterpret_cast<HMENU>(kBtnBrowseExportWav), nullptr, nullptr);

  CreateWindowA("BUTTON", "Play", WS_CHILD | WS_VISIBLE, 560, 304, 92, 30, hwnd, reinterpret_cast<HMENU>(kBtnPlay),
                nullptr, nullptr);
  CreateWindowA("BUTTON", "Stop", WS_CHILD | WS_VISIBLE, 658, 304, 92, 30, hwnd, reinterpret_cast<HMENU>(kBtnStop),
                nullptr, nullptr);
  CreateWindowA("BUTTON", "Save", WS_CHILD | WS_VISIBLE, 560, 340, 92, 28, hwnd,
                reinterpret_cast<HMENU>(kBtnSave), nullptr, nullptr);
  CreateWindowA("BUTTON", "Export", WS_CHILD | WS_VISIBLE, 658, 340, 92, 28, hwnd,
                reinterpret_cast<HMENU>(kBtnExportWav), nullptr, nullptr);

  CreateWindowA("STATIC", "state: idle", WS_CHILD | WS_VISIBLE, 560, 378, 190, 20, hwnd,
                reinterpret_cast<HMENU>(kLabelState), nullptr, nullptr);
  CreateWindowA("STATIC", "status: ready", WS_CHILD | WS_VISIBLE, 560, 402, 190, 30, hwnd,
                reinterpret_cast<HMENU>(kLabelStatus), nullptr, nullptr);
  CreateWindowA("STATIC", "backend: -", WS_CHILD | WS_VISIBLE, 560, 436, 190, 36,
                hwnd, reinterpret_cast<HMENU>(kLabelDiagnostics), nullptr, nullptr);

  CreateWindowA("STATIC", "Active Notes:", WS_CHILD | WS_VISIBLE, 560, 474, 120, 18, hwnd, nullptr, nullptr,
                nullptr);
  CreateWindowA("LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY, 560, 492, 190, 104, hwnd,
                reinterpret_cast<HMENU>(kListActive), nullptr, nullptr);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
  GuiContext* ctx = reinterpret_cast<GuiContext*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

  switch (msg) {
    case WM_CREATE: {
      auto* create = reinterpret_cast<CREATESTRUCT*>(l_param);
      ctx = reinterpret_cast<GuiContext*>(create->lpCreateParams);
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
      CreateControls(hwnd);
      ApplyConfigToControls(hwnd, ctx->app.Config());
      RefreshLiveKeyboardMap(ctx, hwnd);
      SetTimer(hwnd, kTimerId, 100, nullptr);
      SetFocus(hwnd);
      return 0;
    }
    case WM_COMMAND: {
      if (ctx == nullptr) return 0;
      const int id = LOWORD(w_param);
      const int notify = HIWORD(w_param);
      if (id == kComboBackend && notify == CBN_SELCHANGE) {
        SetFocus(hwnd);
        return 0;
      }
      if (id == kBtnBrowseKeyboard) {
        const auto path = PickFile(hwnd, "Keyboard (*.keyboard)\0*.keyboard\0All Files (*.*)\0*.*\0");
        if (!path.empty()) {
          SetText(hwnd, kEditKeyboard, path);
          RefreshLiveKeyboardMap(ctx, hwnd);
        }
        SetFocus(hwnd);
      } else if (id == kBtnBrowseScore) {
        const auto path = PickFile(hwnd, "Score (*.in)\0*.in\0All Files (*.*)\0*.*\0");
        if (!path.empty()) SetText(hwnd, kEditScore, path);
        SetFocus(hwnd);
      } else if (id == kBtnBrowseVsti) {
        const auto path = PickFile(hwnd, "VST2 Plugin (*.dll)\0*.dll\0All Files (*.*)\0*.*\0");
        if (!path.empty()) SetText(hwnd, kEditVstiPlugin, path);
        SetFocus(hwnd);
      } else if (id == kBtnBrowseExportWav) {
        const auto path = PickSaveFile(hwnd, "Wave File (*.wav)\0*.wav\0All Files (*.*)\0*.*\0", "wav");
        if (!path.empty()) SetText(hwnd, kEditExportWav, path);
        SetFocus(hwnd);
      } else if (id == kBtnPlay) {
        auto config = ReadConfigFromControls(hwnd);
        ctx->app.SetConfig(config);
        RefreshLiveKeyboardMap(ctx, hwnd);
        std::string error;
        if (!ctx->app.StartPlayback(&error)) {
          MessageBoxA(hwnd, error.c_str(), "Playback Error", MB_OK | MB_ICONERROR);
        }
        SetFocus(hwnd);
      } else if (id == kBtnStop) {
        ctx->app.StopPlayback();
        SetFocus(hwnd);
      } else if (id == kBtnSave) {
        auto config = ReadConfigFromControls(hwnd);
        ctx->app.SetConfig(config);
        RefreshLiveKeyboardMap(ctx, hwnd);
        std::string error;
        if (!ctx->app.SaveConfig(&error)) {
          MessageBoxA(hwnd, error.c_str(), "Save Error", MB_OK | MB_ICONERROR);
        }
        SetFocus(hwnd);
      } else if (id == kBtnExportWav) {
        auto config = ReadConfigFromControls(hwnd);
        ctx->app.SetConfig(config);
        std::string error;
        if (!ctx->app.ExportWav(&error)) {
          MessageBoxA(hwnd, error.c_str(), "Export Error", MB_OK | MB_ICONERROR);
        } else {
          SetText(hwnd, kLabelStatus, "status: wav exported to " + config.export_wav_path);
        }
        SetFocus(hwnd);
      }
      return 0;
    }
    case WM_TIMER: {
      if (ctx == nullptr) return 0;
      ctx->last_snapshot = ctx->app.Snapshot();
      UpdateSnapshotUI(hwnd, ctx->last_snapshot);
      RECT top_rect = {kComputerKeyboardLeft, kComputerKeyboardTop,
                       kComputerKeyboardLeft + kComputerKeyboardWidth, kComputerKeyboardTop + kComputerKeyboardHeight};
      RECT piano_rect = {kPianoLeft, kPianoTop, kPianoLeft + kPianoWidth, kPianoTop + kPianoHeight};
      InvalidateRect(hwnd, &top_rect, FALSE);
      InvalidateRect(hwnd, &piano_rect, FALSE);
      if (ctx->smoke_exit_ms > 0) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - ctx->started_at)
                .count();
        if (elapsed >= ctx->smoke_exit_ms) {
          PostQuitMessage(0);
        }
      }
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_DESTROY:
      if (ctx != nullptr) {
        ctx->app.StopPlayback();
        if (ctx->live_sink != nullptr) {
          ctx->live_sink->Stop();
          ctx->live_sink.reset();
        }
      }
      PostQuitMessage(0);
      return 0;
    case WM_KEYDOWN:
      if (ctx != nullptr && (l_param & 0x40000000) == 0) {
        const std::string key_name = VkToKeyName(w_param);
        if (!key_name.empty() && ctx->vk_to_midi.count(static_cast<int>(w_param)) > 0) {
          HandleLiveKeyPress(ctx, hwnd, key_name);
          return 0;
        }
      }
      break;
    case WM_KEYUP:
      if (ctx != nullptr) {
        const std::string key_name = VkToKeyName(w_param);
        if (!key_name.empty() && ctx->vk_to_midi.count(static_cast<int>(w_param)) > 0) {
          HandleLiveKeyRelease(ctx, hwnd, key_name);
          return 0;
        }
      }
      break;
    case WM_LBUTTONDOWN:
      if (ctx != nullptr) {
        const int x = static_cast<short>(LOWORD(l_param));
        const int y = static_cast<short>(HIWORD(l_param));
        if (const VisualKey* key = FindVisualKeyAtPoint(x, y); key != nullptr) {
          ctx->mouse_pressed_key = key->label;
          HandleLiveKeyPress(ctx, hwnd, key->label);
          SetCapture(hwnd);
          return 0;
        }
        const int midi = FindPianoMidiAtPoint(x, y);
        if (midi >= 0) {
          ctx->mouse_pressed_midi = midi;
          SendLiveNote(ctx, hwnd, midi, true);
          SetCapture(hwnd);
          return 0;
        }
      }
      break;
    case WM_LBUTTONUP:
      if (ctx != nullptr) {
        if (!ctx->mouse_pressed_key.empty()) {
          HandleLiveKeyRelease(ctx, hwnd, ctx->mouse_pressed_key);
          ctx->mouse_pressed_key.clear();
          ReleaseCapture();
          return 0;
        }
        if (ctx->mouse_pressed_midi >= 0) {
          SendLiveNote(ctx, hwnd, ctx->mouse_pressed_midi, false);
          ctx->mouse_pressed_midi = -1;
          ReleaseCapture();
          return 0;
        }
      }
      break;
    case WM_PAINT:
      if (ctx != nullptr) {
        DrawKeyboard(hwnd, ctx);
      } else {
        PAINTSTRUCT ps = {};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
      }
      return 0;
    case WM_CTLCOLORSTATIC: {
      HDC hdc = reinterpret_cast<HDC>(w_param);
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, RGB(225, 231, 244));
      static HBRUSH brush = CreateSolidBrush(RGB(20, 22, 28));
      return reinterpret_cast<LRESULT>(brush);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
      HDC hdc = reinterpret_cast<HDC>(w_param);
      SetTextColor(hdc, RGB(230, 236, 247));
      SetBkColor(hdc, RGB(38, 42, 51));
      static HBRUSH brush = CreateSolidBrush(RGB(38, 42, 51));
      return reinterpret_cast<LRESULT>(brush);
    }
    default:
      return DefWindowProc(hwnd, msg, w_param, l_param);
  }
  return DefWindowProc(hwnd, msg, w_param, l_param);
}

}  // namespace

int APIENTRY WinMain(HINSTANCE h_instance, HINSTANCE, LPSTR cmd_line, int show_cmd) {
  int smoke_exit_ms = 0;
  if (cmd_line != nullptr) {
    const std::string cmd = cmd_line;
    const std::string key = "--smoke-exit-ms";
    const auto pos = cmd.find(key);
    if (pos != std::string::npos) {
      const auto value_pos = cmd.find_first_of("0123456789", pos + key.size());
      if (value_pos != std::string::npos) {
        smoke_exit_ms = std::stoi(cmd.substr(value_pos));
      } else {
        smoke_exit_ms = 1000;
      }
    }
  }

  INITCOMMONCONTROLSEX icc = {sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES};
  InitCommonControlsEx(&icc);

  GuiContext context;
  context.smoke_exit_ms = smoke_exit_ms;
  context.started_at = std::chrono::steady_clock::now();

  const char* class_name = "PianoGuiWindowClass";
  WNDCLASSA wc = {};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = h_instance;
  wc.lpszClassName = class_name;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassA(&wc);

  HWND hwnd = CreateWindowA(class_name, "Piano GUI Alpha", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 780,
                            560, nullptr, nullptr, h_instance, &context);
  if (hwnd == nullptr) {
    return 1;
  }

  ShowWindow(hwnd, show_cmd);
  UpdateWindow(hwnd);

  MSG msg = {};
  while (GetMessage(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return static_cast<int>(msg.wParam);
}
