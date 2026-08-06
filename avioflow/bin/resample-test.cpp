#include "avioflow-cxx-api.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace avioflow;

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

namespace {

// MSVC does not define M_PI in <cmath> without _USE_MATH_DEFINES.
constexpr double kPi = 3.14159265358979323846;

// Generate a sine wave so resampled output can be checked for sanity.
std::vector<std::vector<float>> make_sine(int num_channels, int num_samples,
                                         int sample_rate, double freq) {
  std::vector<std::vector<float>> out(static_cast<size_t>(num_channels));
  for (int c = 0; c < num_channels; ++c) {
    out[static_cast<size_t>(c)].resize(static_cast<size_t>(num_samples));
    for (int i = 0; i < num_samples; ++i) {
      out[static_cast<size_t>(c)][static_cast<size_t>(i)] = static_cast<float>(
          std::sin(2.0 * kPi * freq * i / sample_rate));
    }
  }
  return out;
}

double peak(const std::vector<float> &channel) {
  double max_value = 0.0;
  for (float sample : channel)
    max_value = std::max(max_value, std::abs(static_cast<double>(sample)));
  return max_value;
}

} // namespace

void test_one_shot_downsample() {
  std::cout << "\n=== Test: One-shot downsample 44100 -> 16000 ===" << std::endl;

  const int in_rate = 44100;
  const int out_rate = 16000;
  const int in_samples = in_rate; // exactly 1 second
  auto input = make_sine(2, in_samples, in_rate, 440.0);

  auto output = resample(input, in_rate, out_rate);

  CHECK(output.size() == 2);
  const double expected = static_cast<double>(in_samples) * out_rate / in_rate;
  const double actual = static_cast<double>(output[0].size());

  std::cout << std::fixed << std::setprecision(2);
  std::cout << "  in : " << in_samples << " samples @ " << in_rate << "Hz\n";
  std::cout << "  out: " << output[0].size() << " samples @ " << out_rate
            << "Hz (expected ~" << expected << ")\n";
  std::cout << "  diff: " << (expected - actual) << " samples\n";

  // With flush() the whole tail is retained, so the count lands within a
  // couple of samples of the exact ratio.
  CHECK(std::abs(expected - actual) <= 2.0);
  CHECK(output[1].size() == output[0].size());

  // A 440Hz sine is well below the 8kHz Nyquist limit, so amplitude survives.
  const double amplitude = peak(output[0]);
  std::cout << "  peak amplitude: " << amplitude << " (expected ~1.0)\n";
  CHECK(amplitude > 0.9 && amplitude < 1.1);
}

void test_flush_recovers_tail() {
  std::cout << "\n=== Test: flush() recovers the resampler tail ===" << std::endl;

  const int in_rate = 44100;
  const int out_rate = 16000;
  auto input = make_sine(1, in_rate, in_rate, 440.0);

  AudioResampler resampler({.input_sample_rate = in_rate,
                            .output_sample_rate = out_rate});
  auto body = resampler.process(input);
  auto tail = resampler.flush();

  CHECK(!body.empty());
  CHECK(!tail.empty());

  std::cout << "  process(): " << body[0].size() << " samples\n";
  std::cout << "  flush()  : " << tail[0].size() << " samples\n";

  const double expected = static_cast<double>(in_rate) * out_rate / in_rate;
  const double without_flush = static_cast<double>(body[0].size());
  const double with_flush = without_flush + static_cast<double>(tail[0].size());
  std::cout << "  without flush: " << without_flush << " (short by "
            << (expected - without_flush) << ")\n";
  std::cout << "  with flush   : " << with_flush << " (expected ~" << expected
            << ")\n";

  // The guarantee: only after flush() does the count match the rate ratio.
  // How much swresample withholds is an internal detail, so assert the total
  // rather than a specific tail length.
  CHECK(std::abs(expected - with_flush) <= 2.0);
  CHECK(with_flush >= without_flush);
}

void test_chunked_matches_one_shot() {
  std::cout << "\n=== Test: Chunked equals one-shot ===" << std::endl;

  const int in_rate = 48000;
  const int out_rate = 16000;
  const int in_samples = 48000;
  auto input = make_sine(1, in_samples, in_rate, 220.0);

  auto expected = resample(input, in_rate, out_rate);

  // Feed the same audio in chunks through one resampler instance.
  AudioResampler resampler({.input_sample_rate = in_rate,
                            .output_sample_rate = out_rate});
  std::vector<float> chunked;
  const int chunk = 1000;
  for (int offset = 0; offset < in_samples; offset += chunk) {
    const int len = std::min(chunk, in_samples - offset);
    std::vector<std::vector<float>> piece(1);
    piece[0].assign(input[0].begin() + offset, input[0].begin() + offset + len);
    auto part = resampler.process(piece);
    if (!part.empty())
      chunked.insert(chunked.end(), part[0].begin(), part[0].end());
  }
  auto tail = resampler.flush();
  if (!tail.empty())
    chunked.insert(chunked.end(), tail[0].begin(), tail[0].end());

  std::cout << "  one-shot: " << expected[0].size() << " samples\n";
  std::cout << "  chunked : " << chunked.size() << " samples\n";

  // Filter state carries across chunks, so results match sample-for-sample.
  CHECK(chunked.size() == expected[0].size());

  double max_diff = 0.0;
  for (size_t i = 0; i < chunked.size(); ++i) {
    max_diff = std::max(max_diff, std::abs(static_cast<double>(chunked[i]) -
                                           static_cast<double>(expected[0][i])));
  }
  std::cout << "  max sample difference: " << std::scientific << max_diff
            << std::fixed << "\n";
  CHECK(max_diff < 1e-6);
}

void test_upsample_and_downmix() {
  std::cout << "\n=== Test: Upsample and channel downmix ===" << std::endl;

  auto input = make_sine(2, 16000, 16000, 440.0);

  auto up = resample(input, 16000, 44100);
  const double expected_up = 16000.0 * 44100 / 16000;
  std::cout << "  16000 -> 44100: " << up[0].size() << " samples (expected ~"
            << expected_up << ")\n";
  CHECK(up.size() == 2);
  CHECK(std::abs(expected_up - static_cast<double>(up[0].size())) <= 2.0);

  auto mono = resample(input, 16000, 16000, 1);
  std::cout << "  stereo -> mono: " << mono.size() << " channel, "
            << mono[0].size() << " samples\n";
  CHECK(mono.size() == 1);
  CHECK(mono[0].size() == 16000);
}

void test_invalid_arguments() {
  std::cout << "\n=== Test: Invalid arguments ===" << std::endl;

  auto input = make_sine(1, 100, 16000, 440.0);

  bool threw = false;
  try {
    resample(input, 0, 16000);
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  threw = false;
  try {
    resample(input, 16000, -1);
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  // Ragged channels must be rejected rather than read out of bounds.
  threw = false;
  try {
    std::vector<std::vector<float>> ragged{std::vector<float>(100, 0.0f),
                                           std::vector<float>(50, 0.0f)};
    resample(ragged, 16000, 8000);
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  // Channel count must stay stable across process() calls.
  threw = false;
  try {
    AudioResampler resampler({.input_sample_rate = 16000,
                              .output_sample_rate = 8000});
    resampler.process(make_sine(2, 100, 16000, 440.0));
    resampler.process(make_sine(1, 100, 16000, 440.0));
  } catch (const std::invalid_argument &) {
    threw = true;
  }
  CHECK(threw);

  std::cout << "  Invalid inputs correctly rejected" << std::endl;
}

void test_empty_input() {
  std::cout << "\n=== Test: Empty input ===" << std::endl;

  auto out = resample({}, 44100, 16000);
  CHECK(out.empty());

  // flush() before any process() must not crash.
  AudioResampler resampler({.input_sample_rate = 44100,
                            .output_sample_rate = 16000});
  auto tail = resampler.flush();
  CHECK(tail.empty());

  std::cout << "  Empty input handled" << std::endl;
}

int main() {
  try {
    test_one_shot_downsample();
    test_flush_recovers_tail();
    test_chunked_matches_one_shot();
    test_upsample_and_downmix();
    test_invalid_arguments();
    test_empty_input();
    std::cout << "\nAll resample tests passed!" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
