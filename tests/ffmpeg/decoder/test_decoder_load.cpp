// Unit tests for SingleStreamDecoder
// Tests cover: URL, file path, memory, and streaming decode

#include "single-stream-decoder.h"
#include "test_framework.h"
#include <cmath>
#include <fstream>

using namespace avioflow;
using namespace avioflow::test;

// Expected metadata for TownTheme.mp3
constexpr int EXPECTED_SAMPLE_RATE = 44100;
constexpr int EXPECTED_NUM_CHANNELS = 2;
constexpr int EXPECTED_NUM_FRAMES = 4297722;
constexpr double EXPECTED_DURATION = 97.489; // seconds (approximate)

// Test file paths
const std::string TEST_FILE_PATH = "./TownTheme.mp3";
const std::string TEST_URL =
    "https://opengameart.org/sites/default/files/TownTheme.mp3";

//=============================================================================
// Test 1: File Path Decode
//=============================================================================
bool test_decode_from_filepath()
{
  SingleStreamDecoder decoder;
  decoder.open(TEST_FILE_PATH);

  const auto &meta = decoder.get_metadata();

  // Verify metadata
  TEST_ASSERT_EQ(EXPECTED_SAMPLE_RATE, meta.sample_rate, "sample_rate");
  TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, meta.num_channels, "num_channels");
  TEST_ASSERT_NEAR(EXPECTED_DURATION, meta.duration, 1.0, "duration");

  // Decode all samples
  size_t total_samples = 0;
  while (decoder.has_more())
  {
    auto frame = decoder.decode_next();
    if (frame.data == nullptr)
      break;
    total_samples += frame.num_samples;
    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, frame.num_channels, "frame channels");
  }

  TEST_ASSERT_EQ(EXPECTED_NUM_FRAMES, (int)total_samples, "num_frames");

  return true;
}

//=============================================================================
// Test 2: URL Decode
//=============================================================================
bool test_decode_from_url()
{
  SingleStreamDecoder decoder;
  decoder.open(TEST_URL);

  const auto &meta = decoder.get_metadata();

  // Verify metadata
  TEST_ASSERT_EQ(EXPECTED_SAMPLE_RATE, meta.sample_rate, "sample_rate");
  TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, meta.num_channels, "num_channels");

  // For URL, just decode first few frames to verify it works
  int frame_count = 0;
  while (decoder.has_more() && frame_count < 10)
  {
    auto frame = decoder.decode_next();
    if (frame.data == nullptr)
      break;

    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, frame.num_channels, "frame channels");
    TEST_ASSERT_GT(frame.num_samples, 0, "frame samples");
    frame_count++;
  }

  TEST_ASSERT_GT(frame_count, 0, "decoded frames from URL");

  return true;
}

//=============================================================================
// Test 3: Memory Decode
//=============================================================================
bool test_decode_from_memory()
{
  // Read file into memory
  std::ifstream file(TEST_FILE_PATH, std::ios::binary | std::ios::ate);
  TEST_ASSERT(file.is_open(), "Could not open test file for memory test");

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(size);
  TEST_ASSERT(file.read(reinterpret_cast<char *>(buffer.data()), size),
              "Failed to read file");
  file.close();

  // Decode from memory
  SingleStreamDecoder decoder;
  decoder.open(buffer.data(), buffer.size());

  const auto &meta = decoder.get_metadata();

  // Verify metadata
  TEST_ASSERT_EQ(EXPECTED_SAMPLE_RATE, meta.sample_rate, "sample_rate");
  TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, meta.num_channels, "num_channels");

  // Decode all samples
  size_t total_samples = 0;
  while (decoder.has_more())
  {
    auto frame = decoder.decode_next();
    if (frame.data == nullptr)
      break;
    total_samples += frame.num_samples;
  }

  TEST_ASSERT_EQ(EXPECTED_NUM_FRAMES, (int)total_samples, "num_frames");

  return true;
}

//=============================================================================
// Test 4: Streaming Decode with FrameOutput
//=============================================================================
bool test_streaming_decode()
{
  SingleStreamDecoder decoder;
  decoder.open(TEST_FILE_PATH);

  const auto &meta = decoder.get_metadata();
  TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, meta.num_channels, "num_channels");

  size_t total_samples = 0;
  int frame_count = 0;

  while (decoder.has_more())
  {
    auto frame = decoder.decode_next();
    if (frame.data == nullptr)
      break;

    // Verify frame structure
    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, frame.num_channels, "frame channels");
    TEST_ASSERT_NOT_NULL(frame.data, "frame data pointer");
    TEST_ASSERT_GT(frame.num_samples, 0, "frame samples count");

    // Cast to float* for FLTP format (planar float)
    // For planar audio, each channel is stored separately
    // The data pointer points to channel 0
    const float *channel_data = reinterpret_cast<const float *>(frame.data);

    // Verify samples are valid floats (not NaN or Inf)
    for (int i = 0; i < std::min(frame.num_samples, 10); ++i)
    {
      TEST_ASSERT(!std::isnan(channel_data[i]), "sample is NaN");
      TEST_ASSERT(!std::isinf(channel_data[i]), "sample is Inf");
      // Audio samples should be in range [-1.0, 1.0] for normalized float
      TEST_ASSERT(channel_data[i] >= -2.0f && channel_data[i] <= 2.0f,
                  "sample out of reasonable range");
    }

    total_samples += frame.num_samples;
    frame_count++;
  }

  // Verify total decoded samples
  TEST_ASSERT_EQ(EXPECTED_NUM_FRAMES, (int)total_samples, "total samples");
  TEST_ASSERT_GT(frame_count, 0, "frame count");

  std::cout << "    [INFO] Decoded " << frame_count << " frames, "
            << total_samples << " samples total" << std::endl;

  return true;
}

//=============================================================================
// Test 5: Metadata Verification
//=============================================================================
bool test_metadata_format()
{
  SingleStreamDecoder decoder;
  decoder.open(TEST_FILE_PATH);

  const auto &meta = decoder.get_metadata();

  // MP3 format should be detected
  TEST_ASSERT(meta.sample_format.find("mp3") != std::string::npos,
              "format should contain 'mp3'");
  TEST_ASSERT_EQ(EXPECTED_SAMPLE_RATE, meta.sample_rate, "sample_rate");
  TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, meta.num_channels, "num_channels");
  TEST_ASSERT_GT(meta.duration, 90.0, "duration > 90s");
  TEST_ASSERT(meta.duration < 100.0, "duration < 100s");

  return true;
}

//=============================================================================
// Main Test Runner
//=============================================================================
int main(int argc, char **argv)
{
  std::cout << "\n=== avioflow Decoder Unit Tests ===" << std::endl;
  std::cout << "Test file: " << TEST_FILE_PATH << std::endl;
  std::cout << "Test URL: " << TEST_URL << std::endl;

  // Check if test file exists
  std::ifstream check_file(TEST_FILE_PATH);
  bool file_exists = check_file.good();
  check_file.close();

  if (!file_exists)
  {
    std::cerr << "\n[WARNING] Test file not found: " << TEST_FILE_PATH
              << std::endl;
    std::cerr << "Please download: " << TEST_URL << std::endl;
    std::cerr << "Or run: curl -o TownTheme.mp3 \"" << TEST_URL << "\""
              << std::endl;
  }

  TestRunner runner;

  // File-based tests (require local file)
  if (file_exists)
  {
    runner.add_test("test_decode_from_filepath", test_decode_from_filepath);
    runner.add_test("test_decode_from_memory", test_decode_from_memory);
    runner.add_test("test_streaming_decode", test_streaming_decode);
    runner.add_test("test_metadata_format", test_metadata_format);
  }
  else
  {
    runner.add_test_skip("test_decode_from_filepath", test_decode_from_filepath,
                         "test file not found");
    runner.add_test_skip("test_decode_from_memory", test_decode_from_memory,
                         "test file not found");
    runner.add_test_skip("test_streaming_decode", test_streaming_decode,
                         "test file not found");
    runner.add_test_skip("test_metadata_format", test_metadata_format,
                         "test file not found");
  }

  // URL test (requires network, can be skipped with --skip-network flag)
  bool skip_network = false;
  for (int i = 1; i < argc; ++i)
  {
    if (std::string(argv[i]) == "--skip-network")
    {
      skip_network = true;
    }
  }

  if (skip_network)
  {
    runner.add_test_skip("test_decode_from_url", test_decode_from_url,
                         "network tests disabled");
  }
  else
  {
    runner.add_test("test_decode_from_url", test_decode_from_url);
  }

  auto stats = runner.run_all();

  return stats.failed > 0 ? 1 : 0;
}
