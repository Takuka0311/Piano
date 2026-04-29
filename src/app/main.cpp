#include <iostream>
#include <string>

#include "piano/app/application.h"

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
            << "  --audio-backend <name>    wasapi|log (default: wasapi)\n"
            << "  --sample-rate <value>     sample rate, e.g. 44100/48000 (default: 48000)\n"
            << "  --buffer-ms <value>       audio buffer in milliseconds (default: 40)\n"
            << "  --help                    show this message\n";
}

ParseResult ParseArgs(int argc, char** argv) {
  ParseResult result;
  result.options.keyboard_map_path = "assets/default.keyboard";
  result.options.score_path = "assets/demo.in";

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
    } else if (arg == "--audio-backend" && i + 1 < argc) {
      result.options.audio_backend = argv[++i];
    } else if (arg == "--sample-rate" && i + 1 < argc) {
      result.options.sample_rate = std::stoi(argv[++i]);
    } else if (arg == "--buffer-ms" && i + 1 < argc) {
      result.options.buffer_ms = std::stoi(argv[++i]);
    } else {
      result.ok = false;
      result.error = "Unknown or incomplete argument: " + arg;
      return result;
    }
  }

  if (result.options.audio_backend != "wasapi" && result.options.audio_backend != "log") {
    result.ok = false;
    result.error = "Invalid --audio-backend. Allowed values: wasapi|log";
    return result;
  }
  if (result.options.sample_rate <= 0 || result.options.buffer_ms <= 0) {
    result.ok = false;
    result.error = "--sample-rate and --buffer-ms must be positive";
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
  std::cout << "Piano CLI M3 started\n";
  std::cout << "keyboard=" << options.keyboard_map_path << '\n';
  std::cout << "score=" << options.score_path << '\n';
  std::cout << "audio_backend=" << options.audio_backend
            << " sample_rate=" << options.sample_rate
            << " buffer_ms=" << options.buffer_ms << '\n';

  piano::app::Application app;
  return app.Run(options);
}
