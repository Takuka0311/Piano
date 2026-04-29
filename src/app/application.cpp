#include "piano/app/application.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "piano/audio/log_output_sink.h"
#include "piano/audio/output_sink.h"
#include "piano/audio/wasapi_output_sink.h"
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

  std::unique_ptr<audio::OutputSink> sink;
  bool using_log_backend = options.audio_backend == "log";
  const auto build_sink = [&](bool use_log) -> std::unique_ptr<audio::OutputSink> {
    if (use_log) {
      return std::make_unique<audio::LogOutputSink>(std::cout);
    }
    return std::make_unique<audio::WasapiOutputSink>(options.sample_rate, options.buffer_ms);
  };
  sink = build_sink(using_log_backend);

  if (!sink->Start(&error)) {
    std::cerr << "Audio backend start failed: " << error << '\n';
    if (!using_log_backend) {
      std::cerr << "Fallback to log backend\n";
      using_log_backend = true;
      sink = build_sink(true);
      if (!sink->Start(&error)) {
        std::cerr << "Log backend start failed: " << error << '\n';
        return 1;
      }
    } else {
      return 1;
    }
  }

  const auto start = std::chrono::steady_clock::now();
  for (const auto& event : events) {
    const auto target = start + std::chrono::milliseconds(static_cast<int>(event.at_ms));
    std::this_thread::sleep_until(target);

    if (!sink->IsHealthy(&error)) {
      std::cerr << "Audio backend runtime unhealthy: " << error << '\n';
      if (!using_log_backend) {
        std::cerr << "Fallback to log backend (runtime)\n";
        sink->Stop();
        using_log_backend = true;
        sink = build_sink(true);
        if (!sink->Start(&error)) {
          std::cerr << "Log backend start failed: " << error << '\n';
          return 1;
        }
      } else {
        return 1;
      }
    }

    sink->Emit(event);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  sink->Stop();

  return 0;
}

}  // namespace piano::app
