
#pragma once

#include <string>
#include <cstdint>

namespace avioflow {

struct Metadata {
    double duration = 0.0;           // Duration in seconds
    int64_t num_samples = 0;         // Total number of samples (if known)
    int sample_rate = 0;             // Sampling frequency (Hz)
    int num_channels = 0;            // Number of audio channels
    std::string format;              // Container/Codec format name
    std::string artist;              // Metadata tags (potential expansion)
    std::string title;
};

} // namespace avioflow
