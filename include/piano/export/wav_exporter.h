#pragma once

#include <string>

#include "piano/app/application.h"

namespace piano::exporter {

bool ExportScoreToWav(const app::AppOptions& options,
                      const std::string& output_wav_path,
                      std::string* error_message);

}  // namespace piano::exporter
