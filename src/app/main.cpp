#include <iostream>
#include <string>

#include "piano/app/application.h"
#include "piano/app/error_codes.h"
#include "piano/platform/config_store.h"

namespace {

struct ParseResult {
  piano::app::AppOptions options;
  bool ok = true;
  bool show_help = false;
  std::string error;
};

void PrintUsage(const char* program) {
  std::cout << "Usage: " << program << " [options]\n"
            << "  --keyboard <path>         keyboard mapping file (default: assets/default.keyboard)\n"
            << "  --score <path>            score file (default: assets/demo.in)\n"
            << "  --probe-key <name>        optional probe key for mapping lookup\n"
            << "  --output-mode <name>      vsti|midiout|wasapi|dsound|log (default: wasapi)\n"
            << "  --audio-backend <name>    legacy alias of --output-mode\n"
            << "  --backend-priority <list> fallback order, e.g. vsti,midiout,wasapi,dsound,log\n"
            << "  --midi-out-device <name>  MIDI out device name or index\n"
            << "  --vsti-plugin <path>      VST2.4 plugin path (.dll)\n"
            << "  --sample-rate <value>     sample rate, e.g. 44100/48000 (default: 48000)\n"
            << "  --buffer-ms <value>       audio buffer in milliseconds (default: 40)\n"
            << "  --help                    show this message\n";
}

ParseResult ParseArgs(int argc, char** argv) {
  ParseResult result;
  piano::platform::ConfigStore config_store("piano-ui-config.ini");
  const auto persisted = config_store.Load();
  result.options.keyboard_map_path = persisted.keyboard_path;
  result.options.score_path = persisted.score_path;
  result.options.output_mode = persisted.output_mode;
  result.options.audio_backend = persisted.audio_backend;
  result.options.backend_priority = persisted.backend_priority;
  result.options.midi_out_device = persisted.midi_out_device;
  result.options.vsti_plugin_path = persisted.vsti_plugin_path;
  result.options.sample_rate = persisted.sample_rate;
  result.options.buffer_ms = persisted.buffer_ms;
  if (result.options.output_mode.empty()) {
    result.options.output_mode = result.options.audio_backend.empty() ? "wasapi" : result.options.audio_backend;
  }
  if (result.options.audio_backend.empty()) {
    result.options.audio_backend = result.options.output_mode;
  }

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help") {
      result.show_help = true;
      return result;
    }
    if (arg == "--keyboard" && i + 1 < argc) {
      result.options.keyboard_map_path = argv[++i];
    } else if (arg == "--score" && i + 1 < argc) {
      result.options.score_path = argv[++i];
    } else if (arg == "--probe-key" && i + 1 < argc) {
      result.options.probe_key = argv[++i];
    } else if (arg == "--output-mode" && i + 1 < argc) {
      result.options.output_mode = argv[++i];
      result.options.audio_backend = result.options.output_mode;
    } else if (arg == "--audio-backend" && i + 1 < argc) {
      result.options.audio_backend = argv[++i];
      result.options.output_mode = result.options.audio_backend;
    } else if (arg == "--backend-priority" && i + 1 < argc) {
      result.options.backend_priority = argv[++i];
    } else if (arg == "--midi-out-device" && i + 1 < argc) {
      result.options.midi_out_device = argv[++i];
    } else if (arg == "--vsti-plugin" && i + 1 < argc) {
      result.options.vsti_plugin_path = argv[++i];
    } else if (arg == "--sample-rate" && i + 1 < argc) {
      result.options.sample_rate = std::stoi(argv[++i]);
    } else if (arg == "--buffer-ms" && i + 1 < argc) {
      result.options.buffer_ms = std::stoi(argv[++i]);
    } else {
      result.ok = false;
      result.error = piano::app::FormatError(piano::app::AppErrorCode::kInvalidArgument,
                                             "Unknown or incomplete argument: " + arg);
      return result;
    }
  }

  if (result.options.output_mode != "vsti" && result.options.output_mode != "midiout" &&
      result.options.output_mode != "wasapi" && result.options.output_mode != "dsound" &&
      result.options.output_mode != "log") {
    result.ok = false;
    result.error = piano::app::FormatError(piano::app::AppErrorCode::kInvalidArgument,
                                           "Invalid --output-mode. Allowed values: vsti|midiout|wasapi|dsound|log");
    return result;
  }
  if (result.options.sample_rate <= 0 || result.options.buffer_ms <= 0) {
    result.ok = false;
    result.error = piano::app::FormatError(piano::app::AppErrorCode::kInvalidArgument,
                                           "--sample-rate and --buffer-ms must be positive");
    return result;
  }
  if (result.options.output_mode == "vsti" && result.options.vsti_plugin_path.empty()) {
    result.ok = false;
    result.error = piano::app::FormatError(piano::app::AppErrorCode::kInvalidArgument,
                                           "--vsti-plugin is required when --output-mode vsti");
    return result;
  }

  return result;
}

}  // namespace

int main(int argc, char** argv) {
  const auto parsed = ParseArgs(argc, argv);
  if (parsed.show_help) {
    PrintUsage(argv[0]);
    return 0;
  }
  if (!parsed.ok) {
    std::cerr << parsed.error << '\n';
    PrintUsage(argv[0]);
    return 1;
  }

  const auto options = parsed.options;
  std::cout << "Piano CLI M6 started\n";
  std::cout << "keyboard=" << options.keyboard_map_path << '\n';
  std::cout << "score=" << options.score_path << '\n';
  std::cout << "output_mode=" << options.output_mode
            << " audio_backend=" << options.audio_backend
            << " sample_rate=" << options.sample_rate
            << " buffer_ms=" << options.buffer_ms << '\n';

  piano::app::Application app;
  const int run_code = app.Run(options);

  piano::platform::ConfigStore config_store("piano-ui-config.ini");
  auto config = config_store.Load();
  config.keyboard_path = options.keyboard_map_path;
  config.score_path = options.score_path;
  config.output_mode = options.output_mode;
  config.audio_backend = options.audio_backend;
  config.backend_priority = options.backend_priority;
  config.midi_out_device = options.midi_out_device;
  config.vsti_plugin_path = options.vsti_plugin_path;
  config.sample_rate = options.sample_rate;
  config.buffer_ms = options.buffer_ms;
  config.recent_keyboard_path = options.keyboard_map_path;
  config.recent_score_path = options.score_path;
  std::string save_error;
  if (!config_store.Save(config, &save_error)) {
    std::cerr << piano::app::FormatError(piano::app::AppErrorCode::kConfigIoFailed, save_error) << '\n';
  }

  return run_code;
}
