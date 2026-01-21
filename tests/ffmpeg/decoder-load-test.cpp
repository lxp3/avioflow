#include "avioflow-cxx-api.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

using namespace avioflow;

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
void test_decode_from_filepath()
{
  std::cout << "Running test_decode_from_filepath..." << std::endl;
  AudioDecoder decoder;
  const auto &meta = decoder.get_metadata();
  (void)meta;

  // Verify metadata
  assert(meta.sample_rate == EXPECTED_SAMPLE_RATE);
  assert(meta.num_channels == EXPECTED_NUM_CHANNELS);
  assert(std::fabs(meta.duration - EXPECTED_DURATION) < 1.0);

  // Decode all samples
  size_t total_samples = 0;
  while (!decoder.is_finished())
  {
    auto samples = decoder.decode_next();
    if (samples.data.empty())
      break;
    total_samples += samples.data[0].size();
    assert((int)samples.data.size() == EXPECTED_NUM_CHANNELS);
  }

  assert((int)total_samples == EXPECTED_NUM_FRAMES);
}

//=============================================================================
// Test 2: URL Decode
//=============================================================================
void test_decode_from_url()
{
  std::cout << "Running test_decode_from_url..." << std::endl;
  AudioDecoder decoder;
  decoder.open(TEST_URL);

  const auto &meta = decoder.get_metadata();
  (void)meta;

  // Verify metadata
  assert(meta.sample_rate == EXPECTED_SAMPLE_RATE);
  assert(meta.num_channels == EXPECTED_NUM_CHANNELS);

  // For URL, just decode first few frames to verify it works
  int frame_count = 0;
  while (!decoder.is_finished() && frame_count < 10)
  {
    auto samples = decoder.decode_next();
    if (samples.data.empty())
      break;

    assert((int)samples.data.size() == EXPECTED_NUM_CHANNELS);
    assert((int)samples.data[0].size() > 0);
    frame_count++;
  }

  assert(frame_count > 0);
}

//=============================================================================
// Test 3: Memory Decode
//=============================================================================
void test_decode_from_memory()
{
  std::cout << "Running test_decode_from_memory..." << std::endl;
  // Read file into memory
  std::ifstream file(TEST_FILE_PATH, std::ios::binary | std::ios::ate);
  assert(file.is_open());

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(size);
  assert(file.read(reinterpret_cast<char *>(buffer.data()), size));
  file.close();

  // Decode from memory
  AudioDecoder decoder;
  decoder.open_memory(buffer.data(), buffer.size());

  const auto &meta = decoder.get_metadata();
  (void)meta;

  // Verify metadata
  assert(meta.sample_rate == EXPECTED_SAMPLE_RATE);
  assert(meta.num_channels == EXPECTED_NUM_CHANNELS);

  // Decode all samples
  size_t total_samples = 0;
  while (!decoder.is_finished())
  {
    auto samples = decoder.decode_next();
    if (samples.data.empty())
      break;
    total_samples += samples.data[0].size();
  }

  assert((int)total_samples == EXPECTED_NUM_FRAMES);
}

//=============================================================================
// Test 4: Streaming Decode with callback-based data provider
//=============================================================================
void test_streaming_decode()
{
  std::cout << "Running test_streaming_decode..." << std::endl;
  // Read file into memory
  std::ifstream file(TEST_FILE_PATH, std::ios::binary | std::ios::ate);
  assert(file.is_open());
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  assert(file.read(reinterpret_cast<char *>(buffer.data()), size));
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

  AudioDecoder decoder;
  decoder.open_stream(avio_read_callback);

  size_t total_samples = 0;

  while (!decoder.is_finished())
  {
    auto samples = decoder.decode_next();
    if (samples.data.empty())
      break;

    assert((int)samples.data.size() == EXPECTED_NUM_CHANNELS);
    assert((int)samples.data[0].size() > 0);

    total_samples += samples.data[0].size();
  }

  auto diff = std::abs(static_cast<int64_t>(total_samples) - EXPECTED_NUM_FRAMES);
  (void)diff;
  assert(diff < 500);
}

//=============================================================================
// Test 5: Metadata Verification
//=============================================================================
void test_metadata_format()
{
  std::cout << "Running test_metadata_format..." << std::endl;
  AudioDecoder decoder;
  decoder.open(TEST_FILE_PATH);

  const auto &metadata = decoder.get_metadata();
  (void)metadata;

  // MP3 container should be detected
  assert(metadata.container.find("mp3") != std::string::npos);
  assert(metadata.sample_rate == EXPECTED_SAMPLE_RATE);
  assert(metadata.num_channels == EXPECTED_NUM_CHANNELS);
  assert(metadata.duration > 90.0);
  assert(metadata.duration < 100.0);
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

  if (file_exists)
  {
    test_decode_from_filepath();
    test_decode_from_memory();
    test_streaming_decode();
    test_metadata_format();
    std::cout << "All tests passed!" << std::endl;
  }
  else
  {
    std::cout << "Test file not found, skipping local tests." << std::endl;
  }

  return 0;
}
