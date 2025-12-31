// Unit tests for SingleStreamDecoder - Resampling
// Tests cover: output sample rate conversion to common rates using get_all_samples()

#include "single-stream-decoder.h"
#include "test_framework.h"
#include <cmath>
#include <fstream>

using namespace avioflow;
using namespace avioflow::test;

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
bool test_resample_8000()
{
    constexpr int TARGET_RATE = 8000;

    SingleStreamDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);

    // Verify source metadata unchanged
    const auto &meta = decoder.get_metadata();
    TEST_ASSERT_EQ(ORIGINAL_SAMPLE_RATE, meta.sample_rate, "source sample_rate");
    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, meta.num_channels, "source num_channels");

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_8000;
    printf(std::format("sample_rate: {} -> {},  num_samples: {}, diff: {}\n", meta.sample_rate, TARGET_RATE, num_samples, diff).c_str());

    TEST_ASSERT_EQ(TARGET_RATE, samples.sample_rate, "output sample_rate");
    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, (int)samples.data.size(), "num_channels");
    TEST_ASSERT(is_within_tolerance(num_samples, EXPECTED_SAMPLES_8000),
                "sample count within tolerance");

    return true;
}

//=============================================================================
// Test: Resample to 16000 Hz
//=============================================================================
bool test_resample_16000()
{
    constexpr int TARGET_RATE = 16000;

    SingleStreamDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_16000;
    printf(std::format("sample_rate: {} -> {},  num_samples: {}, diff: {}\n", meta.sample_rate, TARGET_RATE, num_samples, diff).c_str());

    TEST_ASSERT_EQ(TARGET_RATE, samples.sample_rate, "output sample_rate");
    TEST_ASSERT(is_within_tolerance(num_samples, EXPECTED_SAMPLES_16000),
                "sample count within tolerance");

    return true;
}

//=============================================================================
// Test: Resample to 32000 Hz
//=============================================================================
bool test_resample_32000()
{
    constexpr int TARGET_RATE = 32000;

    SingleStreamDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_32000;
    printf(std::format("sample_rate: {} -> {},  num_samples: {}, diff: {}\n", meta.sample_rate, TARGET_RATE, num_samples, diff).c_str());

    TEST_ASSERT_EQ(TARGET_RATE, samples.sample_rate, "output sample_rate");
    TEST_ASSERT(is_within_tolerance(num_samples, EXPECTED_SAMPLES_32000),
                "sample count within tolerance");

    return true;
}

//=============================================================================
// Test: No resample (44100 Hz - same as source)
//=============================================================================
bool test_resample_44100()
{
    constexpr int TARGET_RATE = 44100;

    SingleStreamDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_44100;
    printf(std::format("sample_rate: {} -> {},  num_samples: {}, diff: {}\n", meta.sample_rate, TARGET_RATE, num_samples, diff).c_str());

    TEST_ASSERT_EQ(TARGET_RATE, samples.sample_rate, "output sample_rate");
    TEST_ASSERT_EQ(EXPECTED_SAMPLES_44100, (int)num_samples, "exact sample count");

    return true;
}

//=============================================================================
// Test: Resample to 48000 Hz
//=============================================================================
bool test_resample_48000()
{
    constexpr int TARGET_RATE = 48000;

    SingleStreamDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);
    const auto &meta = decoder.get_metadata();

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();
    auto diff = static_cast<int64_t>(num_samples) - EXPECTED_SAMPLES_48000;
    printf(std::format("sample_rate: {} -> {},  num_samples: {}, diff: {}\n", meta.sample_rate, TARGET_RATE, num_samples, diff).c_str());

    TEST_ASSERT_EQ(TARGET_RATE, samples.sample_rate, "output sample_rate");
    TEST_ASSERT(is_within_tolerance(num_samples, EXPECTED_SAMPLES_48000),
                "sample count within tolerance");

    return true;
}

//=============================================================================
// Test: Verify audio quality after resampling (check for NaN/Inf)
//=============================================================================
bool test_resample_audio_quality()
{
    constexpr int TARGET_RATE = 16000;

    SingleStreamDecoder decoder({TARGET_RATE});
    decoder.open(TEST_FILE_PATH);

    auto samples = decoder.get_all_samples();
    size_t num_samples = samples.data.empty() ? 0 : samples.data[0].size();

    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, (int)samples.data.size(), "channels count");
    TEST_ASSERT_GT((int)num_samples, 0, "has samples");

    // Check first 1000 samples of each channel
    for (size_t c = 0; c < samples.data.size(); ++c)
    {
        for (size_t i = 0; i < std::min(num_samples, (size_t)1000); ++i)
        {
            float sample = samples.data[c][i];
            TEST_ASSERT(!std::isnan(sample), "sample is NaN");
            TEST_ASSERT(!std::isinf(sample), "sample is Inf");
            TEST_ASSERT(sample >= -2.0f && sample <= 2.0f, "sample in valid range");
        }
    }

    return true;
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

    TestRunner runner;

    runner.add_test("test_resample_8000", test_resample_8000);
    runner.add_test("test_resample_16000", test_resample_16000);
    runner.add_test("test_resample_32000", test_resample_32000);
    runner.add_test("test_resample_44100", test_resample_44100);
    runner.add_test("test_resample_48000", test_resample_48000);
    runner.add_test("test_resample_audio_quality", test_resample_audio_quality);

    auto stats = runner.run_all();

    return stats.failed > 0 ? 1 : 0;
}
