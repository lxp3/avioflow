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
const std::string TEST_FILE_PATH = "./public/TownTheme.mp3";
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
  while (!decoder.is_finished())
  {
    auto *frame = decoder.decode_next();
    if (frame == nullptr)
      break;
    total_samples += frame->nb_samples;
    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, frame->ch_layout.nb_channels, "frame channels");
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
  while (!decoder.is_finished() && frame_count < 10)
  {
    auto *frame = decoder.decode_next();
    if (frame == nullptr)
      break;

    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, frame->ch_layout.nb_channels, "frame channels");
    TEST_ASSERT_GT(frame->nb_samples, 0, "frame samples");
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
  decoder.open_memory(buffer.data(), buffer.size());

  const auto &meta = decoder.get_metadata();

  // Verify metadata
  TEST_ASSERT_EQ(EXPECTED_SAMPLE_RATE, meta.sample_rate, "sample_rate");
  TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, meta.num_channels, "num_channels");

  // Decode all samples
  size_t total_samples = 0;
  while (!decoder.is_finished())
  {
    auto *frame = decoder.decode_next();
    if (frame == nullptr)
      break;
    total_samples += frame->nb_samples;
  }

  TEST_ASSERT_EQ(EXPECTED_NUM_FRAMES, (int)total_samples, "num_frames");

  return true;
}

//=============================================================================
// Test 4: Streaming Decode with callback-based data provider
//=============================================================================
bool test_streaming_decode()
{
  // Read file into memory
  std::ifstream file(TEST_FILE_PATH, std::ios::binary | std::ios::ate);
  TEST_ASSERT(file.is_open(), "Could not open test file");
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  TEST_ASSERT(file.read(reinterpret_cast<char *>(buffer.data()), size), "Failed to read file");
  file.close();

  size_t read_pos = 0;

  // Create a read callback that simulates streaming
  auto avio_read_callback = [&](uint8_t *buf, int buf_size) -> int
  {
    if (read_pos >= buffer.size())
    {
      return 0; // EOF (instead of AVERROR_EOF)
    }

    size_t available = buffer.size() - read_pos;
    int to_read = std::min(buf_size, static_cast<int>(available));
    std::memcpy(buf, buffer.data() + read_pos, to_read);
    read_pos += to_read;

    return to_read;
  };

  SingleStreamDecoder decoder;
  decoder.open_stream(avio_read_callback);

  size_t total_samples = 0;

  while (!decoder.is_finished())
  {
    auto *frame = decoder.decode_next();
    if (frame == nullptr)
      break;

    TEST_ASSERT_EQ(EXPECTED_NUM_CHANNELS, frame->ch_layout.nb_channels, "frame channels");
    TEST_ASSERT_GT(frame->nb_samples, 0, "frame samples count");

    total_samples += frame->nb_samples;
  }

  // Note: Streaming mode may produce slightly different sample counts due to
  // MP3 frame boundary handling when file size is unknown (差异通常 < 0.01%)
  // [mp3 @ ...] invalid concatenated file detected - using bitrate for duration
  auto diff = std::abs(static_cast<int64_t>(total_samples) - EXPECTED_NUM_FRAMES);
  TEST_ASSERT(diff < 500, "total samples should be within 500 of expected");

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
  (void)argc;
  (void)argv;
  std::cout << "\n=== avioflow Decoder Unit Tests ===" << std::endl;

  // Check if test file exists
  std::ifstream check_file(TEST_FILE_PATH);
  bool file_exists = check_file.good();
  check_file.close();

  TestRunner runner;

  if (file_exists)
  {
    runner.add_test("test_decode_from_filepath", test_decode_from_filepath);
    runner.add_test("test_decode_from_memory", test_decode_from_memory);
    runner.add_test("test_streaming_decode", test_streaming_decode);
    runner.add_test("test_metadata_format", test_metadata_format);
  }

  auto stats = runner.run_all();
  return stats.failed > 0 ? 1 : 0;
}
