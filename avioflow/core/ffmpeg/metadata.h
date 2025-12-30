
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace avioflow
{

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

  // Output structure for decoded audio frames (per-frame, zero-copy)
  struct FrameOutput
  {
    uint8_t *data = nullptr; // Pointer to planar float data (FLTP format)
    size_t size = 0;         // Size in bytes per channel
    int sample_rate = 0;     // Output sample rate
    int num_channels = 0;
    int num_samples = 0; // Samples per channel
  };

} // namespace avioflow
