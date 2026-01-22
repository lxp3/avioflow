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
const std::string MP3_PATH = "./public/TownTheme.mp3";
const std::string MP3_URL =
    "https://opengameart.org/sites/default/files/TownTheme.mp3";
const std::string WAV_PATH = "./public/zh.wav";

// Helper function to print metadata
void print_metadata(const Metadata& meta, const std::string& test_name) {
  std::cout << "[" << test_name << "] Metadata:" << std::endl;
  std::cout << "  Sample Rate: " << meta.sample_rate << " Hz" << std::endl;
  std::cout << "  Channels: " << meta.num_channels << std::endl;
  std::cout << "  Codec: " << meta.codec << std::endl;
  std::cout << "  Container: " << meta.container << std::endl;
  std::cout << "  Duration: " << meta.duration << " seconds" << std::endl;
  std::cout << "  Num Samples: " << meta.num_samples << std::endl;
}

//=============================================================================
// Test 1: File Path Decode
//=============================================================================
void test_decode_from_filepath()
{
  std::cout << "\n=== Running test_decode_from_filepath ===" << std::endl;
  std::cout << "File: " << MP3_PATH << std::endl;
  AudioDecoder decoder;
  decoder.open(MP3_PATH);
  
  const auto &meta = decoder.get_metadata();
  print_metadata(meta, "test_decode_from_filepath");

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
  std::cout << "\n=== Running test_decode_from_url ===" << std::endl;
  std::cout << "URL: " << MP3_URL << std::endl;
  AudioDecoder decoder;
  decoder.open(MP3_URL);

  const auto &meta = decoder.get_metadata();
  print_metadata(meta, "test_decode_from_url");

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


std::vector<uint8_t> read_file_bytes(const std::string &filepath)
{
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    assert(false);
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(size);
  if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
    std::cerr << "Failed to read file" << std::endl;
    assert(false);
  }
  file.close();
  return buffer;
}

//=============================================================================
// Test 3: Memory Decode
//=============================================================================
void test_decode_from_memory()
{
  std::cout << "\n=== Running test_decode_from_memory ===" << std::endl;
  std::cout << "File: " << MP3_PATH << std::endl;
  // Read file into memory
  auto buffer = read_file_bytes(MP3_PATH);

  // Decode from memory
  AudioStreamOptions options;
  AudioDecoder decoder(options);
  decoder.open_memory(buffer.data(), buffer.size());

  const auto &meta = decoder.get_metadata();
  print_metadata(meta, "test_decode_from_memory");

  // Verify metadata
  // assert(meta.sample_rate == EXPECTED_SAMPLE_RATE);
  // assert(meta.num_channels == EXPECTED_NUM_CHANNELS);

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
// Test 4: PCM Decode from Memory (raw PCM without WAV header)
//=============================================================================
void test_decode_pcm_from_memory()
{
  std::cout << "\n=== Running test_decode_pcm_from_memory ===" << std::endl;
  std::cout << "File: " << WAV_PATH << std::endl;
  auto buffer = read_file_bytes(WAV_PATH);

  AudioStreamOptions options1;
  AudioDecoder decoder1(options1);
  decoder1.open_memory(buffer.data(), buffer.size());

  const auto &meta1 = decoder1.get_metadata();
  print_metadata(meta1, "test_decode_pcm_from_memory");
  
  std::cout << "Original buffer size: " << buffer.size() << " bytes" << std::endl;
  
  // Strip the first 44 bytes (WAV header)
  constexpr size_t WAV_HEADER_SIZE = 44;
  if (buffer.size() <= WAV_HEADER_SIZE) {
    std::cerr << "File too small to contain WAV header" << std::endl;
    assert(false);
  }
  
  buffer.erase(buffer.begin(), buffer.begin() + WAV_HEADER_SIZE);
  std::cout << "PCM buffer size (after removing header): " << buffer.size() << " bytes" << std::endl;

  // Decode raw PCM data with explicit format specification
  AudioStreamOptions options2;
  options2.input_format = "s16le";       // PCM signed 16-bit little-endian
  options2.input_sample_rate = 16000;    // 16kHz
  options2.input_channels = 1;           // Mono

  AudioDecoder decoder2(options2);
  decoder2.open_memory(buffer.data(), buffer.size());

  const auto &meta2 = decoder2.get_metadata();
  print_metadata(meta2, "test_decode_pcm_from_memory");

  // Verify metadata respects our input parameters
  assert(meta2.sample_rate == 16000);
  assert(meta2.num_channels == 1);
  assert(meta2.codec == "pcm_s16le");
  
  // Calculate expected duration based on 16kHz
  double expected_duration = (double)buffer.size() / (16000 * 1 * 2); // 16kHz, mono, 2 bytes/sample
  assert(std::fabs(meta2.duration - expected_duration) < 0.1);

  // Decode a few frames to verify it works
  int frame_count = 0;
  size_t total_samples = 0;
  while (!decoder2.is_finished() && frame_count < 10) {
    auto samples = decoder2.decode_next();
    if (samples.data.empty())
      break;
    total_samples += samples.data[0].size();
    frame_count++;
  }
  
  std::cout << "Decoded " << frame_count << " frames, " << total_samples << " samples" << std::endl;
  assert(frame_count > 0);
  std::cout << "PCM decoding test passed!" << std::endl;
}

//=============================================================================
// Test 5: Streaming Decode with callback-based data provider
//=============================================================================
void test_streaming_decode()
{
  std::cout << "Running test_streaming_decode..." << std::endl;
  auto buffer = read_file_bytes(WAV_PATH);

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

  // reset read position before opening stream
  read_pos = 0;
  AudioStreamOptions stream_options;
  stream_options.input_format = "wav";
  AudioDecoder decoder(stream_options);
  decoder.open_stream(avio_read_callback, stream_options);

  const auto &meta = decoder.get_metadata();
  print_metadata(meta, "test_streaming_decode");

  assert(meta.sample_rate == 16000);
  assert(meta.num_channels == 1);

  size_t total_samples = 0;

  while (!decoder.is_finished())
  {
    auto samples = decoder.decode_next();
    if (samples.data.empty())
      break;

    assert((int)samples.data.size() == meta.num_channels);
    assert((int)samples.data[0].size() > 0);

    total_samples += samples.data[0].size();
  }

  auto diff = std::abs(static_cast<int64_t>(total_samples) - 89472);
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
  decoder.open(MP3_PATH);

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

  // avioflow_set_log_level("debug");
  // Check if test file exists
  std::ifstream check_file(MP3_PATH);
  bool file_exists = check_file.good();
  check_file.close();

  if (file_exists)
  {
    test_metadata_format();
    test_decode_from_filepath();
    test_decode_from_url();
    test_decode_from_memory();
    test_decode_pcm_from_memory();
    test_streaming_decode();

    std::cout << "All tests passed!" << std::endl;
  }
  else
  {
    std::cout << "Test file not found: " << MP3_PATH << std::endl;
  }

  return 0;
}
