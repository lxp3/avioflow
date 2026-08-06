/**
 * @file avioflow-c-api.h
 * @brief Flat C ABI over the avioflow C++ API.
 *
 * The C++ facade in avioflow-cxx-api.h uses std::vector, std::optional and
 * exceptions, none of which cross an FFI boundary. This header exposes the same
 * functionality through opaque handles, out-parameters and integer status codes.
 *
 * Conventions:
 * - Every fallible function returns int: 0 on success, negative on failure.
 *   No C++ exception escapes; failures are recorded and readable through
 *   avf_last_error().
 * - Optional fields are a value plus an explicit has_* flag, so that documented
 *   sentinel values such as -1 ("preserve source") keep their meaning.
 * - Sample buffers returned as AvfSamples are owned by the caller and must be
 *   released with avf_samples_free().
 */

#ifndef AVIOFLOW_C_API_H
#define AVIOFLOW_C_API_H

#include <stddef.h>
#include <stdint.h>

/*
 * Export/import annotation, defined here rather than taken from metadata.h so
 * that this header stays usable from a C compiler; metadata.h is C++.
 * Kept in sync with the definition there.
 */
#ifndef AVIOFLOW_API
#ifdef AVIOFLOW_STATIC
#define AVIOFLOW_API
#else
#ifdef _WIN32
#ifdef AVIOFLOW_EXPORTS
#define AVIOFLOW_API __declspec(dllexport)
#else
#define AVIOFLOW_API __declspec(dllimport)
#endif
#else
#define AVIOFLOW_API __attribute__((visibility("default")))
#endif
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Status codes. Only the sign is contractual; use avf_last_error() for detail. */
#define AVF_OK 0
#define AVF_ERR_INVALID_ARGUMENT (-1)
#define AVF_ERR_RUNTIME (-2)
#define AVF_ERR_UNKNOWN (-3)

/**
 * @brief Message for the most recent failure on the calling thread.
 *
 * The storage is thread-local and is overwritten by the next failing call on
 * the same thread. Returns an empty string when nothing has failed yet, never
 * NULL. Copy the message before making another avioflow call.
 */
AVIOFLOW_API const char *avf_last_error(void);

/**
 * @brief Status code of the most recent failure on the calling thread.
 *
 * Carries the classification for functions that signal failure by returning
 * NULL and so have no status code of their own. Returns AVF_OK when the last
 * call succeeded. Same thread-local lifetime as avf_last_error().
 */
AVIOFLOW_API int avf_last_error_code(void);

/** @brief Set the FFmpeg log level. Pass NULL for the default ("error"). */
AVIOFLOW_API void avf_set_log_level(const char *level);

/* ------------------------------------------------------------------------- */
/* Owned string list                                                          */
/* ------------------------------------------------------------------------- */

/** Opaque owned list of strings. Release with avf_string_list_free(). */
typedef struct AvfStringList AvfStringList;

AVIOFLOW_API size_t avf_string_list_size(const AvfStringList *list);

/**
 * @brief Borrow entry @p index as a NUL-terminated string.
 * @return NULL when @p index is out of range. Valid until the list is freed.
 */
AVIOFLOW_API const char *avf_string_list_get(const AvfStringList *list, size_t index);

AVIOFLOW_API void avf_string_list_free(AvfStringList *list);

/** Format/codec queries. On success *out_list is owned by the caller. */
AVIOFLOW_API int avf_get_supported_decoders(AvfStringList **out_list);
AVIOFLOW_API int avf_get_supported_encoders(AvfStringList **out_list);
AVIOFLOW_API int avf_get_supported_input_formats(AvfStringList **out_list);
AVIOFLOW_API int avf_get_supported_output_formats(AvfStringList **out_list);

/* ------------------------------------------------------------------------- */
/* Owned sample buffer                                                        */
/* ------------------------------------------------------------------------- */

/**
 * @brief Owned planar float samples: channels[channel][sample].
 *
 * Every channel holds num_samples floats. Release with avf_samples_free().
 */
typedef struct AvfSamples AvfSamples;

AVIOFLOW_API int32_t avf_samples_num_channels(const AvfSamples *samples);
AVIOFLOW_API int64_t avf_samples_num_samples(const AvfSamples *samples);

/**
 * @brief Borrow the sample data for @p channel.
 * @return NULL when @p channel is out of range. Valid until the buffer is freed.
 */
AVIOFLOW_API const float *avf_samples_channel(const AvfSamples *samples, int32_t channel);

AVIOFLOW_API void avf_samples_free(AvfSamples *samples);

/* ------------------------------------------------------------------------- */
/* Plain data structs                                                         */
/* ------------------------------------------------------------------------- */

/** @brief Mirrors avioflow::Metadata. Strings are fixed-size to keep this POD. */
typedef struct AvfMetadata {
  double duration;
  int64_t num_samples;
  int32_t sample_rate;
  int32_t num_channels;
  int64_t bit_rate;
  char sample_format[32];
  char codec[64];
  char container[64];
} AvfMetadata;

/** @brief Mirrors avioflow::AudioStreamOptions. */
typedef struct AvfStreamOptions {
  int32_t output_sample_rate;
  int32_t has_output_sample_rate;
  int32_t output_num_channels;
  int32_t has_output_num_channels;
  int32_t input_sample_rate;
  int32_t has_input_sample_rate;
  int32_t input_channels;
  int32_t has_input_channels;
  /** NUL-terminated, or NULL when unset. Borrowed for the duration of the call. */
  const char *input_format;
} AvfStreamOptions;

/** @brief Mirrors avioflow::AudioWriteOptions. NULL strings mean unset. */
typedef struct AvfWriteOptions {
  const char *codec_name;
  const char *container_format;
  const char *sample_format;
  int32_t sample_rate;
  int32_t has_sample_rate;
  int32_t num_channels;
  int32_t has_num_channels;
  int64_t bit_rate;
  int32_t has_bit_rate;
  int32_t overwrite;
} AvfWriteOptions;

/** @brief Mirrors avioflow::AudioResampleOptions. */
typedef struct AvfResampleOptions {
  int32_t input_sample_rate;
  int32_t output_sample_rate;
  int32_t output_num_channels;
  int32_t has_output_num_channels;
} AvfResampleOptions;

/**
 * @brief Borrowed view of a decoded frame, mirroring avioflow::FrameData.
 *
 * data points into decoder-owned buffers and is invalidated by the next
 * avf_decoder_get_frame() or avf_decoder_get_samples() call. Copy before then.
 * data is NULL at end of stream.
 */
typedef struct AvfFrame {
  const float *const *data;
  int32_t num_channels;
  int32_t num_samples;
} AvfFrame;

/* ------------------------------------------------------------------------- */
/* Decoder                                                                    */
/* ------------------------------------------------------------------------- */

typedef struct AvfDecoder AvfDecoder;

/**
 * @brief Create a decoder. Pass NULL for default options.
 * @return NULL on failure; see avf_last_error().
 */
AVIOFLOW_API AvfDecoder *avf_decoder_new(const AvfStreamOptions *options);

AVIOFLOW_API void avf_decoder_free(AvfDecoder *decoder);

/** @brief Open a file path, URL or device. @p out_metadata may be NULL. */
AVIOFLOW_API int avf_decoder_load_file(AvfDecoder *decoder, const char *source,
                          AvfMetadata *out_metadata);

/** @brief Open complete audio file bytes held in memory. */
AVIOFLOW_API int avf_decoder_load_buffer(AvfDecoder *decoder, const uint8_t *data,
                            size_t size, AvfMetadata *out_metadata);

/** @brief Push encoded bytes for streaming decode. Requires input_format. */
AVIOFLOW_API int avf_decoder_feed(AvfDecoder *decoder, const uint8_t *data, size_t size);

/** @brief Mark streaming input complete so remaining frames can be drained. */
AVIOFLOW_API int avf_decoder_flush(AvfDecoder *decoder);

/**
 * @brief Decode the next frame without copying.
 *
 * At end of stream this succeeds with out_frame->data set to NULL.
 */
AVIOFLOW_API int avf_decoder_get_frame(AvfDecoder *decoder, AvfFrame *out_frame);

/**
 * @brief Decode the half-open range [start_seconds, stop_seconds).
 *
 * Set has_stop_seconds to 0 to decode through to the end. Range decoding
 * requires offline mode. On success *out_samples is owned by the caller.
 */
AVIOFLOW_API int avf_decoder_get_samples(AvfDecoder *decoder, double start_seconds,
                            double stop_seconds, int32_t has_stop_seconds,
                            AvfSamples **out_samples);

/** @brief Non-zero once the stream is exhausted. */
AVIOFLOW_API int avf_decoder_is_finished(const AvfDecoder *decoder, int32_t *out_finished);

AVIOFLOW_API int avf_decoder_get_metadata(const AvfDecoder *decoder,
                             AvfMetadata *out_metadata);

/* ------------------------------------------------------------------------- */
/* Encoder                                                                    */
/* ------------------------------------------------------------------------- */

typedef struct AvfEncoder AvfEncoder;

/** @brief Create an encoder. Pass NULL for default options. */
AVIOFLOW_API AvfEncoder *avf_encoder_new(const AvfWriteOptions *options);

AVIOFLOW_API void avf_encoder_free(AvfEncoder *encoder);

/**
 * @brief Write planar float samples to @p path.
 *
 * @param channels Array of @p num_channels pointers, each to @p num_samples
 * floats.
 */
AVIOFLOW_API int avf_encoder_save(AvfEncoder *encoder, const char *path,
                     const float *const *channels, int32_t num_channels,
                     int64_t num_samples);

/** @brief One-shot encode, equivalent to avioflow::save_audio(). */
AVIOFLOW_API int avf_save_audio(const char *path, const float *const *channels,
                   int32_t num_channels, int64_t num_samples,
                   const AvfWriteOptions *options);

/* ------------------------------------------------------------------------- */
/* Resampler                                                                  */
/* ------------------------------------------------------------------------- */

typedef struct AvfResampler AvfResampler;

/** @brief Create a stateful resampler. @p options is required. */
AVIOFLOW_API AvfResampler *avf_resampler_new(const AvfResampleOptions *options);

AVIOFLOW_API void avf_resampler_free(AvfResampler *resampler);

/**
 * @brief Resample one chunk, preserving filter state across calls.
 *
 * May emit fewer samples than the rate ratio suggests; the remainder is held
 * internally until avf_resampler_flush().
 */
AVIOFLOW_API int avf_resampler_process(AvfResampler *resampler, const float *const *channels,
                          int32_t num_channels, int64_t num_samples,
                          AvfSamples **out_samples);

/**
 * @brief Drain buffered samples. Call once after the final process() call;
 * skipping it drops the tail of the audio.
 */
AVIOFLOW_API int avf_resampler_flush(AvfResampler *resampler, AvfSamples **out_samples);

AVIOFLOW_API int avf_resampler_output_sample_rate(const AvfResampler *resampler,
                                     int32_t *out_rate);

/** @brief Output channel count. Zero until the first process() call. */
AVIOFLOW_API int avf_resampler_output_num_channels(const AvfResampler *resampler,
                                      int32_t *out_channels);

/**
 * @brief Resample a complete buffer in one call, flushing internally.
 *
 * Set has_output_num_channels to 0 to keep the input channel count.
 */
AVIOFLOW_API int avf_resample(const float *const *channels, int32_t num_channels,
                 int64_t num_samples, int32_t input_sample_rate,
                 int32_t output_sample_rate, int32_t output_num_channels,
                 int32_t has_output_num_channels, AvfSamples **out_samples);

/* ------------------------------------------------------------------------- */
/* Device enumeration                                                         */
/* ------------------------------------------------------------------------- */

/** Opaque owned device list. Release with avf_device_list_free(). */
typedef struct AvfDeviceList AvfDeviceList;

AVIOFLOW_API int avf_list_audio_devices(AvfDeviceList **out_list);

AVIOFLOW_API size_t avf_device_list_size(const AvfDeviceList *list);

/** @return NULL when @p index is out of range. Valid until the list is freed. */
AVIOFLOW_API const char *avf_device_list_name(const AvfDeviceList *list, size_t index);
AVIOFLOW_API const char *avf_device_list_description(const AvfDeviceList *list, size_t index);

/** @return Non-zero for output/loopback devices; 0 also when out of range. */
AVIOFLOW_API int32_t avf_device_list_is_output(const AvfDeviceList *list, size_t index);

AVIOFLOW_API void avf_device_list_free(AvfDeviceList *list);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AVIOFLOW_C_API_H */
