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

    AudioDecoder buffer_decoder;
    expect_exception(
        [&] { buffer_decoder.load_buffer(nullptr, 1); },
        "null buffer with non-zero size");

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
