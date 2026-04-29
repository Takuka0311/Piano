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

bool IsControlLetter(char c) {
  const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return up >= 'A' && up <= 'G';
}

bool TryParseLegacyScaleToken(const std::string& token, int* new_transpose) {
  if (new_transpose == nullptr || token.empty() || !IsControlLetter(token[0])) {
    return false;
  }
  if (token.size() > 2) {
    return false;
  }
  if (token.size() == 2 && token[1] != '+' && token[1] != '-') {
    return false;
  }

  const bool minus_tail = token.size() == 2 && token[1] == '-';
  const bool plus_tail = token.size() == 2 && token[1] == '+';
  switch (token[0]) {
    case 'A':
      *new_transpose = -3 - (minus_tail ? 1 : 0);
      return true;
    case 'B':
      *new_transpose = -1 - (minus_tail ? 1 : 0);
      return true;
    case 'C':
      *new_transpose = 0;
      return true;
    case 'D':
      *new_transpose = 2 - (minus_tail ? 1 : 0);
      return true;
    case 'E':
      *new_transpose = 4 - (minus_tail ? 1 : 0);
      return true;
    case 'F':
      *new_transpose = 5;
      return true;
    case 'G':
      *new_transpose = -5 - (minus_tail ? 1 : 0);
      return true;
    case 'a':
      *new_transpose = 0;
      return true;
    case 'b':
      *new_transpose = 2 - (minus_tail ? 1 : 0);
      return true;
    case 'c':
      *new_transpose = 3 + (plus_tail ? 1 : 0);
      return true;
    case 'd':
      *new_transpose = 5;
      return true;
    case 'e':
      *new_transpose = -5 - (minus_tail ? 1 : 0);
      return true;
    case 'f':
      *new_transpose = -4 + (plus_tail ? 1 : 0);
      return true;
    case 'g':
      *new_transpose = -2 + (plus_tail ? 1 : 0);
      return true;
    default:
      return false;
  }
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

    // Legacy compatibility: any token that starts with '0' is treated as rest.
    if (!token.empty() && token[0] == '0') {
      continue;
    }

    int legacy_transpose = 0;
    if (TryParseLegacyScaleToken(token, &legacy_transpose)) {
      transpose = legacy_transpose;
      events.push_back({at_ms, EventType::kTransposeChange, -1, transpose, token});
      continue;
    }

    int midi_key = -1;
    if (!TryParseNoteToken(token, transpose, &midi_key)) {
      if (error_message != nullptr) {
        std::string line_hint;
        if (command.source_line > 0) {
          line_hint = " at line " + std::to_string(command.source_line);
        }
        std::string source_hint;
        if (!command.source_text.empty()) {
          source_hint = " | " + command.source_text;
        }
        *error_message = "unsupported token in score" + line_hint + ": " + token + source_hint;
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
