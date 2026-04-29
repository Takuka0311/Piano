#include "piano/app/playback_service.h"

#include <chrono>
#include <iostream>
#include <thread>

#include "piano/audio/log_output_sink.h"
#include "piano/audio/output_sink.h"
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
      *error_message = "playback already running";
    }
    return false;
  }

  std::vector<engine::ScheduledEvent> events;
  if (!PrepareEvents(options, &events, error_message)) {
    SetError(error_message != nullptr ? *error_message : "failed to prepare events");
    return false;
  }

  std::unique_ptr<audio::OutputSink> sink;
  if (!BuildSink(options, &sink, error_message)) {
    SetError(error_message != nullptr ? *error_message : "failed to build output sink");
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

  worker_ = std::thread(&PlaybackService::RunPlayback, this, std::move(events), options, std::move(sink));
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
      *error_message = "Keyboard map load failed: " + error;
    }
    return false;
  }

  score::ScoreParser parser;
  if (!parser.LoadFromFile(options.score_path, &error)) {
    if (error_message != nullptr) {
      *error_message = "Score load failed: " + error;
    }
    return false;
  }

  engine::ScoreScheduler scheduler;
  *events = scheduler.BuildEvents(parser.Commands(), &error);
  if (!error.empty()) {
    if (error_message != nullptr) {
      *error_message = "Schedule failed: " + error;
    }
    return false;
  }
  return true;
}

bool PlaybackService::BuildSink(const AppOptions& options,
                                std::unique_ptr<audio::OutputSink>* sink,
                                std::string* error_message) {
  if (options.audio_backend == "log") {
    *sink = std::make_unique<audio::LogOutputSink>(std::cout);
  } else {
    *sink = std::make_unique<audio::WasapiOutputSink>(options.sample_rate, options.buffer_ms);
  }

  std::string error;
  if ((*sink)->Start(&error)) {
    return true;
  }

  if (options.audio_backend != "log") {
    *sink = std::make_unique<audio::LogOutputSink>(std::cout);
    if ((*sink)->Start(&error)) {
      return true;
    }
  }

  if (error_message != nullptr) {
    *error_message = error;
  }
  return false;
}

void PlaybackService::RunPlayback(std::vector<engine::ScheduledEvent> events,
                                  AppOptions options,
                                  std::unique_ptr<audio::OutputSink> sink) {
  (void)options;
  std::string error;
  const auto start = std::chrono::steady_clock::now();
  for (const auto& event : events) {
    if (stop_requested_.load()) {
      break;
    }

    const auto target = start + std::chrono::milliseconds(static_cast<int>(event.at_ms));
    std::this_thread::sleep_until(target);

    if (!sink->IsHealthy(&error)) {
      SetError("Audio backend runtime unhealthy: " + error);
      sink->Stop();
      sink = std::make_unique<audio::LogOutputSink>(std::cout);
      if (!sink->Start(&error)) {
        SetError("Log backend start failed: " + error);
        break;
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

}  // namespace piano::app
