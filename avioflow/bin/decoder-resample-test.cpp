// Unit tests for SingleStreamDecoder - Resampling
// Tests cover: output sample rate conversion to common rates using get_samples()

#include "avioflow-cxx-api.h"
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <fstream>
#include <iostream>
#include <algorithm>

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

// Test file paths
const std::string TEST_FILE_PATH = "public/wavs/TownTheme.mp3";

// Original file parameters
constexpr int ORIGINAL_SAMPLE_RATE = 44100;
constexpr int ORIGINAL_NUM_SAMPLES = 4297722;
constexpr int EXPECTED_NUM_CHANNELS = 2;

// Expected sample counts after resampling
constexpr int EXPECTED_SAMPLES_8000 = 779632;
constexpr int EXPECTED_SAMPLES_16000 = 1559264;
constexpr int EXPECTED_SAMPLES_32000 = 3118529;
constexpr int EXPECTED_SAMPLES_44100 = 4297722;
constexpr int EXPECTED_SAMPLES_48000 = 4677793;

// Only rounding of the rate ratio may move the count; the resampler tail is
// drained at EOF, so a larger gap means samples are being dropped.
constexpr int64_t SAMPLE_COUNT_TOLERANCE = 2;

//=============================================================================
// Helper: Check if sample count is within tolerance
//=============================================================================
bool is_within_tolerance(size_t actual, int expected)
{
    const int64_t diff = static_cast<int64_t>(actual) - expected;
    return diff >= -SAMPLE_COUNT_TOLERANCE && diff <= SAMPLE_COUNT_TOLERANCE;
}

//=============================================================================
// Test: Resample to 8000 Hz
//=============================================================================
void test_resample_8000()
{
    std::cout << "Running test_resample_8000..." << std::endl;
    constexpr int TARGET_RATE = 8000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.load_file(TEST_FILE_PATH);

    // Verify source metadata unchanged
    const auto &meta = decoder.get_metadata();
    CHECK(meta.sample_rate == ORIGINAL_SAMPLE_RATE);
    CHECK(meta.num_channels == EXPECTED_NUM_CHANNELS);

    auto samples = decoder.get_samples();
    size_t num_samples = samples.empty() ? 0 : samples[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_8000;
    std::cout << "sample_rate: " << meta.sample_rate << " -> " << TARGET_RATE 
              << ", num_samples: " << num_samples << ", diff: " << diff << std::endl;

    CHECK((int)samples.size() == EXPECTED_NUM_CHANNELS);
    CHECK(is_within_tolerance(num_samples, EXPECTED_SAMPLES_8000));
}

//=============================================================================
// Test: Resample to 16000 Hz
//=============================================================================
void test_resample_16000()
{
    std::cout << "Running test_resample_16000..." << std::endl;
    constexpr int TARGET_RATE = 16000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.load_file(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_samples();
    size_t num_samples = samples.empty() ? 0 : samples[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_16000;
    std::cout << "sample_rate: " << meta.sample_rate << " -> " << TARGET_RATE 
              << ", num_samples: " << num_samples << ", diff: " << diff << std::endl;

    CHECK(is_within_tolerance(num_samples, EXPECTED_SAMPLES_16000));
}

//=============================================================================
// Test: Resample to 32000 Hz
//=============================================================================
void test_resample_32000()
{
    std::cout << "Running test_resample_32000..." << std::endl;
    constexpr int TARGET_RATE = 32000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.load_file(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_samples();
    size_t num_samples = samples.empty() ? 0 : samples[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_32000;
    std::cout << "sample_rate: " << meta.sample_rate << " -> " << TARGET_RATE 
              << ", num_samples: " << num_samples << ", diff: " << diff << std::endl;

    CHECK(is_within_tolerance(num_samples, EXPECTED_SAMPLES_32000));
}

//=============================================================================
// Test: No resample (44100 Hz - same as source)
//=============================================================================
void test_resample_44100()
{
    std::cout << "Running test_resample_44100..." << std::endl;
    constexpr int TARGET_RATE = 44100;

    AudioDecoder decoder({TARGET_RATE});
    decoder.load_file(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_samples();
    size_t num_samples = samples.empty() ? 0 : samples[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_44100;
    std::cout << "sample_rate: " << meta.sample_rate << " -> " << TARGET_RATE 
              << ", num_samples: " << num_samples << ", diff: " << diff << std::endl;

    CHECK((int)num_samples == EXPECTED_SAMPLES_44100);
}

//=============================================================================
// Test: Resample to 48000 Hz
//=============================================================================
void test_resample_48000()
{
    std::cout << "Running test_resample_48000..." << std::endl;
    constexpr int TARGET_RATE = 48000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.load_file(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_samples();
    size_t num_samples = samples.empty() ? 0 : samples[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_48000;
    std::cout << "sample_rate: " << meta.sample_rate << " -> " << TARGET_RATE 
              << ", num_samples: " << num_samples << ", diff: " << diff << std::endl;

    CHECK(is_within_tolerance(num_samples, EXPECTED_SAMPLES_48000));
}

//=============================================================================
// Test: Verify audio quality after resampling (check for NaN/Inf)
//=============================================================================
void test_resample_audio_quality()
{
    std::cout << "Running test_resample_audio_quality..." << std::endl;
    constexpr int TARGET_RATE = 16000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.load_file(TEST_FILE_PATH);

    auto samples = decoder.get_samples();
    size_t num_samples = samples.empty() ? 0 : samples[0].size();

    CHECK((int)samples.size() == EXPECTED_NUM_CHANNELS);
    CHECK(num_samples > 0);

    // Check first 1000 samples of each channel
    for (size_t c = 0; c < samples.size(); ++c)
    {
        for (size_t i = 0; i < std::min(num_samples, (size_t)1000); ++i)
        {
            float sample = samples[c][i];
            (void)sample;
            CHECK(!std::isnan(sample));
            CHECK(!std::isinf(sample));
            CHECK(sample >= -2.0f && sample <= 2.0f);
        }
    }
}

//=============================================================================
// Main Test Runner
//=============================================================================
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    std::cout << "\n=== avioflow Decoder Resample Tests (get_samples) ===" << std::endl;

    // Check if test file exists
    std::ifstream check_file(TEST_FILE_PATH);
    bool file_exists = check_file.good();
    check_file.close();

    if (!file_exists)
    {
        std::cerr << "\n[ERROR] Test file not found: " << TEST_FILE_PATH << std::endl;
        return 1;
    }

    test_resample_8000();
    test_resample_16000();
    test_resample_32000();
    test_resample_44100();
    test_resample_48000();
    test_resample_audio_quality();

    std::cout << "All resample tests passed!" << std::endl;

    return 0;
}
