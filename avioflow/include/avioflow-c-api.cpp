// C ABI over the avioflow C++ API.
//
// Usable from any language with C FFI; the Rust crate in rust/ is the first
// consumer. Every entry point catches all exceptions, because unwinding across
// an FFI boundary is undefined behaviour: failures become status codes plus a
// thread-local message readable through avf_last_error().

#include "avioflow-c-api.h"

#include "avioflow-cxx-api.h"
#include "metadata.h"

#include <cstring>
#include <cstdlib>
#include <exception>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace avioflow;

namespace {

std::string &last_error_storage() {
  thread_local std::string storage;
  return storage;
}

int &last_error_code_storage() {
  thread_local int storage = AVF_OK;
  return storage;
}

void set_last_error(int code, const std::string &message) {
  last_error_code_storage() = code;
  try {
    last_error_storage() = message;
  } catch (...) {
    // A message we cannot store is still reported through the status code.
  }
}

void clear_last_error() { set_last_error(AVF_OK, std::string()); }

// Runs `body` and converts any exception into a status code. Used by every
// entry point so no exception reaches the caller.
template <typename Body> int guard(Body &&body) {
  try {
    clear_last_error();
    return body();
  } catch (const std::invalid_argument &error) {
    set_last_error(AVF_ERR_INVALID_ARGUMENT, error.what());
    return AVF_ERR_INVALID_ARGUMENT;
  } catch (const std::exception &error) {
    set_last_error(AVF_ERR_RUNTIME, error.what());
    return AVF_ERR_RUNTIME;
  } catch (...) {
    set_last_error(AVF_ERR_UNKNOWN, "unknown error");
    return AVF_ERR_UNKNOWN;
  }
}

// Same contract for functions returning a handle: NULL signals failure.
template <typename Body> auto guard_ptr(Body &&body) -> decltype(body()) {
  try {
    clear_last_error();
    return body();
  } catch (const std::invalid_argument &error) {
    set_last_error(AVF_ERR_INVALID_ARGUMENT, error.what());
    return nullptr;
  } catch (const std::exception &error) {
    set_last_error(AVF_ERR_RUNTIME, error.what());
    return nullptr;
  } catch (...) {
    set_last_error(AVF_ERR_UNKNOWN, "unknown error");
    return nullptr;
  }
}

// Copies into a fixed-size char array, always NUL-terminating. Metadata strings
// are codec and format names, far shorter than the buffers in AvfMetadata.
template <size_t N> void copy_string(char (&dest)[N], const std::string &src) {
  const size_t length = src.size() < N - 1 ? src.size() : N - 1;
  std::memcpy(dest, src.data(), length);
  dest[length] = '\0';
}

void fill_metadata(AvfMetadata *out, const Metadata &meta) {
  if (!out)
    return;
  out->duration = meta.duration;
  out->num_samples = meta.num_samples;
  out->sample_rate = meta.sample_rate;
  out->num_channels = meta.num_channels;
  out->bit_rate = meta.bit_rate;
  copy_string(out->sample_format, meta.sample_format);
  copy_string(out->codec, meta.codec);
  copy_string(out->container, meta.container);
}

std::optional<int> optional_int(int32_t value, int32_t has_value) {
  if (!has_value)
    return std::nullopt;
  return static_cast<int>(value);
}

std::optional<std::string> optional_string(const char *value) {
  if (!value)
    return std::nullopt;
  return std::string(value);
}

AudioStreamOptions to_stream_options(const AvfStreamOptions *options) {
  AudioStreamOptions result;
  if (!options)
    return result;
  result.output_sample_rate =
      optional_int(options->output_sample_rate, options->has_output_sample_rate);
  result.output_num_channels = optional_int(options->output_num_channels,
                                           options->has_output_num_channels);
  result.input_sample_rate =
      optional_int(options->input_sample_rate, options->has_input_sample_rate);
  result.input_channels =
      optional_int(options->input_channels, options->has_input_channels);
  result.input_format = optional_string(options->input_format);
  return result;
}

AudioWriteOptions to_write_options(const AvfWriteOptions *options) {
  AudioWriteOptions result;
  if (!options)
    return result;
  result.codec_name = optional_string(options->codec_name);
  result.container_format = optional_string(options->container_format);
  result.sample_format = optional_string(options->sample_format);
  result.sample_rate =
      optional_int(options->sample_rate, options->has_sample_rate);
  result.num_channels =
      optional_int(options->num_channels, options->has_num_channels);
  if (options->has_bit_rate)
    result.bit_rate = static_cast<int64_t>(options->bit_rate);
  result.overwrite = options->overwrite != 0;
  return result;
}

AudioResampleOptions to_resample_options(const AvfResampleOptions *options) {
  AudioResampleOptions result;
  result.input_sample_rate = options->input_sample_rate;
  result.output_sample_rate = options->output_sample_rate;
  result.output_num_channels = optional_int(options->output_num_channels,
                                           options->has_output_num_channels);
  return result;
}

// Copies caller-owned planar pointers into the vector-of-vectors the C++ API
// expects. Rejects negative counts before they become huge size_t values.
std::vector<std::vector<float>> to_sample_vectors(const float *const *channels,
                                                  int32_t num_channels,
                                                  int64_t num_samples) {
  if (num_channels < 0)
    throw std::invalid_argument("num_channels must not be negative");
  if (num_samples < 0)
    throw std::invalid_argument("num_samples must not be negative");
  if (num_channels > 0 && !channels)
    throw std::invalid_argument("channels must not be null");

  std::vector<std::vector<float>> result(static_cast<size_t>(num_channels));
  for (int32_t c = 0; c < num_channels; ++c) {
    const float *data = channels[c];
    if (!data && num_samples > 0)
      throw std::invalid_argument("channel data must not be null");
    result[static_cast<size_t>(c)].assign(data, data + num_samples);
  }
  return result;
}

} // namespace

// Definitions of the opaque handle types declared in the header.
struct AvfStringList {
  std::vector<std::string> items;
};

struct AvfSamples {
  std::vector<std::vector<float>> channels;
};

struct AvfDecoder {
  explicit AvfDecoder(const AudioStreamOptions &options) : decoder(options) {}
  AudioDecoder decoder;
};

struct AvfEncoder {
  explicit AvfEncoder(const AudioWriteOptions &options) : encoder(options) {}
  AudioEncoder encoder;
};

struct AvfResampler {
  explicit AvfResampler(const AudioResampleOptions &options)
      : resampler(options) {}
  AudioResampler resampler;
};

struct AvfDeviceList {
  std::vector<DeviceInfo> items;
};

namespace {

// Wraps a result into an owned AvfSamples. `out` is only written on success, so
// a failed call never leaves a dangling pointer with the caller.
int emit_samples(std::vector<std::vector<float>> &&samples,
                 AvfSamples **out) {
  if (!out)
    throw std::invalid_argument("out_samples must not be null");
  auto owned = new AvfSamples{std::move(samples)};
  *out = owned;
  return AVF_OK;
}

int emit_string_list(std::vector<std::string> &&items, AvfStringList **out) {
  if (!out)
    throw std::invalid_argument("out_list must not be null");
  *out = new AvfStringList{std::move(items)};
  return AVF_OK;
}

} // namespace

extern "C" {

const char *avf_last_error(void) { return last_error_storage().c_str(); }

int avf_last_error_code(void) { return last_error_code_storage(); }

void avf_set_log_level(const char *level) {
  // Not fallible in the C++ API, but keep exceptions from escaping regardless.
  guard([&] {
    avioflow_set_log_level(level);
    return AVF_OK;
  });
}

/* --- String list --- */

size_t avf_string_list_size(const AvfStringList *list) {
  return list ? list->items.size() : 0;
}

const char *avf_string_list_get(const AvfStringList *list, size_t index) {
  if (!list || index >= list->items.size())
    return nullptr;
  return list->items[index].c_str();
}

void avf_string_list_free(AvfStringList *list) { delete list; }

int avf_get_supported_decoders(AvfStringList **out_list) {
  return guard([&] { return emit_string_list(get_supported_decoders(), out_list); });
}

int avf_get_supported_encoders(AvfStringList **out_list) {
  return guard([&] { return emit_string_list(get_supported_encoders(), out_list); });
}

int avf_get_supported_input_formats(AvfStringList **out_list) {
  return guard(
      [&] { return emit_string_list(get_supported_input_formats(), out_list); });
}

int avf_get_supported_output_formats(AvfStringList **out_list) {
  return guard(
      [&] { return emit_string_list(get_supported_output_formats(), out_list); });
}

/* --- Samples --- */

int32_t avf_samples_num_channels(const AvfSamples *samples) {
  return samples ? static_cast<int32_t>(samples->channels.size()) : 0;
}

int64_t avf_samples_num_samples(const AvfSamples *samples) {
  if (!samples || samples->channels.empty())
    return 0;
  return static_cast<int64_t>(samples->channels[0].size());
}

const float *avf_samples_channel(const AvfSamples *samples, int32_t channel) {
  if (!samples || channel < 0 ||
      static_cast<size_t>(channel) >= samples->channels.size())
    return nullptr;
  return samples->channels[static_cast<size_t>(channel)].data();
}

void avf_samples_free(AvfSamples *samples) { delete samples; }

/* --- Decoder --- */

AvfDecoder *avf_decoder_new(const AvfStreamOptions *options) {
  return guard_ptr(
      [&] { return new AvfDecoder(to_stream_options(options)); });
}

void avf_decoder_free(AvfDecoder *decoder) { delete decoder; }

int avf_decoder_load_file(AvfDecoder *decoder, const char *source,
                          AvfMetadata *out_metadata) {
  return guard([&] {
    if (!decoder)
      throw std::invalid_argument("decoder must not be null");
    if (!source)
      throw std::invalid_argument("source must not be null");
    fill_metadata(out_metadata, decoder->decoder.load_file(source));
    return AVF_OK;
  });
}

int avf_decoder_load_buffer(AvfDecoder *decoder, const uint8_t *data,
                            size_t size, AvfMetadata *out_metadata) {
  return guard([&] {
    if (!decoder)
      throw std::invalid_argument("decoder must not be null");
    if (!data && size > 0)
      throw std::invalid_argument("data must not be null");
    fill_metadata(out_metadata, decoder->decoder.load_buffer(data, size));
    return AVF_OK;
  });
}

int avf_decoder_feed(AvfDecoder *decoder, const uint8_t *data, size_t size) {
  return guard([&] {
    if (!decoder)
      throw std::invalid_argument("decoder must not be null");
    if (!data && size > 0)
      throw std::invalid_argument("data must not be null");
    decoder->decoder.feed(data, size);
    return AVF_OK;
  });
}

int avf_decoder_flush(AvfDecoder *decoder) {
  return guard([&] {
    if (!decoder)
      throw std::invalid_argument("decoder must not be null");
    decoder->decoder.flush();
    return AVF_OK;
  });
}

int avf_decoder_get_frame(AvfDecoder *decoder, AvfFrame *out_frame) {
  return guard([&] {
    if (!decoder)
      throw std::invalid_argument("decoder must not be null");
    if (!out_frame)
      throw std::invalid_argument("out_frame must not be null");
    const FrameData frame = decoder->decoder.get_frame();
    out_frame->data = const_cast<const float *const *>(frame.data);
    out_frame->num_channels = frame.num_channels;
    out_frame->num_samples = frame.num_samples;
    return AVF_OK;
  });
}

int avf_decoder_get_samples(AvfDecoder *decoder, double start_seconds,
                            double stop_seconds, int32_t has_stop_seconds,
                            AvfSamples **out_samples) {
  return guard([&] {
    if (!decoder)
      throw std::invalid_argument("decoder must not be null");
    std::optional<double> stop;
    if (has_stop_seconds)
      stop = stop_seconds;
    return emit_samples(decoder->decoder.get_samples(start_seconds, stop),
                        out_samples);
  });
}

int avf_decoder_is_finished(const AvfDecoder *decoder, int32_t *out_finished) {
  return guard([&] {
    if (!decoder)
      throw std::invalid_argument("decoder must not be null");
    if (!out_finished)
      throw std::invalid_argument("out_finished must not be null");
    *out_finished = decoder->decoder.is_finished() ? 1 : 0;
    return AVF_OK;
  });
}

int avf_decoder_get_metadata(const AvfDecoder *decoder,
                            AvfMetadata *out_metadata) {
  return guard([&] {
    if (!decoder)
      throw std::invalid_argument("decoder must not be null");
    if (!out_metadata)
      throw std::invalid_argument("out_metadata must not be null");
    fill_metadata(out_metadata, decoder->decoder.get_metadata());
    return AVF_OK;
  });
}

/* --- Encoder --- */

AvfEncoder *avf_encoder_new(const AvfWriteOptions *options) {
  return guard_ptr([&] { return new AvfEncoder(to_write_options(options)); });
}

void avf_encoder_free(AvfEncoder *encoder) { delete encoder; }

int avf_encoder_save(AvfEncoder *encoder, const char *path,
                     const float *const *channels, int32_t num_channels,
                     int64_t num_samples) {
  return guard([&] {
    if (!encoder)
      throw std::invalid_argument("encoder must not be null");
    if (!path)
      throw std::invalid_argument("path must not be null");
    encoder->encoder.save(path,
                          to_sample_vectors(channels, num_channels, num_samples));
    return AVF_OK;
  });
}

int avf_encoder_save_buffer(AvfEncoder *encoder, const float *const *channels,
                            int32_t num_channels, int64_t num_samples,
                            uint8_t **out_data, size_t *out_size) {
  return guard([&] {
    if (!encoder || !out_data || !out_size) throw std::invalid_argument("null argument");
    auto data = encoder->encoder.save_buffer(to_sample_vectors(channels, num_channels, num_samples));
    *out_size = data.size();
    *out_data = static_cast<uint8_t *>(std::malloc(data.size()));
    if (data.size() && !*out_data) throw std::bad_alloc();
    if (data.size()) std::memcpy(*out_data, data.data(), data.size());
    return AVF_OK;
  });
}

int avf_save_audio_buffer(const float *const *channels, int32_t num_channels,
                          int64_t num_samples, const AvfWriteOptions *options,
                          uint8_t **out_data, size_t *out_size) {
  return guard([&] {
    if (!out_data || !out_size) throw std::invalid_argument("null output");
    auto data = save_audio_buffer(to_sample_vectors(channels, num_channels, num_samples), to_write_options(options));
    *out_size = data.size();
    *out_data = static_cast<uint8_t *>(std::malloc(data.size()));
    if (data.size() && !*out_data) throw std::bad_alloc();
    if (data.size()) std::memcpy(*out_data, data.data(), data.size());
    return AVF_OK;
  });
}

void avf_free_buffer(uint8_t *data) { std::free(data); }

int avf_save_audio(const char *path, const float *const *channels,
                   int32_t num_channels, int64_t num_samples,
                   const AvfWriteOptions *options) {
  return guard([&] {
    if (!path)
      throw std::invalid_argument("path must not be null");
    save_audio(path, to_sample_vectors(channels, num_channels, num_samples),
               to_write_options(options));
    return AVF_OK;
  });
}

/* --- Resampler --- */

AvfResampler *avf_resampler_new(const AvfResampleOptions *options) {
  return guard_ptr([&]() -> AvfResampler * {
    if (!options)
      throw std::invalid_argument("options must not be null");
    return new AvfResampler(to_resample_options(options));
  });
}

void avf_resampler_free(AvfResampler *resampler) { delete resampler; }

int avf_resampler_process(AvfResampler *resampler, const float *const *channels,
                          int32_t num_channels, int64_t num_samples,
                          AvfSamples **out_samples) {
  return guard([&] {
    if (!resampler)
      throw std::invalid_argument("resampler must not be null");
    return emit_samples(resampler->resampler.process(to_sample_vectors(
                            channels, num_channels, num_samples)),
                        out_samples);
  });
}

int avf_resampler_flush(AvfResampler *resampler, AvfSamples **out_samples) {
  return guard([&] {
    if (!resampler)
      throw std::invalid_argument("resampler must not be null");
    return emit_samples(resampler->resampler.flush(), out_samples);
  });
}

int avf_resampler_output_sample_rate(const AvfResampler *resampler,
                                     int32_t *out_rate) {
  return guard([&] {
    if (!resampler)
      throw std::invalid_argument("resampler must not be null");
    if (!out_rate)
      throw std::invalid_argument("out_rate must not be null");
    *out_rate = resampler->resampler.output_sample_rate();
    return AVF_OK;
  });
}

int avf_resampler_output_num_channels(const AvfResampler *resampler,
                                      int32_t *out_channels) {
  return guard([&] {
    if (!resampler)
      throw std::invalid_argument("resampler must not be null");
    if (!out_channels)
      throw std::invalid_argument("out_channels must not be null");
    *out_channels = resampler->resampler.output_num_channels();
    return AVF_OK;
  });
}

int avf_resample(const float *const *channels, int32_t num_channels,
                 int64_t num_samples, int32_t input_sample_rate,
                 int32_t output_sample_rate, int32_t output_num_channels,
                 int32_t has_output_num_channels, AvfSamples **out_samples) {
  return guard([&] {
    return emit_samples(
        resample(to_sample_vectors(channels, num_channels, num_samples),
                 input_sample_rate, output_sample_rate,
                 optional_int(output_num_channels, has_output_num_channels)),
        out_samples);
  });
}

/* --- Devices --- */

int avf_list_audio_devices(AvfDeviceList **out_list) {
  return guard([&] {
    if (!out_list)
      throw std::invalid_argument("out_list must not be null");
    *out_list = new AvfDeviceList{DeviceManager::list_audio_devices()};
    return AVF_OK;
  });
}

size_t avf_device_list_size(const AvfDeviceList *list) {
  return list ? list->items.size() : 0;
}

const char *avf_device_list_name(const AvfDeviceList *list, size_t index) {
  if (!list || index >= list->items.size())
    return nullptr;
  return list->items[index].name.c_str();
}

const char *avf_device_list_description(const AvfDeviceList *list,
                                        size_t index) {
  if (!list || index >= list->items.size())
    return nullptr;
  return list->items[index].description.c_str();
}

int32_t avf_device_list_is_output(const AvfDeviceList *list, size_t index) {
  if (!list || index >= list->items.size())
    return 0;
  return list->items[index].is_output ? 1 : 0;
}

void avf_device_list_free(AvfDeviceList *list) { delete list; }

} // extern "C"
