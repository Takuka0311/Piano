#pragma once

#include <atomic>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <dsound.h>
#include <windows.h>

#include "piano/audio/output_sink.h"

struct AEffect;

namespace piano::audio {

class VstiOutputSink : public OutputSink {
 public:
  VstiOutputSink(std::string plugin_path, std::string midi_device_name, int sample_rate, int buffer_ms);
  ~VstiOutputSink() override;

  bool Start(std::string* error_message) override;
  void Stop() override;
  void Emit(const engine::ScheduledEvent& event) override;
  bool IsHealthy(std::string* error_message) const override;

 private:
  struct MidiMessage {
    unsigned char status = 0;
    unsigned char data1 = 0;
    unsigned char data2 = 0;
  };

  bool InitializePlugin(std::string* error_message);
  bool InitializeAudio(std::string* error_message);
  void ShutdownPlugin();
  void ShutdownAudio();
  void RenderLoop();
  void RenderFrames(int16_t* interleaved, int frames);
  void QueueMidi(unsigned char status, int midi_key, int velocity);
  void MarkUnhealthy(const std::string& error);

  std::string plugin_path_;
  std::string midi_device_name_;
  int requested_sample_rate_ = 48000;
  int requested_buffer_ms_ = 40;
  int stream_sample_rate_ = 44100;
  int block_frames_ = 256;
  std::atomic<bool> started_{false};
  std::atomic<bool> running_{false};
  std::atomic<bool> healthy_{true};

  HMODULE plugin_module_ = nullptr;
  ::AEffect* effect_ = nullptr;

  LPDIRECTSOUND8 dsound_ = nullptr;
  LPDIRECTSOUNDBUFFER primary_buffer_ = nullptr;
  LPDIRECTSOUNDBUFFER secondary_buffer_ = nullptr;
  DWORD buffer_bytes_ = 0;
  DWORD write_cursor_ = 0;
  std::thread render_thread_;

  std::deque<MidiMessage> midi_queue_;
  std::set<int> active_notes_;
  std::vector<float> left_buffer_;
  std::vector<float> right_buffer_;
  std::vector<float> temp_buffer_;
  std::vector<int16_t> pcm_buffer_;
  mutable std::mutex state_mutex_;
  std::string last_error_;
};

}  // namespace piano::audio
