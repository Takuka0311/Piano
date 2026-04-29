#pragma once

#include <string>

namespace piano::app {

enum class AppErrorCode {
  kNone,
  kPlaybackBusy,
  kKeyboardLoadFailed,
  kScoreLoadFailed,
  kScheduleFailed,
  kAudioStartFailed,
  kAudioRuntimeFailed,
  kAudioFallbackFailed,
  kInvalidArgument,
  kConfigIoFailed,
};

inline std::string FormatError(AppErrorCode code, const std::string& detail) {
  const char* prefix = "UNKNOWN";
  switch (code) {
    case AppErrorCode::kNone:
      prefix = "NONE";
      break;
    case AppErrorCode::kPlaybackBusy:
      prefix = "PLAYBACK_BUSY";
      break;
    case AppErrorCode::kKeyboardLoadFailed:
      prefix = "KEYBOARD_LOAD_FAILED";
      break;
    case AppErrorCode::kScoreLoadFailed:
      prefix = "SCORE_LOAD_FAILED";
      break;
    case AppErrorCode::kScheduleFailed:
      prefix = "SCHEDULE_FAILED";
      break;
    case AppErrorCode::kAudioStartFailed:
      prefix = "AUDIO_START_FAILED";
      break;
    case AppErrorCode::kAudioRuntimeFailed:
      prefix = "AUDIO_RUNTIME_FAILED";
      break;
    case AppErrorCode::kAudioFallbackFailed:
      prefix = "AUDIO_FALLBACK_FAILED";
      break;
    case AppErrorCode::kInvalidArgument:
      prefix = "INVALID_ARGUMENT";
      break;
    case AppErrorCode::kConfigIoFailed:
      prefix = "CONFIG_IO_FAILED";
      break;
  }
  if (detail.empty()) {
    return prefix;
  }
  return std::string(prefix) + ": " + detail;
}

}  // namespace piano::app
