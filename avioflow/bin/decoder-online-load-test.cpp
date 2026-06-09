#include "avioflow-cxx-api.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

using namespace avioflow;

const std::string MP3_PATH = "public/wavs/TownTheme.mp3";
const std::string WAV_PATH = "public/wavs/zh.wav";

std::vector<uint8_t> read_file_bytes(const std::string &filepath) {
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + filepath);
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  file.read(reinterpret_cast<char *>(buffer.data()), size);
  return buffer;
}

void simulate_streaming(const std::string &test_name, const std::string &file_path, const std::string &format, int sample_rate, int channels) {
  std::cout << "\n=== Running " << test_name << " (100ms chunks) ===" << std::endl;
  auto buffer = read_file_bytes(file_path);
  std::cout << "  File size: " << buffer.size() << " bytes" << std::endl;

  AudioStreamOptions options;
  options.input_format = format;
  if (sample_rate > 0) options.input_sample_rate = sample_rate;
  if (channels > 0) options.input_channels = channels;

  AudioDecoder decoder(options);

  // Calculate bytes for 100ms
  // For PCM s16le: sample_rate * channels * 2 bytes * 0.1s
  // For compressed formats like MP3, we just pick a reasonable chunk size
  size_t chunk_size;
  if (format == "s16le" || format == "wav") {
      int sr = sample_rate > 0 ? sample_rate : 16000; // default for zh.wav
      int ch = channels > 0 ? channels : 1;
      chunk_size = static_cast<size_t>(static_cast<double>(sr) * ch * 2 * 0.1); 
  } else {
      chunk_size = 4096; // ~100ms of typical MP3 bitrate
  }

  size_t offset = 0;
  size_t total_decoded = 0;
  int push_count = 0;

  // Push all data in chunks, decoding after each push
  while (offset < buffer.size()) {
    size_t to_push = std::min(chunk_size, buffer.size() - offset);
    decoder.feed(buffer.data() + offset, to_push);
    offset += to_push;
    push_count++;

    // Try to decode available frames
    while (auto frame = decoder.get_frame()) {
      total_decoded += frame.num_samples;
    }
  }

  // After all data is pushed, continue decoding until finished
  decoder.flush();
  while (!decoder.is_finished()) {
    auto frame = decoder.get_frame();
    if (!frame) break;
    total_decoded += frame.num_samples;
  }

  std::cout << "  Push count: " << push_count << ", Total samples: " << total_decoded << std::endl;
  assert(total_decoded > 0);
}

void test_online_mp3() {
  // MP3 streaming (TownTheme.mp3 is 44100Hz, 2ch)
  simulate_streaming("Online MP3 Test", MP3_PATH, "mp3", 44100, 2);
}

void test_online_wav() {
  // WAV streaming (zh.wav is 16000Hz, 1ch)
  simulate_streaming("Online WAV Test", WAV_PATH, "wav", 16000, 1);
}

void test_online_pcm_fallback() {
  // Test the fallback: send PCM data but set format to "wav"
  std::cout << "\n=== Running Online PCM Fallback Test (WAV format, but PCM input) ===" << std::endl;
  auto buffer = read_file_bytes(WAV_PATH);
  
  // Strip 44 bytes WAV header to simulate raw PCM
  if (buffer.size() > 44) {
      buffer.erase(buffer.begin(), buffer.begin() + 44);
  }

  AudioStreamOptions options;
  options.input_format = "wav"; // Intentional: should fallback to s16le
  options.input_sample_rate = 16000;
  options.input_channels = 1;

  AudioDecoder decoder(options);
  
  // Push in 100ms chunks
  size_t chunk_size = static_cast<size_t>(16000.0 * 1 * 2 * 0.1);
  size_t offset = 0;
  size_t total_decoded = 0;

  while (offset < buffer.size()) {
      size_t to_push = std::min(chunk_size, buffer.size() - offset);
      decoder.feed(buffer.data() + offset, to_push);
      offset += to_push;

      while (auto frame = decoder.get_frame()) {
          total_decoded += frame.num_samples;
      }
  }
  decoder.flush();
  while (!decoder.is_finished()) {
      auto frame = decoder.get_frame();
      if (!frame) break;
      total_decoded += frame.num_samples;
  }
  std::cout << "  Fallback test finished. Total samples: " << total_decoded << std::endl;
  assert(total_decoded > 0);
}

void test_online_pcm_8k_small_chunks() {
  std::cout << "\n=== Running Online PCM 8k Small Chunk Tests ===" << std::endl;
  const int sample_rate = 8000;
  const int channels = 1;
  const int seconds = 1;
  std::vector<uint8_t> pcm(static_cast<size_t>(sample_rate * channels * seconds * 2));
  for (int i = 0; i < sample_rate * seconds; ++i) {
    const double phase = 2.0 * 3.14159265358979323846 * 440.0 * i / sample_rate;
    int16_t sample = static_cast<int16_t>(std::sin(phase) * 12000.0);
    pcm[static_cast<size_t>(i * 2)] = static_cast<uint8_t>(sample & 0xff);
    pcm[static_cast<size_t>(i * 2 + 1)] = static_cast<uint8_t>((sample >> 8) & 0xff);
  }

  for (size_t chunk_size : {size_t{1}, size_t{2}, size_t{320}, size_t{3200}}) {
    AudioStreamOptions options;
    options.input_format = "s16le";
    options.input_sample_rate = sample_rate;
    options.input_channels = channels;
    AudioDecoder decoder(options);

    size_t total_decoded = 0;
    for (size_t offset = 0; offset < pcm.size(); offset += chunk_size) {
      size_t to_push = std::min(chunk_size, pcm.size() - offset);
      decoder.feed(pcm.data() + offset, to_push);
      auto samples = decoder.get_samples();
      if (!samples.empty()) {
        total_decoded += samples[0].size();
      }
    }
    decoder.flush();
    while (!decoder.is_finished()) {
      auto samples = decoder.get_samples();
      if (samples.empty()) break;
      total_decoded += samples[0].size();
    }

    std::cout << "  Chunk " << chunk_size << " bytes: " << total_decoded << " samples" << std::endl;
    assert(total_decoded == static_cast<size_t>(sample_rate * seconds));
  }
}

int main() {
  try {
    test_online_mp3();
    test_online_wav();
    test_online_pcm_fallback();
    test_online_pcm_8k_small_chunks();
    std::cout << "\nAll online tests passed!" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
