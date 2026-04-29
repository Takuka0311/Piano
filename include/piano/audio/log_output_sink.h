#pragma once

#include <ostream>

#include "piano/audio/output_sink.h"

namespace piano::audio {

class LogOutputSink : public OutputSink {
 public:
  explicit LogOutputSink(std::ostream& stream);
  bool Start(std::string* error_message) override;
  void Stop() override;
  void Emit(const engine::ScheduledEvent& event) override;
  bool IsHealthy(std::string* error_message) const override;

 private:
  std::ostream& stream_;
};

}  // namespace piano::audio
