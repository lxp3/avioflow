#include "avioflow-cxx-api.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>

using avioflow::AudioDecoder;
using avioflow::AudioStreamOptions;

template <typename Function>
void expect_exception(Function function, const char *description) {
  try {
    function();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error(std::string("Expected exception: ") + description);
}

template <typename Function>
void expect_retryable_failure(Function function) {
  try {
    function();
  } catch (const std::exception &error) {
    if (std::string(error.what()).find("already initialized") != std::string::npos) {
      throw std::runtime_error("Failed initialization left decoder initialized");
    }
    return;
  }
  throw std::runtime_error("Expected missing file failure");
}

int main() {
  try {
    expect_exception(
        [] {
          AudioStreamOptions options;
          options.output_sample_rate = 0;
          AudioDecoder decoder(options);
        },
        "zero output sample rate");

    expect_exception(
        [] {
          AudioStreamOptions options;
          options.output_num_channels = 0;
          AudioDecoder decoder(options);
        },
        "zero output channel count");

    expect_exception(
        [] {
          AudioStreamOptions options;
          options.output_sample_rate = -2;
          AudioDecoder decoder(options);
        },
        "output sample rate below -1");

    expect_exception(
        [] {
          AudioStreamOptions options;
          options.output_num_channels = -2;
          AudioDecoder decoder(options);
        },
        "output channel count below -1");

    AudioStreamOptions passthrough_options;
    passthrough_options.output_sample_rate = -1;
    passthrough_options.output_num_channels = -1;
    AudioDecoder passthrough_decoder(passthrough_options);

    AudioDecoder buffer_decoder;
    expect_exception(
        [&] { buffer_decoder.load_buffer(nullptr, 1); },
        "null buffer with non-zero size");

    expect_exception(
        [] {
          const uint8_t garbage[] = {0x00, 0x01, 0x02, 0x03, 0xff, 0xfe, 0x7f, 0x80};
          AudioDecoder decoder;
          decoder.load_buffer(garbage, sizeof(garbage));
          decoder.get_samples();
        },
        "invalid audio bytes");

    expect_exception(
        [] {
          const uint8_t truncated_wav[] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
          AudioDecoder decoder;
          decoder.load_buffer(truncated_wav, sizeof(truncated_wav));
          decoder.get_samples();
        },
        "truncated wav header");

    AudioDecoder retry_after_invalid;
    expect_exception(
        [&] {
          const uint8_t garbage[] = {0x11, 0x22, 0x33, 0x44};
          retry_after_invalid.load_buffer(garbage, sizeof(garbage));
        },
        "first invalid audio");
    expect_exception(
        [&] {
          const uint8_t garbage[] = {'R', 'I', 'F', 'F', 1, 2, 3, 4};
          retry_after_invalid.load_buffer(garbage, sizeof(garbage));
          retry_after_invalid.get_samples();
        },
        "second invalid audio on same decoder");

    AudioStreamOptions stream_options;
    stream_options.input_sample_rate = 16000;
    stream_options.input_channels = 1;
    stream_options.input_format = "s16le";
    AudioDecoder stream_decoder(stream_options);
    expect_exception([&] { stream_decoder.feed(nullptr, 1); },
                     "null stream input with non-zero size");

    AudioDecoder retry_decoder;
    expect_exception([&] { retry_decoder.load_file("missing-audio-file.wav"); },
                     "missing file");
    expect_retryable_failure(
        [&] { retry_decoder.load_file("still-missing.wav"); });
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
