#include <iostream>
#include <string>

#include "piano/app/application.h"

namespace {

piano::app::AppOptions ParseArgs(int argc, char** argv) {
  piano::app::AppOptions options;
  options.keyboard_map_path = "assets/default.keyboard";
  options.score_path = "assets/demo.in";

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--keyboard" && i + 1 < argc) {
      options.keyboard_map_path = argv[++i];
    } else if (arg == "--score" && i + 1 < argc) {
      options.score_path = argv[++i];
    } else if (arg == "--probe-key" && i + 1 < argc) {
      options.probe_key = argv[++i];
    }
  }

  return options;
}

}  // namespace

int main(int argc, char** argv) {
  const auto options = ParseArgs(argc, argv);
  std::cout << "Piano CLI M1 started\n";
  std::cout << "keyboard=" << options.keyboard_map_path << '\n';
  std::cout << "score=" << options.score_path << '\n';

  piano::app::Application app;
  return app.Run(options);
}
