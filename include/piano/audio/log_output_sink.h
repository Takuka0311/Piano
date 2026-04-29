#pragma once

#include <ostream>

#include "piano/audio/output_sink.h"

namespace piano::audio {

class LogOutputSink : public OutputSink {
 public:
  explicit LogOutputSink(std::ostream& stream);
  void Emit(const engine::ScheduledEvent& event) override;

 private:
  std::ostream& stream_;
};

}  // namespace piano::audio
