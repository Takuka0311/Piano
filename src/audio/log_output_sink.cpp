#include "piano/audio/log_output_sink.h"

#include <iomanip>

namespace piano::audio {
namespace {

const char* EventTypeToString(engine::EventType type) {
  switch (type) {
    case engine::EventType::kNoteOn:
      return "NoteOn";
    case engine::EventType::kNoteOff:
      return "NoteOff";
    case engine::EventType::kTempoChange:
      return "TempoChange";
    case engine::EventType::kTransposeChange:
      return "TransposeChange";
    default:
      return "Unknown";
  }
}

}  // namespace

LogOutputSink::LogOutputSink(std::ostream& stream) : stream_(stream) {}

void LogOutputSink::Emit(const engine::ScheduledEvent& event) {
  stream_ << "[t=" << std::fixed << std::setprecision(1) << event.at_ms << "ms]"
          << " type=" << EventTypeToString(event.type)
          << " token=" << event.source_token;
  if (event.midi_key >= 0) {
    stream_ << " midi=" << event.midi_key;
  }
  stream_ << " value=" << event.value << '\n';
}

}  // namespace piano::audio
