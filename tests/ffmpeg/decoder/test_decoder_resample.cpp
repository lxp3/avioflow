// Unit tests for SingleStreamDecoder - Resampling
// Tests cover: output sample rate conversion to common rates

#include "single-stream-decoder.h"
#include "test_framework.h"
#include <cmath>
#include <fstream>

using namespace avioflow;
using namespace avioflow::test;

// Test file paths
const std::string TEST_FILE_PATH = "./TownTheme.mp3";

// Original file parameters
constexpr int ORIGINAL_SAMPLE_RATE = 44100;
constexpr int ORIGINAL_NUM_SAMPLES = 4297722;
constexpr int EXPECTED_NUM_CHANNELS = 2;

// Expected sample counts after resampling (provided by user)
// sample rate: 44100 -> 8000, num samples: 779632
// sample rate: 44100 -> 16000, num samples: 1559264
// sample rate: 44100 -> 32000, num samples: 3118529
// sample rate: 44100 -> 44100, num samples: 4297722
// sample rate: 44100 -> 48000, num samples: 4677793

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

    SingleStreamDecoder decoder(TARGET_RATE);
    decoder.open(TEST_FILE_PATH);

    // Verify source metadata unchanged
    const auto &meta = decoder.get_metadata();
    TEST_ASSERT_EQ(ORIGINAL_SAMPLE_RATE, meta.sample_rate, "source sample_rate");
    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, meta.num_channels, "source num_channels");

    size_t total_samples = 0;
    while (decoder.has_more())
    {
        auto frame = decoder.decode_next();
        if (frame.data == nullptr)
            break;

        // Verify output sample rate in frame
        TEST_ASSERT_EQ(TARGET_RATE, frame.sample_rate, "output sample_rate");
        TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, frame.num_channels, "frame channels");
        total_samples += frame.num_samples;
    }

    TEST_ASSERT(is_within_tolerance(total_samples, EXPECTED_SAMPLES_8000),
                "sample count within tolerance");

    std::cout << "    [INFO] 44100 -> " << TARGET_RATE << " Hz: " << total_samples
              << " samples (expected: " << EXPECTED_SAMPLES_8000 << ")" << std::endl;

    return true;
}

//=============================================================================
// Test: Resample to 16000 Hz
//=============================================================================
bool test_resample_16000()
{
    constexpr int TARGET_RATE = 16000;

    SingleStreamDecoder decoder(TARGET_RATE);
    decoder.open(TEST_FILE_PATH);

    size_t total_samples = 0;
    while (decoder.has_more())
    {
        auto frame = decoder.decode_next();
        if (frame.data == nullptr)
            break;
        TEST_ASSERT_EQ(TARGET_RATE, frame.sample_rate, "output sample_rate");
        total_samples += frame.num_samples;
    }

    TEST_ASSERT(is_within_tolerance(total_samples, EXPECTED_SAMPLES_16000),
                "sample count within tolerance");

    std::cout << "    [INFO] 44100 -> " << TARGET_RATE << " Hz: " << total_samples
              << " samples (expected: " << EXPECTED_SAMPLES_16000 << ")" << std::endl;

    return true;
}

//=============================================================================
// Test: Resample to 32000 Hz
//=============================================================================
bool test_resample_32000()
{
    constexpr int TARGET_RATE = 32000;

    SingleStreamDecoder decoder(TARGET_RATE);
    decoder.open(TEST_FILE_PATH);

    size_t total_samples = 0;
    while (decoder.has_more())
    {
        auto frame = decoder.decode_next();
        if (frame.data == nullptr)
            break;
        TEST_ASSERT_EQ(TARGET_RATE, frame.sample_rate, "output sample_rate");
        total_samples += frame.num_samples;
    }

    TEST_ASSERT(is_within_tolerance(total_samples, EXPECTED_SAMPLES_32000),
                "sample count within tolerance");

    std::cout << "    [INFO] 44100 -> " << TARGET_RATE << " Hz: " << total_samples
              << " samples (expected: " << EXPECTED_SAMPLES_32000 << ")" << std::endl;

    return true;
}

//=============================================================================
// Test: No resample (44100 Hz - same as source)
//=============================================================================
bool test_resample_44100()
{
    constexpr int TARGET_RATE = 44100;

    SingleStreamDecoder decoder(TARGET_RATE);
    decoder.open(TEST_FILE_PATH);

    size_t total_samples = 0;
    while (decoder.has_more())
    {
        auto frame = decoder.decode_next();
        if (frame.data == nullptr)
            break;
        TEST_ASSERT_EQ(TARGET_RATE, frame.sample_rate, "output sample_rate");
        total_samples += frame.num_samples;
    }

    // Should be exact match since no resampling
    TEST_ASSERT_EQ(EXPECTED_SAMPLES_44100, (int)total_samples, "exact sample count");

    std::cout << "    [INFO] 44100 -> " << TARGET_RATE << " Hz: " << total_samples
              << " samples (expected: " << EXPECTED_SAMPLES_44100 << ")" << std::endl;

    return true;
}

//=============================================================================
// Test: Resample to 48000 Hz
//=============================================================================
bool test_resample_48000()
{
    constexpr int TARGET_RATE = 48000;

    SingleStreamDecoder decoder(TARGET_RATE);
    decoder.open(TEST_FILE_PATH);

    size_t total_samples = 0;
    while (decoder.has_more())
    {
        auto frame = decoder.decode_next();
        if (frame.data == nullptr)
            break;
        TEST_ASSERT_EQ(TARGET_RATE, frame.sample_rate, "output sample_rate");
        total_samples += frame.num_samples;
    }

    TEST_ASSERT(is_within_tolerance(total_samples, EXPECTED_SAMPLES_48000),
                "sample count within tolerance");

    std::cout << "    [INFO] 44100 -> " << TARGET_RATE << " Hz: " << total_samples
              << " samples (expected: " << EXPECTED_SAMPLES_48000 << ")" << std::endl;

    return true;
}

//=============================================================================
// Test: Verify audio quality after resampling (check for NaN/Inf)
//=============================================================================
bool test_resample_audio_quality()
{
    constexpr int TARGET_RATE = 16000;

    SingleStreamDecoder decoder(TARGET_RATE);
    decoder.open(TEST_FILE_PATH);

    int frame_count = 0;
    while (decoder.has_more() && frame_count < 100)
    {
        auto frame = decoder.decode_next();
        if (frame.data == nullptr)
            break;

        const float *samples = reinterpret_cast<const float *>(frame.data);
        for (int i = 0; i < std::min(frame.num_samples, 100); ++i)
        {
            TEST_ASSERT(!std::isnan(samples[i]), "sample is NaN");
            TEST_ASSERT(!std::isinf(samples[i]), "sample is Inf");
            TEST_ASSERT(samples[i] >= -2.0f && samples[i] <= 2.0f,
                        "sample in valid range");
        }

        frame_count++;
    }

    TEST_ASSERT_GT(frame_count, 0, "decoded frames");

    return true;
}

//=============================================================================
// Main Test Runner
//=============================================================================
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    std::cout << "\n=== avioflow Decoder Resample Tests ===" << std::endl;
    std::cout << "Test file: " << TEST_FILE_PATH << std::endl;
    std::cout << "Source: " << ORIGINAL_SAMPLE_RATE << " Hz, "
              << ORIGINAL_NUM_SAMPLES << " samples" << std::endl;
    std::cout << "Testing sample rates: 8000, 16000, 32000, 44100, 48000 Hz"
              << std::endl;

    // Check if test file exists
    std::ifstream check_file(TEST_FILE_PATH);
    bool file_exists = check_file.good();
    check_file.close();

    if (!file_exists)
    {
        std::cerr << "\n[ERROR] Test file not found: " << TEST_FILE_PATH
                  << std::endl;
        std::cerr << "Please ensure TownTheme.mp3 is in the current directory."
                  << std::endl;
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
