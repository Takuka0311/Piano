#include "piano/app/application.h"

#include <iostream>

#include "piano/audio/log_output_sink.h"
#include "piano/engine/score_scheduler.h"
#include "piano/input/keyboard_map.h"
#include "piano/score/score_parser.h"

namespace piano::app {

int Application::Run(const AppOptions& options) const {
  input::KeyboardMap keyboard_map;
  std::string error;
  if (!keyboard_map.LoadFromFile(options.keyboard_map_path, &error)) {
    std::cerr << "Keyboard map load failed: " << error << '\n';
    return 1;
  }

  std::cout << "Loaded keyboard map: " << keyboard_map.Size() << " bindings\n";
  if (!options.probe_key.empty()) {
    const auto midi = keyboard_map.LookupMidiKey(options.probe_key);
    if (midi.has_value()) {
      std::cout << "Probe key '" << options.probe_key << "' -> midi " << *midi << '\n';
    } else {
      std::cout << "Probe key '" << options.probe_key << "' has no mapping\n";
    }
  }

  score::ScoreParser parser;
  if (!parser.LoadFromFile(options.score_path, &error)) {
    std::cerr << "Score load failed: " << error << '\n';
    return 1;
  }
  std::cout << "Loaded score commands: " << parser.Commands().size() << '\n';

  engine::ScoreScheduler scheduler;
  const auto events = scheduler.BuildEvents(parser.Commands(), &error);
  if (!error.empty()) {
    std::cerr << "Schedule failed: " << error << '\n';
    return 1;
  }
  std::cout << "Generated scheduled events: " << events.size() << '\n';

  audio::LogOutputSink sink(std::cout);
  for (const auto& event : events) {
    sink.Emit(event);
  }

  return 0;
}

}  // namespace piano::app
