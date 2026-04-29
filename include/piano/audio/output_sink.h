#pragma once

#include "piano/engine/score_scheduler.h"

namespace piano::audio {

class OutputSink {
 public:
  virtual ~OutputSink() = default;
  virtual void Emit(const engine::ScheduledEvent& event) = 0;
};

}  // namespace piano::audio
