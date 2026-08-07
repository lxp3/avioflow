#include "avioflow-cxx-api.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>

// assert() is compiled out in Release builds (NDEBUG), which would make this
// test pass unconditionally. CHECK always evaluates and reports.
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << "CHECK failed: " #cond " at " << __FILE__ << ":" << __LINE__ \
                << std::endl;                                                   \
      std::exit(1);                                                            \
    }                                                                          \
  } while (0)

using namespace avioflow;

const std::string MP3_PATH = "public/wavs/TownTheme.mp3";

// TownTheme.mp3 is ~97.45s at 44100 Hz.
constexpr double FILE_DURATION = 97.45;

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
  CHECK(std::abs(range_duration - 10.0) < 0.01);
  CHECK(range.total_samples < full.total_samples);
  // Seeking should skip decode work for the prefix/suffix outside the range.
  CHECK(range.elapsed_ms < full.elapsed_ms);
}

// Ranges late in the file are the regression this guards. The trim bounds are
// absolute sample offsets, but a seek restarts the decoder's own sample counter,
// so comparing the two directly made every range past roughly half the duration
// come back short or empty.
void test_ranges_across_the_whole_file() {
  std::cout << "\n=== Test: 5s windows across the whole file ===" << std::endl;

  for (double start = 0.0; start + 5.0 <= FILE_DURATION; start += 10.0) {
    const DecodeResult range = decode_range(start, start + 5.0);
    const double duration = range.sample_rate > 0
        ? static_cast<double>(range.total_samples) / range.sample_rate
        : 0.0;

    std::cout << "  [" << start << ", " << (start + 5.0) << ") -> "
              << range.total_samples << " samples, " << duration << "s" << std::endl;

    CHECK(range.total_samples > 0);
    CHECK(std::abs(duration - 5.0) < 0.01);
  }
}

// An unset stop_seconds must decode through to the end, from any start point.
void test_open_ended_ranges() {
  std::cout << "\n=== Test: Open-ended ranges ===" << std::endl;

  for (double start : {0.0, 30.0, 60.0, 90.0}) {
    const DecodeResult range = decode_range(start, std::nullopt);
    const double duration = range.sample_rate > 0
        ? static_cast<double>(range.total_samples) / range.sample_rate
        : 0.0;
    const double expected = FILE_DURATION - start;

    std::cout << "  from " << start << "s -> " << duration << "s (expected ~"
              << expected << "s)" << std::endl;

    CHECK(range.total_samples > 0);
    // Seeking lands on a frame boundary, so allow a frame of slack either way.
    CHECK(std::abs(duration - expected) < 0.1);
  }
}

// Each call seeks independently, so the same range must be repeatable and a
// different range must return different audio.
void test_repeated_ranges_are_stable() {
  std::cout << "\n=== Test: Repeated ranges on one decoder ===" << std::endl;

  AudioDecoder decoder;
  decoder.load_file(MP3_PATH);

  auto first = decoder.get_samples(60.0, 62.0);
  auto second = decoder.get_samples(60.0, 62.0);
  auto elsewhere = decoder.get_samples(20.0, 22.0);

  CHECK(!first.empty() && !first[0].empty());
  CHECK(!second.empty() && !second[0].empty());
  CHECK(!elsewhere.empty() && !elsewhere[0].empty());

  std::cout << "  60-62s twice: " << first[0].size() << " and " << second[0].size()
            << " samples" << std::endl;

  CHECK(first[0].size() == second[0].size());
  CHECK(first[0] == second[0]);
  CHECK(first[0] != elsewhere[0]);
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
  CHECK(threw);

  threw = false;
  try {
    decoder.get_samples(10.0, 5.0);
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  std::cout << "  Invalid range combinations correctly rejected" << std::endl;
}

int main() {
  try {
    test_full_vs_range_decode();
    test_ranges_across_the_whole_file();
    test_open_ended_ranges();
    test_repeated_ranges_are_stable();
    test_invalid_range();
    std::cout << "\nAll seek tests passed!" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
