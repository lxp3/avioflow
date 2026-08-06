#include "avioflow-cxx-api.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>

using namespace avioflow;

const std::string MP3_PATH = "public/wavs/TownTheme.mp3";

struct DecodeResult {
  size_t total_samples = 0;
  int sample_rate = 0;
  double elapsed_ms = 0.0;
};

DecodeResult decode_range(double start_seconds, std::optional<double> stop_seconds) {
  const auto t0 = std::chrono::steady_clock::now();

  AudioDecoder decoder;
  decoder.load_file(MP3_PATH);
  auto samples = decoder.get_samples(start_seconds, stop_seconds);

  const auto t1 = std::chrono::steady_clock::now();
  DecodeResult result;
  result.total_samples = samples.empty() ? 0 : samples[0].size();
  result.sample_rate = decoder.get_metadata().sample_rate;
  result.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  return result;
}

void test_full_vs_range_decode() {
  std::cout << "\n=== Test: Full Decode vs Time-Range Decode (10.3s - 20.3s) ===" << std::endl;

  const DecodeResult full = decode_range(0.0, std::nullopt);
  const DecodeResult range = decode_range(10.3, 20.3);

  const double full_duration = static_cast<double>(full.total_samples) / full.sample_rate;
  const double range_duration = static_cast<double>(range.total_samples) / range.sample_rate;

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "  Full decode : " << full.total_samples << " samples, "
            << full_duration << "s, " << full.elapsed_ms << " ms" << std::endl;
  std::cout << "  Range decode: " << range.total_samples << " samples, "
            << range_duration << "s, " << range.elapsed_ms << " ms" << std::endl;

  // Range decode must be sample-accurate: within a couple ms of the requested 10s window.
  assert(std::abs(range_duration - 10.0) < 0.01);
  assert(range.total_samples < full.total_samples);
  // Seeking should skip decode work for the prefix/suffix outside the range.
  assert(range.elapsed_ms < full.elapsed_ms);
}

void test_invalid_range() {
  std::cout << "\n=== Test: Invalid start_seconds/stop_seconds ===" << std::endl;

  AudioDecoder decoder;
  decoder.load_file(MP3_PATH);

  bool threw = false;
  try {
    decoder.get_samples(-1.0);
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    decoder.get_samples(10.0, 5.0);
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  assert(threw);

  std::cout << "  Invalid range combinations correctly rejected" << std::endl;
}

int main() {
  try {
    test_full_vs_range_decode();
    test_invalid_range();
    std::cout << "\nAll seek tests passed!" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
