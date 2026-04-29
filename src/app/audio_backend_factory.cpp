#include "piano/app/audio_backend_factory.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

#include "piano/audio/dsound_output_sink.h"
#include "piano/audio/log_output_sink.h"
#include "piano/audio/midi_output_sink.h"
#include "piano/audio/output_sink.h"
#include "piano/audio/vsti_output_sink.h"
#include "piano/audio/wasapi_output_sink.h"

namespace piano::app {
namespace {

std::vector<std::string> ParseBackendPriority(const std::string& raw) {
  std::vector<std::string> out;
  std::stringstream ss(raw);
  std::string item;
  while (std::getline(ss, item, ',')) {
    item.erase(std::remove_if(item.begin(), item.end(), [](unsigned char c) { return std::isspace(c); }), item.end());
    if (!item.empty()) {
      out.push_back(item);
    }
  }
  if (out.empty()) {
    out = {"vsti", "midiout", "wasapi", "dsound", "log"};
  }
  return out;
}

}  // namespace

std::vector<std::string> ResolveBackendOrder(const AppOptions& options) {
  const auto configured = ParseBackendPriority(options.backend_priority);
  std::vector<std::string> candidates;
  auto append_unique = [&candidates](const std::string& backend) {
    if (std::find(candidates.begin(), candidates.end(), backend) == candidates.end()) {
      candidates.push_back(backend);
    }
  };

  std::string preferred = options.output_mode;
  if (preferred.empty()) {
    preferred = options.audio_backend;
  }
  if (preferred == "vsti") {
    append_unique("vsti");
    append_unique("midiout");
    append_unique("wasapi");
    append_unique("dsound");
  } else if (preferred == "midiout") {
    append_unique("midiout");
    append_unique("wasapi");
    append_unique("dsound");
  } else if (preferred == "wasapi") {
    append_unique("wasapi");
    append_unique("dsound");
  } else if (preferred == "dsound") {
    append_unique("dsound");
  } else if (preferred == "log") {
    append_unique("log");
  } else {
    append_unique("wasapi");
    append_unique("dsound");
  }

  for (const auto& item : configured) {
    append_unique(item);
  }
  append_unique("log");
  return candidates;
}

bool CreateOutputSink(const std::string& backend,
                      const AppOptions& options,
                      std::unique_ptr<audio::OutputSink>* sink,
                      std::string* error_message) {
  if (sink == nullptr) {
    if (error_message != nullptr) {
      *error_message = "output sink pointer is null";
    }
    return false;
  }
  if (backend == "log") {
    *sink = std::make_unique<audio::LogOutputSink>(std::cout);
    return true;
  }
  if (backend == "wasapi") {
    *sink = std::make_unique<audio::WasapiOutputSink>(options.sample_rate, options.buffer_ms);
    return true;
  }
  if (backend == "dsound") {
    *sink = std::make_unique<audio::DsoundOutputSink>(options.sample_rate, options.buffer_ms);
    return true;
  }
  if (backend == "midiout") {
    *sink = std::make_unique<audio::MidiOutputSink>(options.midi_out_device);
    return true;
  }
  if (backend == "vsti") {
    *sink = std::make_unique<audio::VstiOutputSink>(options.vsti_plugin_path,
                                                    options.midi_out_device,
                                                    options.sample_rate,
                                                    options.buffer_ms);
    return true;
  }
  if (error_message != nullptr) {
    *error_message = "unsupported backend: " + backend;
  }
  return false;
}

bool BuildAndStartSink(const AppOptions& options,
                       const std::vector<std::string>& preferred_backends,
                       std::unique_ptr<audio::OutputSink>* sink,
                       std::string* active_backend,
                       std::string* error_message) {
  std::string start_error = "no backend candidate";
  for (const auto& backend : preferred_backends) {
    std::unique_ptr<audio::OutputSink> candidate;
    if (!CreateOutputSink(backend, options, &candidate, &start_error)) {
      continue;
    }
    if (candidate == nullptr) {
      start_error = "backend created null sink: " + backend;
      continue;
    }
    std::string backend_error;
    if (candidate->Start(&backend_error)) {
      *sink = std::move(candidate);
      if (active_backend != nullptr) {
        *active_backend = backend;
      }
      return true;
    }
    start_error = backend + ": " + backend_error;
  }
  if (error_message != nullptr) {
    *error_message = start_error;
  }
  return false;
}

}  // namespace piano::app
