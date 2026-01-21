#pragma once

#include "metadata.h"
#include <functional>
#include <memory>
#include <vector>

#ifdef _WIN32
#ifdef AVIOFLOW_EXPORTS
#define AVIOFLOW_API __declspec(dllexport)
#else
#define AVIOFLOW_API __declspec(dllimport)
#endif
#else
#define AVIOFLOW_API __attribute__((visibility("default")))
#endif

namespace avioflow {

// AVIO read callback for streaming input
// Returns: >0 (bytes read), 0 (EOF), <0 (no data available, try again)
using AVIOReadCallback = std::function<int(uint8_t *, int)>;

// Global configuration
// level: "quiet", "panic", "fatal", "error", "warning", "info", "verbose", "debug", "trace"
// If level is nullptr, it reads from the environment variable AVIOFLOW_LOG_LEVEL.
AVIOFLOW_API void avioflow_set_log_level(const char *level = nullptr);

// Audio Decoder - Public API using PIMPL
class AVIOFLOW_API AudioDecoder {
public:
  explicit AudioDecoder(const AudioStreamOptions &options = {});
  ~AudioDecoder();

  // Non-copyable
  AudioDecoder(const AudioDecoder &) = delete;
  AudioDecoder &operator=(const AudioDecoder &) = delete;

  // Movable
  AudioDecoder(AudioDecoder &&) noexcept;
  AudioDecoder &operator=(AudioDecoder &&) noexcept;

  // --- Input Methods ---

  // Open from file path, URL, or device
  void open(const std::string &source);

  // Open from memory buffer
  void open_memory(const uint8_t *data, size_t size);

  // Open for streaming with read callback
  void open_stream(AVIOReadCallback avio_read_callback);

  // --- Decoding Methods ---

  // Decode next frame and return as AudioSamples
  // Returns empty AudioSamples if no data available or EOF
  AudioSamples decode_next();

  // Decode entire audio at once (offline mode)
  AudioSamples get_all_samples();

  // --- Status ---

  bool is_finished() const;
  const Metadata &get_metadata() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Device Manager for hardware discovery
class AVIOFLOW_API DeviceManager {
public:
  static std::vector<DeviceInfo> list_audio_devices();
};

} // namespace avioflow
