#pragma once

#include <Audioclient.h>
#include <Mmdeviceapi.h>

#include <atomic>
#include <deque>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "piano/audio/output_sink.h"

namespace piano::audio {

class WasapiOutputSink : public OutputSink {
 public:
  WasapiOutputSink(int sample_rate, int buffer_ms);
  ~WasapiOutputSink() override;

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
  float MixOneSample();
  void MarkUnhealthy(const std::string& message);
  static float MidiToFrequency(int midi_key);

  const int requested_sample_rate_;
  const int requested_buffer_ms_;

  std::atomic<bool> started_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> healthy_{true};
  bool com_initialized_ = false;
  std::thread render_thread_;

  IMMDevice* device_ = nullptr;
  IAudioClient* audio_client_ = nullptr;
  IAudioRenderClient* render_client_ = nullptr;
  WAVEFORMATEX* mix_format_ = nullptr;
  std::uint32_t buffer_frames_ = 0;
  int stream_channels_ = 2;
  int stream_sample_rate_ = 48000;
  bool stream_is_float_ = false;

  mutable std::mutex state_mutex_;
  std::unordered_map<int, ActiveNote> active_notes_;
  std::deque<QueuedEvent> event_queue_;
  std::string last_error_;
};

}  // namespace piano::audio
