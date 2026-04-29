#pragma once

#include <string>

#include "piano/engine/score_scheduler.h"

namespace piano::audio {

class OutputSink {
 public:
  virtual ~OutputSink() = default;
  virtual bool Start(std::string* error_message) = 0;
  virtual void Stop() = 0;
  virtual void Emit(const engine::ScheduledEvent& event) = 0;
  virtual bool IsHealthy(std::string* error_message) const = 0;
};

}  // namespace piano::audio
