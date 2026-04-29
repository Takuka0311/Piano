#include "piano/app/playback_service.h"

#include <chrono>
#include <iostream>
#include <thread>

#include "piano/app/audio_backend_factory.h"
#include "piano/audio/output_sink.h"
#include "piano/engine/score_scheduler.h"
#include "piano/input/keyboard_map.h"
#include "piano/score/score_parser.h"

namespace piano::app {
namespace {

void LogObsEvent(const std::string& event, const std::string& detail) {
  std::cerr << "[obs] event=" << event << " detail=" << detail << '\n';
}

}  // namespace

PlaybackService::PlaybackService() = default;

PlaybackService::~PlaybackService() {
  Stop();
}

bool PlaybackService::Start(const AppOptions& options, std::string* error_message) {
  if (running_.load()) {
    if (error_message != nullptr) {
      *error_message = FormatError(AppErrorCode::kPlaybackBusy, "playback already running");
    }
    return false;
  }

  std::vector<engine::ScheduledEvent> events;
  if (!PrepareEvents(options, &events, error_message)) {
    SetError(error_message != nullptr ? *error_message : FormatError(AppErrorCode::kScheduleFailed, "failed to prepare events"));
    return false;
  }

  std::unique_ptr<audio::OutputSink> sink;
  std::string active_backend;
  const auto preferred_backends = ResolveBackendOrder(options);
  if (!BuildSink(options, preferred_backends, &sink, &active_backend, error_message)) {
    SetError(AppErrorCode::kAudioStartFailed, error_message != nullptr ? *error_message : "failed to build output sink");
    return false;
  }

  stop_requested_.store(false);
  running_.store(true);
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_ = PlaybackState::kPlaying;
    message_ = "playing";
    active_backend_ = active_backend;
    recoveries_ = 0;
    errors_ = 0;
    emitted_events_ = 0;
    active_notes_.clear();
  }
  LogObsEvent("playback_start", "backend=" + active_backend + " events=" + std::to_string(events.size()));

  AppOptions run_options = options;
  run_options.audio_backend = active_backend;
  run_options.output_mode = active_backend;
  worker_ = std::thread(&PlaybackService::RunPlayback, this, std::move(events), run_options, std::move(sink));
  return true;
}

void PlaybackService::Stop() {
  stop_requested_.store(true);
  if (worker_.joinable()) {
    worker_.join();
  }
}

void PlaybackService::Wait() {
  if (worker_.joinable()) {
    worker_.join();
  }
}

PlaybackSnapshot PlaybackService::Snapshot() const {
  std::lock_guard<std::mutex> lock(state_mutex_);
  return {state_, message_, active_backend_, recoveries_, errors_, emitted_events_, active_notes_};
}

bool PlaybackService::PrepareEvents(const AppOptions& options,
                                    std::vector<engine::ScheduledEvent>* events,
                                    std::string* error_message) {
  input::KeyboardMap keyboard_map;
  std::string error;
  if (!keyboard_map.LoadFromFile(options.keyboard_map_path, &error)) {
    if (error_message != nullptr) {
      *error_message = FormatError(AppErrorCode::kKeyboardLoadFailed, error);
    }
    return false;
  }

  score::ScoreParser parser;
  if (!parser.LoadFromFile(options.score_path, &error)) {
    if (error_message != nullptr) {
      *error_message = FormatError(AppErrorCode::kScoreLoadFailed, error);
    }
    return false;
  }

  engine::ScoreScheduler scheduler;
  *events = scheduler.BuildEvents(parser.Commands(), &error);
  if (!error.empty()) {
    if (error_message != nullptr) {
      *error_message = FormatError(AppErrorCode::kScheduleFailed, error);
    }
    return false;
  }
  return true;
}

bool PlaybackService::BuildSink(const AppOptions& options,
                                const std::vector<std::string>& preferred_backends,
                                std::unique_ptr<audio::OutputSink>* sink,
                                std::string* active_backend,
                                std::string* error_message) {
  return BuildAndStartSink(options, preferred_backends, sink, active_backend, error_message);
}

std::vector<std::string> PlaybackService::ResolveBackendOrder(const AppOptions& options) const {
  if (options.output_mode == "vsti") {
    return {"vsti"};
  }
  return app::ResolveBackendOrder(options);
}

bool PlaybackService::TryRecoverAudio(const AppOptions& options,
                                      std::unique_ptr<audio::OutputSink>* sink,
                                      std::string* active_backend,
                                      std::string* error_message) {
  const auto preferred = ResolveBackendOrder(options);
  constexpr int kRecoverAttempts = 2;
  for (int attempt = 0; attempt < kRecoverAttempts; ++attempt) {
    if (BuildSink(options, preferred, sink, active_backend, error_message)) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
  return false;
}

void PlaybackService::RunPlayback(std::vector<engine::ScheduledEvent> events,
                                  AppOptions options,
                                  std::unique_ptr<audio::OutputSink> sink) {
  std::string active_backend = options.audio_backend;
  std::string error;
  const auto start = std::chrono::steady_clock::now();
  for (const auto& event : events) {
    if (stop_requested_.load()) {
      break;
    }

    const auto target = start + std::chrono::milliseconds(static_cast<int>(event.at_ms));
    std::this_thread::sleep_until(target);

    if (!sink->IsHealthy(&error)) {
      SetError(AppErrorCode::kAudioRuntimeFailed, "backend=" + active_backend + " detail=" + error);
      sink->Stop();
      std::string recover_error;
      if (!TryRecoverAudio(options, &sink, &active_backend, &recover_error)) {
        SetError(AppErrorCode::kAudioFallbackFailed, recover_error);
        break;
      }
      {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = PlaybackState::kPlaying;
        message_ = "recovered backend=" + active_backend;
        active_backend_ = active_backend;
        ++recoveries_;
      }
      LogObsEvent("audio_recovered", "backend=" + active_backend);
    }

    sink->Emit(event);
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      ++emitted_events_;
      if (event.type == engine::EventType::kNoteOn && event.midi_key >= 0) {
        active_notes_.insert(event.midi_key);
      } else if (event.type == engine::EventType::kNoteOff && event.midi_key >= 0) {
        active_notes_.erase(event.midi_key);
      }
    }
  }

  sink->Stop();
  LogObsEvent("playback_finish", stop_requested_.load() ? "reason=stopped" : "reason=completed");
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    active_notes_.clear();
    if (state_ != PlaybackState::kError) {
      state_ = PlaybackState::kIdle;
      message_ = stop_requested_.load() ? "stopped" : "completed";
    }
  }
  running_.store(false);
}

void PlaybackService::SetError(const std::string& message) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  state_ = PlaybackState::kError;
  message_ = message;
  ++errors_;
  LogObsEvent("playback_error", message);
}

void PlaybackService::SetError(AppErrorCode code, const std::string& message) {
  SetError(FormatError(code, message));
}

}  // namespace piano::app
