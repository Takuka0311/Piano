#pragma once

#include <memory>
#include <string>
#include <vector>

#include "piano/app/application.h"

namespace piano::audio {
class OutputSink;
}

namespace piano::app {

std::vector<std::string> ResolveBackendOrder(const AppOptions& options);

bool BuildAndStartSink(const AppOptions& options,
                       const std::vector<std::string>& preferred_backends,
                       std::unique_ptr<audio::OutputSink>* sink,
                       std::string* active_backend,
                       std::string* error_message);

bool CreateOutputSink(const std::string& backend,
                      const AppOptions& options,
                      std::unique_ptr<audio::OutputSink>* sink,
                      std::string* error_message);

}  // namespace piano::app
