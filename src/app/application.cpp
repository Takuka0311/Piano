#include "piano/app/application.h"

#include <iostream>

#include "piano/app/playback_service.h"

namespace piano::app {

int Application::Run(const AppOptions& options) const {
  std::string error;
  PlaybackService service;
  if (!service.Start(options, &error)) {
    std::cerr << "Playback start failed: " << error << '\n';
    return 1;
  }
  service.Wait();
  const auto snapshot = service.Snapshot();
  if (snapshot.state == PlaybackState::kError) {
    std::cerr << "Playback ended with error: " << snapshot.message << '\n';
    return 1;
  }
  return 0;
}

}  // namespace piano::app
