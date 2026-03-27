#include "avioflow-cxx-api.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace avioflow;

namespace {

struct FixtureData {
  Metadata meta;
  std::vector<std::vector<float>> samples;
};

bool contains_name(const std::vector<std::string> &values, const std::string &name) {
  return std::find(values.begin(), values.end(), name) != values.end();
}

std::string first_available_name(const std::vector<std::string> &values,
                                 const std::vector<std::string> &candidates) {
  for (const auto &candidate : candidates) {
    if (contains_name(values, candidate)) {
      return candidate;
    }
  }
  return "";
}

FixtureData load_fixture_once() {
  const std::filesystem::path input_path = "public/wavs/zh.wav";
  AudioDecoder source_decoder;
  source_decoder.open(input_path.string());

  FixtureData fixture;
  fixture.meta = source_decoder.get_metadata();
  fixture.samples = source_decoder.get_samples();
  assert(!fixture.samples.empty());
  return fixture;
}

void assert_roundtrip(const std::filesystem::path &output_path,
                      const FixtureData &fixture,
                      const std::string &expected_container,
                      const std::string &expected_codec) {
  assert(std::filesystem::exists(output_path));

  AudioDecoder roundtrip_decoder;
  roundtrip_decoder.open(output_path.string());
  const auto &meta = roundtrip_decoder.get_metadata();
  auto decoded = roundtrip_decoder.get_samples();

  std::cout << "  Container: " << meta.container << ", Codec: " << meta.codec
            << ", Channels: " << meta.num_channels << ", Rate: " << meta.sample_rate
            << std::endl;

  assert(meta.sample_rate == fixture.meta.sample_rate);
  assert(meta.num_channels == fixture.meta.num_channels);
  assert(!decoded.empty());
  assert(decoded.size() == fixture.samples.size());
  assert(decoded[0].size() == fixture.samples[0].size());
  assert(meta.container.find(expected_container) != std::string::npos);
  assert(meta.codec.find(expected_codec) != std::string::npos);
}

void test_save_flac(const FixtureData &fixture) {
  std::cout << "\n=== Test: Offline Encode Fixture to FLAC ===" << std::endl;
  const std::filesystem::path output_path = "public/wavs/zh.flac";
  std::filesystem::create_directories(output_path.parent_path());

  AudioWriteOptions options("flac", fixture.meta.sample_rate);
  save_audio(output_path.string(), fixture.samples, options);

  assert_roundtrip(output_path, fixture, "flac", "flac");
}

void test_save_wav_pcm_s16le(const FixtureData &fixture) {
  std::cout << "\n=== Test: Offline Encode Fixture to WAV PCM S16LE ===" << std::endl;
  const std::filesystem::path output_path = "public/wavs/zh-s16.wav";
  std::filesystem::create_directories(output_path.parent_path());

  AudioWriteOptions options("wav", fixture.meta.sample_rate);
  save_audio(output_path.string(), fixture.samples, options);

  assert_roundtrip(output_path, fixture, "wav", "pcm_s16le");
}

void test_save_wav_pcm_f32le(const FixtureData &fixture) {
  std::cout << "\n=== Test: Offline Encode Fixture to WAV PCM F32LE ===" << std::endl;
  const std::filesystem::path output_path = "public/wavs/zh-f32.wav";
  std::filesystem::create_directories(output_path.parent_path());

  AudioWriteOptions options("wav_f32", fixture.meta.sample_rate);
  save_audio(output_path.string(), fixture.samples, options);

  assert_roundtrip(output_path, fixture, "wav", "pcm_f32le");
}

void test_save_aac_if_supported(const FixtureData &fixture) {
  auto encoders = get_supported_encoders();
  auto muxers = get_supported_output_formats();
  if (!contains_name(encoders, "aac") || !contains_name(muxers, "adts")) {
    std::cout << "\n=== Skip: AAC encoder or ADTS muxer not available ===" << std::endl;
    return;
  }

  std::cout << "\n=== Test: Offline Encode Fixture to AAC ADTS ===" << std::endl;
  const std::filesystem::path output_path = "public/wavs/zh.aac";
  std::filesystem::create_directories(output_path.parent_path());

  AudioWriteOptions options("aac", fixture.meta.sample_rate, std::nullopt, 192000);
  save_audio(output_path.string(), fixture.samples, options);

  assert_roundtrip(output_path, fixture, "aac", "aac");
}

void test_save_mp3_if_supported(const FixtureData &fixture) {
  auto encoders = get_supported_encoders();
  auto muxers = get_supported_output_formats();
  std::string encoder_name = first_available_name(encoders, {"libmp3lame", "mp3"});
  if (encoder_name.empty() || !contains_name(muxers, "mp3")) {
    std::cout << "\n=== Skip: MP3 encoder or MP3 muxer not available ===" << std::endl;
    return;
  }

  std::cout << "\n=== Test: Offline Encode Fixture to MP3 ===" << std::endl;
  const std::filesystem::path output_path = "public/wavs/zh.mp3";
  std::filesystem::create_directories(output_path.parent_path());

  AudioWriteOptions options("mp3", fixture.meta.sample_rate);
  save_audio(output_path.string(), fixture.samples, options);

  assert_roundtrip(output_path, fixture, "mp3", "mp3");
}

void test_save_opus_if_supported(const FixtureData &fixture) {
  auto encoders = get_supported_encoders();
  auto muxers = get_supported_output_formats();
  std::string encoder_name = first_available_name(encoders, {"libopus", "opus"});
  std::string muxer_name = first_available_name(muxers, {"opus", "ogg"});
  if (encoder_name.empty() || muxer_name.empty()) {
    std::cout << "\n=== Skip: Opus encoder or muxer not available ===" << std::endl;
    return;
  }

  std::cout << "\n=== Test: Offline Encode Fixture to Opus ===" << std::endl;
  const std::filesystem::path output_path =
      muxer_name == "ogg" ? "public/wavs/zh.ogg" : "public/wavs/zh.opus";
  std::filesystem::create_directories(output_path.parent_path());

  AudioWriteOptions options("opus", fixture.meta.sample_rate);
  save_audio(output_path.string(), fixture.samples, options);

  assert_roundtrip(output_path, fixture, muxer_name, "opus");
}

} // namespace

int main() {
  try {
    FixtureData fixture = load_fixture_once();
    test_save_flac(fixture);
    test_save_wav_pcm_s16le(fixture);
    test_save_wav_pcm_f32le(fixture);
    test_save_aac_if_supported(fixture);
    test_save_mp3_if_supported(fixture);
    test_save_opus_if_supported(fixture);
    std::cout << "\nAll encoder tests passed!" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Test failed: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
