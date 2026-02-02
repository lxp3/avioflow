#include "avioflow-cxx-api.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace avioflow;

const std::string MP3_PATH = "public/wavs/TownTheme.mp3";
const std::string MP3_URL = "https://opengameart.org/sites/default/files/TownTheme.mp3";

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

void test_offline_filepath() {
  std::cout << "\n=== Test: Offline Decode from Filepath ===" << std::endl;
  AudioDecoder decoder;
  decoder.open(MP3_PATH);
  const auto &meta = decoder.get_metadata();
  std::cout << "  Codec: " << meta.codec << ", Duration: " << meta.duration << "s" << std::endl;

  size_t total_samples = 0;
  while (!decoder.is_finished()) {
    auto frame = decoder.decode_next();
    if (!frame) break;
    total_samples += frame.num_samples;
  }
  std::cout << "  Total samples decoded: " << total_samples << std::endl;
  assert(total_samples > 0);
}

void test_offline_memory() {
  std::cout << "\n=== Test: Offline Decode from Memory (Full Bytes) ===" << std::endl;
  auto buffer = read_file_bytes(MP3_PATH);
  AudioDecoder decoder;
  decoder.open(buffer.data(), buffer.size());
  const auto &meta = decoder.get_metadata();
  std::cout << "  Codec: " << meta.codec << ", Duration: " << meta.duration << "s" << std::endl;

  size_t total_samples = 0;
  while (!decoder.is_finished()) {
    auto frame = decoder.decode_next();
    if (!frame) break;
    total_samples += frame.num_samples;
  }
  std::cout << "  Total samples decoded: " << total_samples << std::endl;
  assert(total_samples > 0);
}

void test_offline_url() {
  std::cout << "\n=== Test: Offline Decode from URL ===" << std::endl;
  AudioDecoder decoder;
  decoder.open(MP3_URL);
  const auto &meta = decoder.get_metadata();
  std::cout << "  Codec: " << meta.codec << ", Sample Rate: " << meta.sample_rate << "Hz" << std::endl;

  int frames = 0;
  while (!decoder.is_finished() && frames < 10) {
    auto frame = decoder.decode_next();
    if (!frame) break;
    frames++;
  }
  std::cout << "  Successfully decoded " << frames << " frames from URL" << std::endl;
  assert(frames > 0);
}

int main() {
  try {
    test_offline_filepath();
    test_offline_memory();
    test_offline_url();
    std::cout << "\nAll offline tests passed!" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
