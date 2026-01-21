// Unit tests for SingleStreamDecoder - Resampling
// Tests cover: output sample rate conversion to common rates using get_all_samples()

#include "avioflow-cxx-api.h"
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <format>

using namespace avioflow;

// Test file paths
const std::string TEST_FILE_PATH = "./public/TownTheme.mp3";

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

// Allow 1% tolerance for resampling variations
constexpr double TOLERANCE = 0.01;

//=============================================================================
// Helper: Check if sample count is within tolerance
//=============================================================================
bool is_within_tolerance(size_t actual, int expected)
{
    double ratio = static_cast<double>(actual) / expected;
    return ratio > (1.0 - TOLERANCE) && ratio < (1.0 + TOLERANCE);
}

//=============================================================================
// Test: Resample to 8000 Hz
//=============================================================================
void test_resample_8000()
{
    std::cout << "Running test_resample_8000..." << std::endl;
    constexpr int TARGET_RATE = 8000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);

    // Verify source metadata unchanged
    const auto &meta = decoder.get_metadata();
    assert(meta.sample_rate == ORIGINAL_SAMPLE_RATE);
    assert(meta.num_channels == EXPECTED_NUM_CHANNELS);

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_8000;
    std::cout << "sample_rate: " << meta.sample_rate << " -> " << TARGET_RATE 
              << ", num_samples: " << num_samples << ", diff: " << diff << std::endl;

    assert(samples.sample_rate == TARGET_RATE);
    assert((int)samples.data.size() == EXPECTED_NUM_CHANNELS);
    assert(is_within_tolerance(num_samples, EXPECTED_SAMPLES_8000));
}

//=============================================================================
// Test: Resample to 16000 Hz
//=============================================================================
void test_resample_16000()
{
    std::cout << "Running test_resample_16000..." << std::endl;
    constexpr int TARGET_RATE = 16000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_16000;
    std::cout << "sample_rate: " << meta.sample_rate << " -> " << TARGET_RATE 
              << ", num_samples: " << num_samples << ", diff: " << diff << std::endl;

    assert(samples.sample_rate == TARGET_RATE);
    assert(is_within_tolerance(num_samples, EXPECTED_SAMPLES_16000));
}

//=============================================================================
// Test: Resample to 32000 Hz
//=============================================================================
void test_resample_32000()
{
    std::cout << "Running test_resample_32000..." << std::endl;
    constexpr int TARGET_RATE = 32000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_32000;
    std::cout << "sample_rate: " << meta.sample_rate << " -> " << TARGET_RATE 
              << ", num_samples: " << num_samples << ", diff: " << diff << std::endl;

    assert(samples.sample_rate == TARGET_RATE);
    assert(is_within_tolerance(num_samples, EXPECTED_SAMPLES_32000));
}

//=============================================================================
// Test: No resample (44100 Hz - same as source)
//=============================================================================
void test_resample_44100()
{
    std::cout << "Running test_resample_44100..." << std::endl;
    constexpr int TARGET_RATE = 44100;

    AudioDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_44100;
    std::cout << std::format("sample_rate: {} -> {},  num_samples: {}, diff: {}\n", meta.sample_rate, TARGET_RATE, num_samples, diff);

    assert(samples.sample_rate == TARGET_RATE);
    assert((int)num_samples == EXPECTED_SAMPLES_44100);
}

//=============================================================================
// Test: Resample to 48000 Hz
//=============================================================================
void test_resample_48000()
{
    std::cout << "Running test_resample_48000..." << std::endl;
    constexpr int TARGET_RATE = 48000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_48000;
    std::cout << "sample_rate: " << meta.sample_rate << " -> " << TARGET_RATE 
              << ", num_samples: " << num_samples << ", diff: " << diff << std::endl;

    assert(samples.sample_rate == TARGET_RATE);
    assert(is_within_tolerance(num_samples, EXPECTED_SAMPLES_48000));
}

//=============================================================================
// Test: Verify audio quality after resampling (check for NaN/Inf)
//=============================================================================
void test_resample_audio_quality()
{
    std::cout << "Running test_resample_audio_quality..." << std::endl;
    constexpr int TARGET_RATE = 16000;

    AudioDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();

    assert((int)samples.data.size() == EXPECTED_NUM_CHANNELS);
    assert(num_samples > 0);

    // Check first 1000 samples of each channel
    for (size_t c = 0; c < samples.data.size(); ++c)
    {
        for (size_t i = 0; i < std::min(num_samples, (size_t)1000); ++i)
        {
            float sample = samples.data[c][i];
            (void)sample;
            assert(!std::isnan(sample));
            assert(!std::isinf(sample));
            assert(sample >= -2.0f && sample <= 2.0f);
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

    std::cout << "\n=== avioflow Decoder Resample Tests (get_all_samples) ===" << std::endl;

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
