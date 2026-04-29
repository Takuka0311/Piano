#pragma once

#include <string>
#include <vector>

#include "piano/score/score_parser.h"

namespace piano::engine {

enum class EventType {
  kNoteOn,
  kNoteOff,
  kTempoChange,
  kTransposeChange,
};

struct ScheduledEvent {
  double at_ms = 0.0;
  EventType type = EventType::kNoteOn;
  int midi_key = -1;
  int value = 0;
  std::string source_token;
};

class ScoreScheduler {
 public:
  std::vector<ScheduledEvent> BuildEvents(const std::vector<score::ScoreCommand>& commands,
                                          std::string* error_message);
};

}  // namespace piano::engine
