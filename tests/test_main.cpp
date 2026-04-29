#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "piano/engine/score_scheduler.h"
#include "piano/input/keyboard_map.h"
#include "piano/score/score_parser.h"

namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "[FAIL] " << message << '\n';
    std::exit(1);
  }
}

std::filesystem::path WriteTempFile(const std::string& file_name, const std::string& content) {
  const auto dir = std::filesystem::temp_directory_path() / "piano_m2_tests";
  std::filesystem::create_directories(dir);
  const auto path = dir / file_name;
  std::ofstream out(path);
  out << content;
  out.close();
  return path;
}

void TestKeyboardMapValid() {
  const auto path = WriteTempFile("valid.keyboard", "# comment\nQ 60\nW 62\n");
  piano::input::KeyboardMap map;
  std::string error;
  Require(map.LoadFromFile(path.string(), &error), "keyboard map should load");
  Require(map.Size() == 2, "keyboard map should contain two bindings");
  Require(map.LookupMidiKey("Q").value_or(-1) == 60, "Q should map to midi 60");
  Require(!map.LookupMidiKey("X").has_value(), "X should not map");
}

void TestKeyboardMapInvalidRange() {
  const auto path = WriteTempFile("invalid.keyboard", "Q 180\n");
  piano::input::KeyboardMap map;
  std::string error;
  Require(!map.LoadFromFile(path.string(), &error), "out-of-range midi key should fail");
}

void TestScoreParserSortsByBegin() {
  const auto path = WriteTempFile("sort.in", "1.0 2 1 80\n0.0 1 1 80\n");
  piano::score::ScoreParser parser;
  std::string error;
  Require(parser.LoadFromFile(path.string(), &error), "score should parse");
  const auto& commands = parser.Commands();
  Require(commands.size() == 2, "score should contain two commands");
  Require(commands[0].begin_beats == 0.0, "commands should be sorted by begin time");
}

void TestSchedulerRegression() {
  piano::score::ScoreParser parser;
  std::string error;
  const std::vector<std::string> candidates = {
      "assets/regression/ordered.in",
      "../assets/regression/ordered.in",
  };
  bool loaded = false;
  for (const auto& candidate : candidates) {
    if (parser.LoadFromFile(candidate, &error)) {
      loaded = true;
      break;
    }
  }
  Require(loaded, "regression score should parse");

  piano::engine::ScoreScheduler scheduler;
  error.clear();
  const auto events = scheduler.BuildEvents(parser.Commands(), &error);
  Require(error.empty(), "scheduler should not report error");
  Require(events.size() == 9, "regression score should produce 9 events");
  Require(events[0].type == piano::engine::EventType::kTransposeChange, "first event transpose change");
  Require(events[1].type == piano::engine::EventType::kNoteOn, "second event note on");
  Require(events[3].type == piano::engine::EventType::kTempoChange, "tempo change should happen at 1000ms");
  Require(static_cast<int>(events[3].at_ms) == 1000, "tempo change should be at 1000ms");
  Require(events.back().type == piano::engine::EventType::kNoteOff, "last event should be note off");
}

void TestSchedulerNoteOffOrdering() {
  std::vector<piano::score::ScoreCommand> commands = {
      {0.0, "1", 1.0, 80},
      {1.0, "2", 0.5, 80},
  };

  piano::engine::ScoreScheduler scheduler;
  std::string error;
  const auto events = scheduler.BuildEvents(commands, &error);
  Require(error.empty(), "scheduler ordering case should not error");
  Require(events.size() == 4, "two notes should create four events");
  Require(static_cast<int>(events[1].at_ms) == 1000, "second event should be at 1000ms");
  Require(events[1].type == piano::engine::EventType::kNoteOff, "note off should precede note on at same timestamp");
  Require(events[2].type == piano::engine::EventType::kNoteOn, "second note on should be third event");
}

void TestSchedulerUnsupportedToken() {
  std::vector<piano::score::ScoreCommand> commands = {
      {0.0, "X1", 1.0, 80},
  };
  piano::engine::ScoreScheduler scheduler;
  std::string error;
  const auto events = scheduler.BuildEvents(commands, &error);
  Require(events.empty(), "unsupported token should not emit events");
  Require(!error.empty(), "unsupported token should provide error");
}

}  // namespace

int main() {
  TestKeyboardMapValid();
  TestKeyboardMapInvalidRange();
  TestScoreParserSortsByBegin();
  TestSchedulerRegression();
  TestSchedulerNoteOffOrdering();
  TestSchedulerUnsupportedToken();
  std::cout << "[PASS] piano_tests\n";
  return 0;
}
