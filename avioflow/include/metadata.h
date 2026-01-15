
#pragma once

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

// Output structure for complete decoded audio (offline decoding)
struct AudioSamples {
  std::vector<std::vector<float>> data; // Planar float data per channel
  int sample_rate = 0;
};

} // namespace avioflow
