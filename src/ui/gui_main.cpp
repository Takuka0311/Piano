#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <chrono>
#include <set>
#include <string>

#include "piano/ui/gui_app.h"

namespace {

enum : int {
  kTimerId = 1001,
  kEditKeyboard = 2001,
  kEditScore = 2002,
  kComboBackend = 2003,
  kEditSampleRate = 2004,
  kEditBufferMs = 2005,
  kBtnPlay = 2006,
  kBtnStop = 2007,
  kBtnSave = 2008,
  kBtnBrowseKeyboard = 2009,
  kBtnBrowseScore = 2010,
  kListActive = 2011,
  kLabelState = 2012,
  kLabelStatus = 2013,
};

struct GuiContext {
  piano::ui::GuiApp app;
  int smoke_exit_ms = 0;
  std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();
};

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
  int idx = 0;
  if (config.audio_backend == "dsound") idx = 1;
  if (config.audio_backend == "log") idx = 2;
  SendMessageA(GetDlgItem(hwnd, kComboBackend), CB_SETCURSEL, idx, 0);
}

piano::platform::UiConfig ReadConfigFromControls(HWND hwnd) {
  piano::platform::UiConfig config;
  config.keyboard_path = GetText(hwnd, kEditKeyboard);
  config.score_path = GetText(hwnd, kEditScore);
  config.sample_rate = (std::max)(1, std::stoi(GetText(hwnd, kEditSampleRate)));
  config.buffer_ms = (std::max)(1, std::stoi(GetText(hwnd, kEditBufferMs)));
  const LRESULT backend_idx = SendMessageA(GetDlgItem(hwnd, kComboBackend), CB_GETCURSEL, 0, 0);
  if (backend_idx == 1) {
    config.audio_backend = "dsound";
  } else if (backend_idx == 2) {
    config.audio_backend = "log";
  } else {
    config.audio_backend = "wasapi";
  }
  config.backend_priority = "wasapi,dsound,log";
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

  CreateWindowA("STATIC", "Backend:", WS_CHILD | WS_VISIBLE, 16, 82, 80, 20, hwnd, nullptr, nullptr, nullptr);
  HWND combo = CreateWindowA("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 100, 80, 120, 200, hwnd,
                             reinterpret_cast<HMENU>(kComboBackend), nullptr, nullptr);
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("wasapi"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("dsound"));
  SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("log"));

  CreateWindowA("STATIC", "Sample Rate:", WS_CHILD | WS_VISIBLE, 250, 82, 90, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 345, 80, 90, 24, hwnd,
                reinterpret_cast<HMENU>(kEditSampleRate), nullptr, nullptr);
  CreateWindowA("STATIC", "Buffer(ms):", WS_CHILD | WS_VISIBLE, 445, 82, 80, 20, hwnd, nullptr, nullptr, nullptr);
  CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 530, 80, 100, 24, hwnd,
                reinterpret_cast<HMENU>(kEditBufferMs), nullptr, nullptr);

  CreateWindowA("BUTTON", "Play", WS_CHILD | WS_VISIBLE, 16, 116, 90, 26, hwnd, reinterpret_cast<HMENU>(kBtnPlay),
                nullptr, nullptr);
  CreateWindowA("BUTTON", "Stop", WS_CHILD | WS_VISIBLE, 114, 116, 90, 26, hwnd, reinterpret_cast<HMENU>(kBtnStop),
                nullptr, nullptr);
  CreateWindowA("BUTTON", "Save Config", WS_CHILD | WS_VISIBLE, 212, 116, 110, 26, hwnd,
                reinterpret_cast<HMENU>(kBtnSave), nullptr, nullptr);

  CreateWindowA("STATIC", "state: idle", WS_CHILD | WS_VISIBLE, 16, 154, 300, 20, hwnd,
                reinterpret_cast<HMENU>(kLabelState), nullptr, nullptr);
  CreateWindowA("STATIC", "status: ready", WS_CHILD | WS_VISIBLE, 16, 178, 620, 20, hwnd,
                reinterpret_cast<HMENU>(kLabelStatus), nullptr, nullptr);

  CreateWindowA("LISTBOX", "", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY, 16, 210, 614, 320, hwnd,
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
      SetTimer(hwnd, kTimerId, 100, nullptr);
      return 0;
    }
    case WM_COMMAND: {
      if (ctx == nullptr) return 0;
      const int id = LOWORD(w_param);
      if (id == kBtnBrowseKeyboard) {
        const auto path = PickFile(hwnd, "Keyboard (*.keyboard)\0*.keyboard\0All Files (*.*)\0*.*\0");
        if (!path.empty()) SetText(hwnd, kEditKeyboard, path);
      } else if (id == kBtnBrowseScore) {
        const auto path = PickFile(hwnd, "Score (*.in)\0*.in\0All Files (*.*)\0*.*\0");
        if (!path.empty()) SetText(hwnd, kEditScore, path);
      } else if (id == kBtnPlay) {
        auto config = ReadConfigFromControls(hwnd);
        ctx->app.SetConfig(config);
        std::string error;
        if (!ctx->app.StartPlayback(&error)) {
          MessageBoxA(hwnd, error.c_str(), "Playback Error", MB_OK | MB_ICONERROR);
        }
      } else if (id == kBtnStop) {
        ctx->app.StopPlayback();
      } else if (id == kBtnSave) {
        auto config = ReadConfigFromControls(hwnd);
        ctx->app.SetConfig(config);
        std::string error;
        if (!ctx->app.SaveConfig(&error)) {
          MessageBoxA(hwnd, error.c_str(), "Save Error", MB_OK | MB_ICONERROR);
        }
      }
      return 0;
    }
    case WM_TIMER: {
      if (ctx == nullptr) return 0;
      UpdateSnapshotUI(hwnd, ctx->app.Snapshot());
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
      }
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProc(hwnd, msg, w_param, l_param);
  }
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
