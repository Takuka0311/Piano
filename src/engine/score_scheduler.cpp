#include "piano/engine/score_scheduler.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace piano::engine {
namespace {

constexpr std::array<int, 7> kDegreeSemitone = {0, 2, 4, 5, 7, 9, 11};
constexpr int kMiddleCMidi = 60;
constexpr double kDefaultBeatMs = 1000.0;

bool IsShiftToken(char c) {
  return c == '+' || c == '-';
}

int ClampMidiKey(int midi_key) {
  if (midi_key < 0) {
    return 0;
  }
  if (midi_key > 127) {
    return 127;
  }
  return midi_key;
}

int SemitoneFromLetter(char letter) {
  switch (std::toupper(static_cast<unsigned char>(letter))) {
    case 'C':
      return 0;
    case 'D':
      return 2;
    case 'E':
      return 4;
    case 'F':
      return 5;
    case 'G':
      return 7;
    case 'A':
      return 9;
    case 'B':
      return 11;
    default:
      return 0;
  }
}

bool IsControlLetter(char c) {
  const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return up >= 'A' && up <= 'G';
}

int EventPriority(EventType type) {
  switch (type) {
    case EventType::kTempoChange:
    case EventType::kTransposeChange:
      return 0;
    case EventType::kNoteOff:
      return 1;
    case EventType::kNoteOn:
      return 2;
    default:
      return 3;
  }
}

bool TryParseNoteToken(const std::string& token, int transpose, int* midi_key) {
  if (token.empty()) {
    return false;
  }
  if (token[0] < '1' || token[0] > '7') {
    return false;
  }

  const int degree = token[0] - '1';
  int accidental = 0;
  int octave_shift = 0;
  std::size_t pos = 1;
  if (pos < token.size() && token[pos] == '#') {
    accidental = 1;
    ++pos;
  }
  while (pos < token.size() && IsShiftToken(token[pos])) {
    octave_shift += (token[pos] == '+') ? 1 : -1;
    ++pos;
  }
  if (pos != token.size()) {
    return false;
  }

  const int raw = kMiddleCMidi + transpose + kDegreeSemitone[degree] + accidental + octave_shift * 12;
  *midi_key = ClampMidiKey(raw);
  return true;
}

}  // namespace

std::vector<ScheduledEvent> ScoreScheduler::BuildEvents(const std::vector<score::ScoreCommand>& commands,
                                                        std::string* error_message) {
  std::vector<ScheduledEvent> events;
  double beat_ms = kDefaultBeatMs;
  int transpose = 0;

  for (const auto& command : commands) {
    const double at_ms = command.begin_beats * beat_ms;
    const std::string& token = command.token;
    if (token.empty()) {
      continue;
    }

    if (token == "S") {
      beat_ms = command.value > 0 ? static_cast<double>(command.value) : beat_ms;
      events.push_back({at_ms, EventType::kTempoChange, -1, static_cast<int>(beat_ms), token});
      continue;
    }

    if (token == "0") {
      continue;
    }

    if (token.size() == 1 && IsControlLetter(token[0])) {
      const int semitone = SemitoneFromLetter(token[0]);
      const bool upper = std::isupper(static_cast<unsigned char>(token[0])) != 0;
      transpose = upper ? semitone : -semitone;
      events.push_back({at_ms, EventType::kTransposeChange, -1, transpose, token});
      continue;
    }

    int midi_key = -1;
    if (!TryParseNoteToken(token, transpose, &midi_key)) {
      if (error_message != nullptr) {
        *error_message = "unsupported token in score: " + token;
      }
      return {};
    }

    const int velocity = std::clamp(command.value, 1, 127);
    events.push_back({at_ms, EventType::kNoteOn, midi_key, velocity, token});
    events.push_back({at_ms + command.last_beats * beat_ms, EventType::kNoteOff, midi_key, 0, token});
  }

  std::stable_sort(events.begin(), events.end(),
                   [](const ScheduledEvent& left, const ScheduledEvent& right) {
                     if (left.at_ms != right.at_ms) {
                       return left.at_ms < right.at_ms;
                     }
                     return EventPriority(left.type) < EventPriority(right.type);
                   });

  return events;
}

}  // namespace piano::engine
