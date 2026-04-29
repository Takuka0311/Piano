#include "piano/app/playback_service.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <algorithm>
#include <cctype>

#include "piano/audio/dsound_output_sink.h"
#include "piano/audio/log_output_sink.h"
#include "piano/audio/midi_output_sink.h"
#include "piano/audio/output_sink.h"
#include "piano/audio/vsti_output_sink.h"
#include "piano/audio/wasapi_output_sink.h"
#include "piano/input/keyboard_map.h"
#include "piano/score/score_parser.h"
#include "piano/engine/score_scheduler.h"

namespace piano::app {

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
    SetError(AppErrorCode::kScheduleFailed, error_message != nullptr ? *error_message : "failed to prepare events");
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
    active_notes_.clear();
  }

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
  return {state_, message_, active_notes_};
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
  std::string error;
  for (const auto& backend : preferred_backends) {
    if (backend == "log") {
      *sink = std::make_unique<audio::LogOutputSink>(std::cout);
    } else if (backend == "wasapi") {
      *sink = std::make_unique<audio::WasapiOutputSink>(options.sample_rate, options.buffer_ms);
    } else if (backend == "dsound") {
      *sink = std::make_unique<audio::DsoundOutputSink>(options.sample_rate, options.buffer_ms);
    } else if (backend == "midiout") {
      *sink = std::make_unique<audio::MidiOutputSink>(options.midi_out_device);
    } else if (backend == "vsti") {
      *sink = std::make_unique<audio::VstiOutputSink>(options.vsti_plugin_path, options.midi_out_device);
    } else {
      continue;
    }

    if ((*sink)->Start(&error)) {
      if (active_backend != nullptr) {
        *active_backend = backend;
      }
      return true;
    }
  }

  if (error_message != nullptr) {
    *error_message = FormatError(AppErrorCode::kAudioStartFailed, error);
  }
  return false;
}

std::vector<std::string> PlaybackService::ResolveBackendOrder(const AppOptions& options) const {
  const auto configured = ParseBackendPriority(options.backend_priority);
  std::vector<std::string> candidates;
  auto append_unique = [&candidates](const std::string& backend) {
    if (std::find(candidates.begin(), candidates.end(), backend) == candidates.end()) {
      candidates.push_back(backend);
    }
  };

  std::string preferred = options.output_mode;
  if (preferred.empty()) {
    preferred = options.audio_backend;
  }
  if (preferred == "vsti") {
    append_unique("vsti");
    append_unique("midiout");
    append_unique("wasapi");
    append_unique("dsound");
  } else if (preferred == "midiout") {
    append_unique("midiout");
    append_unique("wasapi");
    append_unique("dsound");
  } else if (preferred == "wasapi") {
    append_unique("wasapi");
    append_unique("dsound");
  } else if (preferred == "dsound") {
    append_unique("dsound");
  } else if (preferred == "log") {
    append_unique("log");
  } else {
    append_unique("wasapi");
    append_unique("dsound");
  }

  for (const auto& item : configured) {
    append_unique(item);
  }
  append_unique("log");
  return candidates;
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
      }
    }

    sink->Emit(event);
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      if (event.type == engine::EventType::kNoteOn && event.midi_key >= 0) {
        active_notes_.insert(event.midi_key);
      } else if (event.type == engine::EventType::kNoteOff && event.midi_key >= 0) {
        active_notes_.erase(event.midi_key);
      }
    }
  }

  sink->Stop();
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
}

void PlaybackService::SetError(AppErrorCode code, const std::string& message) {
  SetError(FormatError(code, message));
}

std::vector<std::string> PlaybackService::ParseBackendPriority(const std::string& raw) {
  std::vector<std::string> out;
  std::stringstream ss(raw);
  std::string item;
  while (std::getline(ss, item, ',')) {
    item.erase(std::remove_if(item.begin(), item.end(), [](unsigned char c) { return std::isspace(c); }), item.end());
    if (!item.empty()) {
      out.push_back(item);
    }
  }
  if (out.empty()) {
    out = {"vsti", "midiout", "wasapi", "dsound", "log"};
  }
  return out;
}

}  // namespace piano::app
