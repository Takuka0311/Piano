#include "piano/export/wav_exporter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "piano/engine/score_scheduler.h"
#include "piano/input/keyboard_map.h"
#include "piano/score/score_parser.h"

namespace piano::exporter {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTailSeconds = 0.5;
constexpr int kChannels = 2;
constexpr int kBitsPerSample = 16;

void WriteLe16(std::ofstream* stream, std::uint16_t value) {
  char bytes[2];
  bytes[0] = static_cast<char>(value & 0xFF);
  bytes[1] = static_cast<char>((value >> 8) & 0xFF);
  stream->write(bytes, sizeof(bytes));
}

void WriteLe32(std::ofstream* stream, std::uint32_t value) {
  char bytes[4];
  bytes[0] = static_cast<char>(value & 0xFF);
  bytes[1] = static_cast<char>((value >> 8) & 0xFF);
  bytes[2] = static_cast<char>((value >> 16) & 0xFF);
  bytes[3] = static_cast<char>((value >> 24) & 0xFF);
  stream->write(bytes, sizeof(bytes));
}

double MidiToFreq(int midi_key) {
  return 440.0 * std::pow(2.0, (static_cast<double>(midi_key) - 69.0) / 12.0);
}

struct ActiveNote {
  double freq = 0.0;
  double phase = 0.0;
};

}  // namespace

bool ExportScoreToWav(const app::AppOptions& options,
                      const std::string& output_wav_path,
                      std::string* error_message) {
  if (output_wav_path.empty()) {
    if (error_message != nullptr) {
      *error_message = "output wav path is empty";
    }
    return false;
  }
  if (options.sample_rate <= 0) {
    if (error_message != nullptr) {
      *error_message = "invalid sample rate";
    }
    return false;
  }

  input::KeyboardMap keyboard_map;
  std::string error;
  if (!keyboard_map.LoadFromFile(options.keyboard_map_path, &error)) {
    if (error_message != nullptr) {
      *error_message = error;
    }
    return false;
  }

  score::ScoreParser parser;
  if (!parser.LoadFromFile(options.score_path, &error)) {
    if (error_message != nullptr) {
      *error_message = error;
    }
    return false;
  }

  engine::ScoreScheduler scheduler;
  std::vector<engine::ScheduledEvent> events = scheduler.BuildEvents(parser.Commands(), &error);
  if (!error.empty()) {
    if (error_message != nullptr) {
      *error_message = error;
    }
    return false;
  }

  double duration_ms = 0.0;
  for (const auto& event : events) {
    duration_ms = (std::max)(duration_ms, event.at_ms);
  }
  const double total_seconds = duration_ms / 1000.0 + kTailSeconds;
  const std::size_t total_frames =
      static_cast<std::size_t>((std::max)(1.0, std::floor(total_seconds * static_cast<double>(options.sample_rate))));
  std::vector<std::int16_t> pcm;
  pcm.resize(total_frames * kChannels, 0);

  std::unordered_map<int, ActiveNote> active_notes;
  std::size_t event_index = 0;
  const auto sorted_events = events;

  for (std::size_t frame = 0; frame < total_frames; ++frame) {
    const double current_ms = static_cast<double>(frame) * 1000.0 / static_cast<double>(options.sample_rate);
    while (event_index < sorted_events.size() && sorted_events[event_index].at_ms <= current_ms) {
      const auto& event = sorted_events[event_index];
      if (event.type == engine::EventType::kNoteOn && event.midi_key >= 0) {
        active_notes[event.midi_key] = ActiveNote{MidiToFreq(event.midi_key), 0.0};
      } else if (event.type == engine::EventType::kNoteOff && event.midi_key >= 0) {
        active_notes.erase(event.midi_key);
      }
      ++event_index;
    }

    double sample = 0.0;
    for (auto& [_, note] : active_notes) {
      note.phase += (2.0 * kPi * note.freq) / static_cast<double>(options.sample_rate);
      if (note.phase > 2.0 * kPi) {
        note.phase -= 2.0 * kPi;
      }
      sample += std::sin(note.phase) * 0.2;
    }
    sample = (std::max)(-1.0, (std::min)(1.0, sample));
    const auto s16 = static_cast<std::int16_t>(sample * 32767.0);
    pcm[frame * 2] = s16;
    pcm[frame * 2 + 1] = s16;
  }

  std::ofstream out(output_wav_path, std::ios::binary);
  if (!out.is_open()) {
    if (error_message != nullptr) {
      *error_message = "failed to open wav file: " + output_wav_path;
    }
    return false;
  }

  const std::uint32_t data_size = static_cast<std::uint32_t>(pcm.size() * sizeof(std::int16_t));
  const std::uint32_t byte_rate = static_cast<std::uint32_t>(options.sample_rate * kChannels * (kBitsPerSample / 8));
  const std::uint16_t block_align = static_cast<std::uint16_t>(kChannels * (kBitsPerSample / 8));

  out.write("RIFF", 4);
  WriteLe32(&out, 36 + data_size);
  out.write("WAVE", 4);
  out.write("fmt ", 4);
  WriteLe32(&out, 16);
  WriteLe16(&out, 1);
  WriteLe16(&out, kChannels);
  WriteLe32(&out, static_cast<std::uint32_t>(options.sample_rate));
  WriteLe32(&out, byte_rate);
  WriteLe16(&out, block_align);
  WriteLe16(&out, kBitsPerSample);
  out.write("data", 4);
  WriteLe32(&out, data_size);
  out.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(data_size));
  return true;
}

}  // namespace piano::exporter
