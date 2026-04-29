#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <dsound.h>

#include "piano/audio/output_sink.h"

namespace piano::audio {

class DsoundOutputSink : public OutputSink {
 public:
  DsoundOutputSink(int sample_rate, int buffer_ms);
  ~DsoundOutputSink() override;

  bool Start(std::string* error_message) override;
  void Stop() override;
  void Emit(const engine::ScheduledEvent& event) override;
  bool IsHealthy(std::string* error_message) const override;

 private:
  struct ActiveNote {
    float frequency_hz = 440.0f;
    float amplitude = 0.1f;
    double phase = 0.0;
  };

  struct QueuedEvent {
    engine::EventType type = engine::EventType::kNoteOn;
    int midi_key = -1;
    int value = 0;
  };

  bool InitializeDevice(std::string* error_message);
  void ShutdownDevice();
  void RenderLoop();
  void ApplyQueuedEvents();
  int16_t MixOneSample();
  void MarkUnhealthy(const std::string& message);
  static float MidiToFrequency(int midi_key);

  const int requested_sample_rate_;
  const int requested_buffer_ms_;

  std::atomic<bool> started_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> healthy_{true};
  std::thread render_thread_;

  LPDIRECTSOUND8 dsound_ = nullptr;
  LPDIRECTSOUNDBUFFER primary_buffer_ = nullptr;
  LPDIRECTSOUNDBUFFER secondary_buffer_ = nullptr;
  DWORD buffer_bytes_ = 0;
  DWORD write_cursor_ = 0;
  int stream_sample_rate_ = 48000;

  mutable std::mutex state_mutex_;
  std::unordered_map<int, ActiveNote> active_notes_;
  std::deque<QueuedEvent> event_queue_;
  std::string last_error_;
};

}  // namespace piano::audio
