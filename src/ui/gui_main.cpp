#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "piano/audio/dsound_output_sink.h"
#include "piano/audio/log_output_sink.h"
#include "piano/audio/midi_output_sink.h"
#include "piano/audio/output_sink.h"
#include "piano/audio/vsti_output_sink.h"
#include "piano/audio/wasapi_output_sink.h"
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
  kListActive = 2014,
  kLabelState = 2015,
  kLabelStatus = 2016,
};

constexpr int kKeyboardLeft = 16;
constexpr int kKeyboardTop = 270;
constexpr int kKeyboardWidth = 614;
constexpr int kKeyboardHeight = 110;

struct GuiContext {
  piano::ui::GuiApp app;
  piano::app::PlaybackSnapshot last_snapshot;
  piano::input::KeyboardMap live_keyboard_map;
  std::unordered_map<int, int> vk_to_midi;
  std::unordered_map<std::string, int> key_to_midi;
  std::set<std::string> active_key_names;
  std::string mouse_pressed_key;
  std::unique_ptr<piano::audio::OutputSink> live_sink;
  std::string live_sink_backend;
  int smoke_exit_ms = 0;
  std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();
};

struct VisualKey {
  std::string label;
  RECT rect;
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

  int y = kKeyboardTop;
  for (std::size_t row_idx = 0; row_idx < rows.size(); ++row_idx) {
    int x = kKeyboardLeft;
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
  const std::string backend = config.output_mode.empty() ? "wasapi" : config.output_mode;
  if (ctx->live_sink != nullptr && ctx->live_sink_backend == backend) {
    std::string error;
    if (ctx->live_sink->IsHealthy(&error)) {
      return true;
    }
    ctx->live_sink->Stop();
    ctx->live_sink.reset();
  }

  std::unique_ptr<piano::audio::OutputSink> sink;
  if (backend == "vsti") {
    sink = std::make_unique<piano::audio::VstiOutputSink>(config.vsti_plugin_path, config.midi_out_device);
  } else if (backend == "midiout") {
    sink = std::make_unique<piano::audio::MidiOutputSink>(config.midi_out_device);
  } else if (backend == "dsound") {
    sink = std::make_unique<piano::audio::DsoundOutputSink>(config.sample_rate, config.buffer_ms);
  } else if (backend == "log") {
    sink = std::make_unique<piano::audio::LogOutputSink>(std::cout);
  } else {
    sink = std::make_unique<piano::audio::WasapiOutputSink>(config.sample_rate, config.buffer_ms);
  }

  std::string error;
  if (!sink->Start(&error)) {
    if (error_message != nullptr) {
      *error_message = "live key sink start failed: " + error;
    }
    return false;
  }
  ctx->live_sink = std::move(sink);
  ctx->live_sink_backend = backend;
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
  RECT keyboard_rect = {kKeyboardLeft, kKeyboardTop, kKeyboardLeft + kKeyboardWidth, kKeyboardTop + kKeyboardHeight};
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
  RECT keyboard_rect = {kKeyboardLeft, kKeyboardTop, kKeyboardLeft + kKeyboardWidth, kKeyboardTop + kKeyboardHeight};
  InvalidateRect(hwnd, &keyboard_rect, FALSE);
}

void DrawKeyboard(HWND hwnd, const GuiContext* ctx) {
  PAINTSTRUCT ps = {};
  HDC hdc = BeginPaint(hwnd, &ps);
  RECT bg = {kKeyboardLeft, kKeyboardTop, kKeyboardLeft + kKeyboardWidth, kKeyboardTop + kKeyboardHeight};
  FillRect(hdc, &bg, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

  const auto keys = BuildVisualKeyboard();
  for (const auto& key : keys) {
    const bool active = ctx != nullptr && ctx->active_key_names.count(key.label) > 0;
    const bool mapped = ctx != nullptr && ctx->key_to_midi.count(key.label) > 0;
    const COLORREF color = active ? RGB(120, 190, 255) : (mapped ? RGB(245, 245, 245) : RGB(220, 220, 220));
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &key.rect, brush);
    DeleteObject(brush);
    Rectangle(hdc, key.rect.left, key.rect.top, key.rect.right, key.rect.bottom);
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, key.rect.left + 10, key.rect.top + 8, key.label.c_str(), static_cast<int>(key.label.size()));
  }
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

void SetText(HWND hwnd, int control_id, const std::string& value) {
  SetWindowTextA(GetDlgItem(hwnd, control_id), value.c_str());
}

std::string GetText(HWND hwnd, int control_id) {
  char buffer[512] = {};
  GetWindowTextA(GetDlgItem(hwnd, control_id), buffer, sizeof(buffer));
  return std::string(buffer);
}

void ApplyConfigToControls(HWND hwnd, const piano::platform::UiConfig& config) {
  SetText(hwnd, kEditKeyboard, config.keyboard_path);
  SetText(hwnd, kEditScore, config.score_path);
  SetText(hwnd, kEditSampleRate, std::to_string(config.sample_rate));
  SetText(hwnd, kEditBufferMs, std::to_string(config.buffer_ms));
  SetText(hwnd, kEditMidiDevice, config.midi_out_device);
  SetText(hwnd, kEditVstiPlugin, config.vsti_plugin_path);
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
  config.sample_rate = (std::max)(1, std::stoi(GetText(hwnd, kEditSampleRate)));
  config.buffer_ms = (std::max)(1, std::stoi(GetText(hwnd, kEditBufferMs)));
  config.midi_out_device = GetText(hwnd, kEditMidiDevice);
  config.vsti_plugin_path = GetText(hwnd, kEditVstiPlugin);
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

  HWND list = GetDlgItem(hwnd, kListActive);
  SendMessageA(list, LB_RESETCONTENT, 0, 0);
  for (int midi = 48; midi <= 84; ++midi) {
    const bool active = snapshot.active_notes.count(midi) > 0;
    std::string line = std::to_string(midi) + (active ? " [ON]" : " [off]");
    SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
  }
}

void CreateControls(HWND hwnd) {
  CreateWindowA("STATIC", "Keyboard:", WS_CHILD | WS_VISIBLE, 16, 16, 80, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 100, 14, 430, 24, hwnd,
                reinterpret_cast<HMENU>(kEditKeyboard), nullptr, nullptr);
  CreateWindowA("BUTTON", "Browse", WS_CHILD | WS_VISIBLE, 540, 14, 90, 24, hwnd,
                reinterpret_cast<HMENU>(kBtnBrowseKeyboard), nullptr, nullptr);

  CreateWindowA("STATIC", "Score:", WS_CHILD | WS_VISIBLE, 16, 48, 80, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 100, 46, 430, 24, hwnd,
                reinterpret_cast<HMENU>(kEditScore), nullptr, nullptr);
  CreateWindowA("BUTTON", "Browse", WS_CHILD | WS_VISIBLE, 540, 46, 90, 24, hwnd,
                reinterpret_cast<HMENU>(kBtnBrowseScore), nullptr, nullptr);

  CreateWindowA("STATIC", "Output:", WS_CHILD | WS_VISIBLE, 16, 82, 80, 20, hwnd, nullptr, nullptr, nullptr);
  HWND combo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 100, 80, 120, 200, hwnd,
                             reinterpret_cast<HMENU>(kComboBackend), nullptr, nullptr);
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("wasapi"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("midiout"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("dsound"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("log"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("vsti"));

  CreateWindowA("STATIC", "Sample Rate:", WS_CHILD | WS_VISIBLE, 250, 82, 90, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 345, 80, 90, 24, hwnd,
                reinterpret_cast<HMENU>(kEditSampleRate), nullptr, nullptr);
  CreateWindowA("STATIC", "Buffer(ms):", WS_CHILD | WS_VISIBLE, 445, 82, 80, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 530, 80, 100, 24, hwnd,
                reinterpret_cast<HMENU>(kEditBufferMs), nullptr, nullptr);

  CreateWindowA("STATIC", "MIDI Device:", WS_CHILD | WS_VISIBLE, 16, 114, 90, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 100, 112, 530, 24, hwnd,
                reinterpret_cast<HMENU>(kEditMidiDevice), nullptr, nullptr);

  CreateWindowA("STATIC", "VSTi DLL:", WS_CHILD | WS_VISIBLE, 16, 146, 90, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 100, 144, 430, 24, hwnd,
                reinterpret_cast<HMENU>(kEditVstiPlugin), nullptr, nullptr);
  CreateWindowA("BUTTON", "Browse", WS_CHILD | WS_VISIBLE, 540, 144, 90, 24, hwnd,
                reinterpret_cast<HMENU>(kBtnBrowseVsti), nullptr, nullptr);

  CreateWindowA("BUTTON", "Play", WS_CHILD | WS_VISIBLE, 16, 176, 90, 26, hwnd, reinterpret_cast<HMENU>(kBtnPlay),
                nullptr, nullptr);
  CreateWindowA("BUTTON", "Stop", WS_CHILD | WS_VISIBLE, 114, 176, 90, 26, hwnd, reinterpret_cast<HMENU>(kBtnStop),
                nullptr, nullptr);
  CreateWindowA("BUTTON", "Save Config", WS_CHILD | WS_VISIBLE, 212, 176, 110, 26, hwnd,
                reinterpret_cast<HMENU>(kBtnSave), nullptr, nullptr);

  CreateWindowA("STATIC", "state: idle", WS_CHILD | WS_VISIBLE, 16, 214, 300, 20, hwnd,
                reinterpret_cast<HMENU>(kLabelState), nullptr, nullptr);
  CreateWindowA("STATIC", "status: ready", WS_CHILD | WS_VISIBLE, 16, 238, 620, 20, hwnd,
                reinterpret_cast<HMENU>(kLabelStatus), nullptr, nullptr);

  CreateWindowA("STATIC", "Computer Keyboard Layout (live):", WS_CHILD | WS_VISIBLE, 16, 248, 260, 18, hwnd, nullptr, nullptr,
                nullptr);
  CreateWindowA("LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY, 16, 390, 614, 140, hwnd,
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
      if (id == kBtnBrowseKeyboard) {
        const auto path = PickFile(hwnd, "Keyboard (*.keyboard)\0*.keyboard\0All Files (*.*)\0*.*\0");
        if (!path.empty()) {
          SetText(hwnd, kEditKeyboard, path);
          RefreshLiveKeyboardMap(ctx, hwnd);
        }
      } else if (id == kBtnBrowseScore) {
        const auto path = PickFile(hwnd, "Score (*.in)\0*.in\0All Files (*.*)\0*.*\0");
        if (!path.empty()) SetText(hwnd, kEditScore, path);
      } else if (id == kBtnBrowseVsti) {
        const auto path = PickFile(hwnd, "VST2 Plugin (*.dll)\0*.dll\0All Files (*.*)\0*.*\0");
        if (!path.empty()) SetText(hwnd, kEditVstiPlugin, path);
      } else if (id == kBtnPlay) {
        auto config = ReadConfigFromControls(hwnd);
        ctx->app.SetConfig(config);
        RefreshLiveKeyboardMap(ctx, hwnd);
        std::string error;
        if (!ctx->app.StartPlayback(&error)) {
          MessageBoxA(hwnd, error.c_str(), "Playback Error", MB_OK | MB_ICONERROR);
        }
      } else if (id == kBtnStop) {
        ctx->app.StopPlayback();
      } else if (id == kBtnSave) {
        auto config = ReadConfigFromControls(hwnd);
        ctx->app.SetConfig(config);
        RefreshLiveKeyboardMap(ctx, hwnd);
        std::string error;
        if (!ctx->app.SaveConfig(&error)) {
          MessageBoxA(hwnd, error.c_str(), "Save Error", MB_OK | MB_ICONERROR);
        }
      }
      return 0;
    }
    case WM_TIMER: {
      if (ctx == nullptr) return 0;
      ctx->last_snapshot = ctx->app.Snapshot();
      UpdateSnapshotUI(hwnd, ctx->last_snapshot);
      RECT keyboard_rect = {kKeyboardLeft, kKeyboardTop, kKeyboardLeft + kKeyboardWidth, kKeyboardTop + kKeyboardHeight};
      InvalidateRect(hwnd, &keyboard_rect, FALSE);
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
      }
      break;
    case WM_LBUTTONUP:
      if (ctx != nullptr && !ctx->mouse_pressed_key.empty()) {
        HandleLiveKeyRelease(ctx, hwnd, ctx->mouse_pressed_key);
        ctx->mouse_pressed_key.clear();
        ReleaseCapture();
        return 0;
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

  HWND hwnd = CreateWindowA(class_name, "Piano GUI Alpha", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 680,
                            600, nullptr, nullptr, h_instance, &context);
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
