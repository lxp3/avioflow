
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace avioflow
{

  struct AudioStreamOptions
  {
    std::optional<int> output_sample_rate;
    std::optional<int> output_num_channels;
    std::optional<int> input_sample_rate;
    std::optional<int> input_channels;
    std::optional<std::string> input_format;
  };

  struct Metadata
  {
    double duration = 0.0;     // Duration in seconds
    int64_t num_samples = 0;   // Total number of samples (if known)
    int sample_rate = 0;       // Sampling frequency (Hz)
    int num_channels = 0;      // Number of audio channels
    std::string sample_format; // Container/Codec format name
  };

  // Output structure for complete decoded audio (offline decoding)
  struct AudioSamples
  {
    std::vector<std::vector<float>> data; // Planar float data per channel
    int sample_rate = 0;
  };

} // namespace avioflow
