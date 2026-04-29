#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "piano/app/application.h"
#include "piano/app/error_codes.h"
#include "piano/engine/score_scheduler.h"

namespace piano::audio {
class OutputSink;
}

namespace piano::app {

enum class PlaybackState {
  kIdle,
  kPlaying,
  kError,
};

struct PlaybackSnapshot {
  PlaybackState state = PlaybackState::kIdle;
  std::string message;
  std::set<int> active_notes;
};

class PlaybackService {
 public:
  PlaybackService();
  ~PlaybackService();

  bool Start(const AppOptions& options, std::string* error_message);
  void Stop();
  void Wait();
  PlaybackSnapshot Snapshot() const;

 private:
  bool PrepareEvents(const AppOptions& options,
                     std::vector<engine::ScheduledEvent>* events,
                     std::string* error_message);
  bool BuildSink(const AppOptions& options,
                 const std::vector<std::string>& preferred_backends,
                 std::unique_ptr<audio::OutputSink>* sink,
                 std::string* active_backend,
                 std::string* error_message);
  std::vector<std::string> ResolveBackendOrder(const AppOptions& options) const;
  bool TryRecoverAudio(const AppOptions& options,
                       std::unique_ptr<audio::OutputSink>* sink,
                       std::string* active_backend,
                       std::string* error_message);
  void RunPlayback(std::vector<engine::ScheduledEvent> events,
                   AppOptions options,
                   std::unique_ptr<audio::OutputSink> sink);
  void SetError(const std::string& message);
  void SetError(AppErrorCode code, const std::string& message);
  static std::vector<std::string> ParseBackendPriority(const std::string& raw);

  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  std::thread worker_;

  mutable std::mutex state_mutex_;
  PlaybackState state_ = PlaybackState::kIdle;
  std::string message_;
  std::set<int> active_notes_;
};

}  // namespace piano::app
