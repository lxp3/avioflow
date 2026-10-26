#include "avioflow-cxx-api.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Benchmarks audio extraction from a media file (typically a video container).
//
// Usage: ffmpeg-decoder-video-benchmark <file> [iterations]
//
// Reports the median of `iterations` runs (default 5) for:
//   open        load_file(), which includes avformat_find_stream_info
//   full        open + decode every sample
//   range       open + decode a 10s window from the middle of the file
//
// Compare an audio-only file against a video file of similar audio length to
// see how much the video stream costs.

using namespace avioflow;
using Clock = std::chrono::steady_clock;

namespace {

double ms_since(Clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

double median(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

bool has_format(const std::vector<std::string> &formats, const std::string &name) {
  // Demuxer names can be comma separated, e.g. "mov,mp4,m4a,3gp,3g2,mj2".
  for (const auto &f : formats)
    if (f == name || f.find(name) != std::string::npos)
      return true;
  return false;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <file> [iterations]" << std::endl;
    return 1;
  }
  const std::string path = argv[1];
  const int iterations = argc > 2 ? std::max(1, std::atoi(argv[2])) : 5;

  const auto demuxers = get_supported_input_formats();
  std::cout << "Video demuxers in this FFmpeg build: mov="
            << has_format(demuxers, "mov") << " matroska="
            << has_format(demuxers, "matroska") << " avi="
            << has_format(demuxers, "avi") << std::endl;

  try {
    std::vector<double> open_ms, full_ms, range_ms;
    Metadata meta;
    size_t full_samples = 0, range_samples = 0;

    for (int i = 0; i < iterations; ++i) {
      {
        const auto t0 = Clock::now();
        AudioDecoder decoder;
        meta = decoder.load_file(path);
        open_ms.push_back(ms_since(t0));
      }
      {
        const auto t0 = Clock::now();
        AudioDecoder decoder;
        decoder.load_file(path);
        auto samples = decoder.get_samples();
        full_ms.push_back(ms_since(t0));
        full_samples = samples.empty() ? 0 : samples[0].size();
      }
      {
        const double mid = std::max(0.0, meta.duration / 2.0 - 5.0);
        const auto t0 = Clock::now();
        AudioDecoder decoder;
        decoder.load_file(path);
        auto samples = decoder.get_samples(mid, mid + 10.0);
        range_ms.push_back(ms_since(t0));
        range_samples = samples.empty() ? 0 : samples[0].size();
      }
    }

    std::cout << std::fixed << std::setprecision(1);
    std::cout << "File: " << path << std::endl;
    std::cout << "  container=" << meta.container << " codec=" << meta.codec
              << " " << meta.sample_rate << " Hz x" << meta.num_channels
              << ", " << meta.duration << "s" << std::endl;
    std::cout << "  iterations=" << iterations << " (median)" << std::endl;
    std::cout << "  open : " << median(open_ms) << " ms" << std::endl;
    std::cout << "  full : " << median(full_ms) << " ms (" << full_samples
              << " samples)" << std::endl;
    std::cout << "  range: " << median(range_ms) << " ms (" << range_samples
              << " samples)" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Benchmark failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
