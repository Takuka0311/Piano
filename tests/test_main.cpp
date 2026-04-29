#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "piano/engine/score_scheduler.h"
#include "piano/export/wav_exporter.h"
#include "piano/input/keyboard_map.h"
#include "piano/platform/config_store.h"
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

void TestScoreParserLegacyColumnOrder() {
  const auto path = WriteTempFile("legacy_order.in", "0.0 0.5 1 90\n0.5 0.5 2 90\n");
  piano::score::ScoreParser parser;
  std::string error;
  Require(parser.LoadFromFile(path.string(), &error), "legacy order score should parse");
  const auto& commands = parser.Commands();
  Require(commands.size() == 2, "legacy order should produce two commands");
  Require(commands[0].token == "1", "legacy token should be parsed from third column");
  Require(commands[0].last_beats == 0.5, "legacy duration should be parsed from second column");
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
      {0.0, "1", 1.0, 80, 0, {}},
      {1.0, "2", 0.5, 80, 0, {}},
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
      {0.0, "X1", 1.0, 80, 0, {}},
  };
  piano::engine::ScoreScheduler scheduler;
  std::string error;
  const auto events = scheduler.BuildEvents(commands, &error);
  Require(events.empty(), "unsupported token should not emit events");
  Require(!error.empty(), "unsupported token should provide error");
}

void TestSchedulerLegacyScaleToken() {
  std::vector<piano::score::ScoreCommand> commands = {
      {0.0, "b-", 0.0, 0, 0, {}},
      {0.0, "1", 1.0, 80, 0, {}},
  };
  piano::engine::ScoreScheduler scheduler;
  std::string error;
  const auto events = scheduler.BuildEvents(commands, &error);
  Require(error.empty(), "legacy scale token should be supported");
  Require(events.size() == 3, "scale change + note on/off expected");
  Require(events[0].type == piano::engine::EventType::kTransposeChange, "first event should be transpose change");
  Require(events[1].midi_key == 61, "b- scale should transpose note 1 to midi 61");
}

void TestConfigStoreRoundTripM6Fields() {
  const auto path = WriteTempFile("config.ini", "");
  piano::platform::ConfigStore store(path.string());
  piano::platform::UiConfig config;
  config.keyboard_path = "assets/default.keyboard";
  config.score_path = "assets/demo.in";
  config.output_mode = "vsti";
  config.audio_backend = "vsti";
  config.backend_priority = "vsti,midiout,wasapi,dsound,log";
  config.midi_out_device = "0";
  config.vsti_plugin_path = "mdaPiano.dll";
  config.export_wav_path = "temp/output.wav";
  config.sample_rate = 48000;
  config.buffer_ms = 40;
  config.recent_keyboard_path = "assets/default.keyboard";
  config.recent_score_path = "assets/demo.in";
  std::string error;
  Require(store.Save(config, &error), "config save should succeed");

  const auto loaded = store.Load();
  Require(loaded.output_mode == "vsti", "output_mode should round-trip");
  Require(loaded.midi_out_device == "0", "midi_out_device should round-trip");
  Require(loaded.vsti_plugin_path == "mdaPiano.dll", "vsti_plugin_path should round-trip");
  Require(loaded.export_wav_path == "temp/output.wav", "export_wav_path should round-trip");
}

void TestWavExportDeterministic() {
  const auto temp_dir = std::filesystem::temp_directory_path() / "piano_m7_export_tests";
  std::filesystem::create_directories(temp_dir);
  const auto wav_a = temp_dir / "demo_a.wav";
  const auto wav_b = temp_dir / "demo_b.wav";

  piano::app::AppOptions options;
  const std::vector<std::string> keyboard_candidates = {
      "assets/default.keyboard",
      "../assets/default.keyboard",
  };
  const std::vector<std::string> score_candidates = {
      "assets/demo.in",
      "../assets/demo.in",
  };
  for (const auto& candidate : keyboard_candidates) {
    if (std::filesystem::exists(candidate)) {
      options.keyboard_map_path = candidate;
      break;
    }
  }
  for (const auto& candidate : score_candidates) {
    if (std::filesystem::exists(candidate)) {
      options.score_path = candidate;
      break;
    }
  }
  Require(!options.keyboard_map_path.empty(), "keyboard fixture should exist");
  Require(!options.score_path.empty(), "score fixture should exist");
  options.sample_rate = 16000;

  std::string error;
  Require(piano::exporter::ExportScoreToWav(options, wav_a.string(), &error), "first wav export should succeed");
  Require(piano::exporter::ExportScoreToWav(options, wav_b.string(), &error), "second wav export should succeed");

  std::ifstream in_a(wav_a, std::ios::binary);
  std::ifstream in_b(wav_b, std::ios::binary);
  std::vector<char> bytes_a((std::istreambuf_iterator<char>(in_a)), std::istreambuf_iterator<char>());
  std::vector<char> bytes_b((std::istreambuf_iterator<char>(in_b)), std::istreambuf_iterator<char>());
  Require(!bytes_a.empty(), "exported wav should not be empty");
  Require(bytes_a == bytes_b, "wav export should be deterministic for same input");
}

}  // namespace

int main() {
  TestKeyboardMapValid();
  TestKeyboardMapInvalidRange();
  TestScoreParserSortsByBegin();
  TestScoreParserLegacyColumnOrder();
  TestSchedulerRegression();
  TestSchedulerNoteOffOrdering();
  TestSchedulerUnsupportedToken();
  TestSchedulerLegacyScaleToken();
  TestConfigStoreRoundTripM6Fields();
  TestWavExportDeterministic();
  std::cout << "[PASS] piano_tests\n";
  return 0;
}
