
#pragma once

#ifdef AVIOFLOW_STATIC
#define AVIOFLOW_API
#else
#ifdef _WIN32
#ifdef AVIOFLOW_EXPORTS
#define AVIOFLOW_API __declspec(dllexport)
#else
#define AVIOFLOW_API __declspec(dllimport)
#endif
#else
#define AVIOFLOW_API __attribute__((visibility("default")))
#endif
#endif

#include <cstdint>
#include <optional>
#include <string>
#include <vector>


namespace avioflow {

struct AudioStreamOptions {
  std::optional<int> output_sample_rate;
  std::optional<int> output_num_channels;
  std::optional<int> input_sample_rate;
  std::optional<int> input_channels;
  std::optional<std::string> input_format;
};

/**
 * @brief Options for audio encoding and writing.
 */
struct AVIOFLOW_API AudioWriteOptions {
  /// Codec name. Common: "pcm_s16le", "pcm_f32le", "flac", "aac", "libmp3lame", "libopus"
  std::optional<std::string> codec_name;

  /// Container format. Common: "wav", "flac", "mp4", "adts", "mp3", "opus", "ogg"
  std::optional<std::string> container_format;

  /// Sample rate in Hz. Common: 8000, 16000, 22050, 44100, 48000
  std::optional<int> sample_rate;

  /// Number of channels. Common: 1 (mono), 2 (stereo)
  std::optional<int> num_channels;

  /// Bit rate in bits/s for lossy codecs. Common: 128000, 192000, 256000, 320000
  std::optional<int64_t> bit_rate;

  /// Sample format. Common: "s16", "s32", "flt", "fltp"
  std::optional<std::string> sample_format;

  /// Overwrite existing file
  bool overwrite = true;

  AudioWriteOptions() = default;

  /**
   * @brief Construct with format preset.
   * @param format "wav" (PCM S16LE), "flac", "aac", "mp3", "opus"
   * @param sample_rate Sample rate in Hz
   * @param num_channels Channel count
   * @param bit_rate Bit rate for lossy codecs
   */
  AudioWriteOptions(const std::string &format,
                    std::optional<int> sample_rate = std::nullopt,
                    std::optional<int> num_channels = std::nullopt,
                    std::optional<int64_t> bit_rate = std::nullopt);
};

struct DeviceInfo {
  std::string name;        // Unique identifier for opening the device
  std::string description; // Human-readable name
  bool is_output = false;  // true if it is an output/loopback device
};

struct Metadata {
  double duration = 0.0;     // Duration in seconds
  int64_t num_samples = 0;   // Total number of samples (if known)
  int sample_rate = 0;       // Sampling frequency (Hz)
  int num_channels = 0;      // Number of audio channels
  std::string sample_format; // Sample format name (e.g., "fltp")
  std::string codec;         // Codec name (e.g., "mp3")
  int64_t bit_rate = 0;      // Bit rate in bits/s
  std::string container;     // Container format (e.g., "mp3", "wav")
};

} // namespace avioflow
