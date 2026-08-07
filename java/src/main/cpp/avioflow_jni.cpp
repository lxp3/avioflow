#include "avioflow-cxx-api.h"
#include "metadata.h"
#include <jni.h>
#include <cmath>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

using namespace avioflow;

namespace {

void throw_exception(JNIEnv *env, const char *class_name, const std::string &message) {
  jclass cls = env->FindClass(class_name);
  if (cls) {
    env->ThrowNew(cls, message.c_str());
    env->DeleteLocalRef(cls);
  }
}

void throw_avioflow(JNIEnv *env, const std::exception &error) {
  throw_exception(env, "io/github/lxp3/avioflow/AvioflowException", error.what());
}

std::string jstring_to_string(JNIEnv *env, jstring value) {
  if (!value) {
    return {};
  }
  const char *chars = env->GetStringUTFChars(value, nullptr);
  std::string result(chars ? chars : "");
  env->ReleaseStringUTFChars(value, chars);
  return result;
}

jstring optional_string_field(JNIEnv *env, jobject object, const char *field_name) {
  jclass cls = env->GetObjectClass(object);
  jfieldID field = env->GetFieldID(cls, field_name, "Ljava/lang/String;");
  jstring result = field ? static_cast<jstring>(env->GetObjectField(object, field)) : nullptr;
  env->DeleteLocalRef(cls);
  return result;
}

jobject object_field(JNIEnv *env, jobject object, const char *field_name, const char *signature) {
  jclass cls = env->GetObjectClass(object);
  jfieldID field = env->GetFieldID(cls, field_name, signature);
  jobject result = field ? env->GetObjectField(object, field) : nullptr;
  env->DeleteLocalRef(cls);
  return result;
}

bool boolean_field(JNIEnv *env, jobject object, const char *field_name, bool default_value) {
  jclass cls = env->GetObjectClass(object);
  jfieldID field = env->GetFieldID(cls, field_name, "Z");
  bool result = field ? env->GetBooleanField(object, field) == JNI_TRUE : default_value;
  env->DeleteLocalRef(cls);
  return result;
}

// Reads a primitive int field, as opposed to a boxed java.lang.Integer.
int int_field(JNIEnv *env, jobject object, const char *field_name) {
  jclass cls = env->GetObjectClass(object);
  jfieldID field = env->GetFieldID(cls, field_name, "I");
  int result = field ? env->GetIntField(object, field) : 0;
  env->DeleteLocalRef(cls);
  return result;
}

int integer_value(JNIEnv *env, jobject value) {
  jclass cls = env->FindClass("java/lang/Integer");
  jmethodID method = env->GetMethodID(cls, "intValue", "()I");
  int result = env->CallIntMethod(value, method);
  env->DeleteLocalRef(cls);
  return result;
}

int64_t long_value(JNIEnv *env, jobject value) {
  jclass cls = env->FindClass("java/lang/Long");
  jmethodID method = env->GetMethodID(cls, "longValue", "()J");
  int64_t result = env->CallLongMethod(value, method);
  env->DeleteLocalRef(cls);
  return result;
}

AudioStreamOptions to_stream_options(JNIEnv *env, jobject options) {
  AudioStreamOptions result;
  if (!options) {
    return result;
  }

  jobject output_sample_rate = object_field(env, options, "outputSampleRate", "Ljava/lang/Integer;");
  if (output_sample_rate) {
    result.output_sample_rate = integer_value(env, output_sample_rate);
    env->DeleteLocalRef(output_sample_rate);
  }

  jobject output_num_channels = object_field(env, options, "outputNumChannels", "Ljava/lang/Integer;");
  if (output_num_channels) {
    result.output_num_channels = integer_value(env, output_num_channels);
    env->DeleteLocalRef(output_num_channels);
  }

  jobject input_sample_rate = object_field(env, options, "inputSampleRate", "Ljava/lang/Integer;");
  if (input_sample_rate) {
    result.input_sample_rate = integer_value(env, input_sample_rate);
    env->DeleteLocalRef(input_sample_rate);
  }

  jobject input_channels = object_field(env, options, "inputChannels", "Ljava/lang/Integer;");
  if (input_channels) {
    result.input_channels = integer_value(env, input_channels);
    env->DeleteLocalRef(input_channels);
  }

  jstring input_format = optional_string_field(env, options, "inputFormat");
  if (input_format) {
    result.input_format = jstring_to_string(env, input_format);
    env->DeleteLocalRef(input_format);
  }

  return result;
}

AudioResampleOptions to_resample_options(JNIEnv *env, jobject options) {
  AudioResampleOptions result;
  if (!options) {
    return result;
  }

  // Both rates are primitive ints on the Java side, so they are always present;
  // the C++ constructor rejects non-positive values.
  result.input_sample_rate = int_field(env, options, "inputSampleRate");
  result.output_sample_rate = int_field(env, options, "outputSampleRate");

  jobject output_num_channels = object_field(env, options, "outputNumChannels", "Ljava/lang/Integer;");
  if (output_num_channels) {
    result.output_num_channels = integer_value(env, output_num_channels);
    env->DeleteLocalRef(output_num_channels);
  }

  return result;
}

AudioWriteOptions to_write_options(JNIEnv *env, jobject options) {
  AudioWriteOptions result;
  if (!options) {
    return result;
  }

  jstring codec_name = optional_string_field(env, options, "codecName");
  if (codec_name) {
    result.codec_name = jstring_to_string(env, codec_name);
    env->DeleteLocalRef(codec_name);
  }

  jstring container_format = optional_string_field(env, options, "containerFormat");
  if (container_format) {
    result.container_format = jstring_to_string(env, container_format);
    env->DeleteLocalRef(container_format);
  }

  jobject sample_rate = object_field(env, options, "sampleRate", "Ljava/lang/Integer;");
  if (sample_rate) {
    result.sample_rate = integer_value(env, sample_rate);
    env->DeleteLocalRef(sample_rate);
  }

  jobject num_channels = object_field(env, options, "numChannels", "Ljava/lang/Integer;");
  if (num_channels) {
    result.num_channels = integer_value(env, num_channels);
    env->DeleteLocalRef(num_channels);
  }

  jobject bit_rate = object_field(env, options, "bitRate", "Ljava/lang/Long;");
  if (bit_rate) {
    result.bit_rate = long_value(env, bit_rate);
    env->DeleteLocalRef(bit_rate);
  }

  jstring sample_format = optional_string_field(env, options, "sampleFormat");
  if (sample_format) {
    result.sample_format = jstring_to_string(env, sample_format);
    env->DeleteLocalRef(sample_format);
  }

  result.overwrite = boolean_field(env, options, "overwrite", true);
  return result;
}

jobject to_metadata(JNIEnv *env, const Metadata &metadata) {
  jclass cls = env->FindClass("io/github/lxp3/avioflow/Metadata");
  jmethodID constructor = env->GetMethodID(
      cls,
      "<init>",
      "(DJIILjava/lang/String;Ljava/lang/String;JLjava/lang/String;)V");
  jstring sample_format = env->NewStringUTF(metadata.sample_format.c_str());
  jstring codec = env->NewStringUTF(metadata.codec.c_str());
  jstring container = env->NewStringUTF(metadata.container.c_str());
  jobject result = env->NewObject(
      cls,
      constructor,
      metadata.duration,
      static_cast<jlong>(metadata.num_samples),
      metadata.sample_rate,
      metadata.num_channels,
      sample_format,
      codec,
      static_cast<jlong>(metadata.bit_rate),
      container);
  env->DeleteLocalRef(sample_format);
  env->DeleteLocalRef(codec);
  env->DeleteLocalRef(container);
  env->DeleteLocalRef(cls);
  return result;
}

jobjectArray to_string_array(JNIEnv *env, const std::vector<std::string> &values) {
  jclass string_cls = env->FindClass("java/lang/String");
  auto result = env->NewObjectArray(static_cast<jsize>(values.size()), string_cls, nullptr);
  for (jsize i = 0; i < static_cast<jsize>(values.size()); ++i) {
    jstring value = env->NewStringUTF(values[static_cast<size_t>(i)].c_str());
    env->SetObjectArrayElement(result, i, value);
    env->DeleteLocalRef(value);
  }
  env->DeleteLocalRef(string_cls);
  return result;
}

jobjectArray to_float_2d_array(JNIEnv *env, const std::vector<std::vector<float>> &samples) {
  jclass float_array_cls = env->FindClass("[F");
  auto result = env->NewObjectArray(static_cast<jsize>(samples.size()), float_array_cls, nullptr);
  for (jsize channel = 0; channel < static_cast<jsize>(samples.size()); ++channel) {
    const auto &channel_samples = samples[static_cast<size_t>(channel)];
    jfloatArray row = env->NewFloatArray(static_cast<jsize>(channel_samples.size()));
    if (!channel_samples.empty()) {
      env->SetFloatArrayRegion(
          row,
          0,
          static_cast<jsize>(channel_samples.size()),
          channel_samples.data());
    }
    env->SetObjectArrayElement(result, channel, row);
    env->DeleteLocalRef(row);
  }
  env->DeleteLocalRef(float_array_cls);
  return result;
}

jobjectArray to_float_2d_array(JNIEnv *env, const FrameData &frame) {
  if (!frame) {
    return nullptr;
  }
  jclass float_array_cls = env->FindClass("[F");
  jobjectArray result = env->NewObjectArray(frame.num_channels, float_array_cls, nullptr);
  for (int channel = 0; channel < frame.num_channels; ++channel) {
    jfloatArray row = env->NewFloatArray(frame.num_samples);
    env->SetFloatArrayRegion(row, 0, frame.num_samples, frame.data[channel]);
    env->SetObjectArrayElement(result, channel, row);
    env->DeleteLocalRef(row);
  }
  env->DeleteLocalRef(float_array_cls);
  return result;
}

jobjectArray to_device_array(JNIEnv *env, const std::vector<DeviceInfo> &devices) {
  jclass cls = env->FindClass("io/github/lxp3/avioflow/DeviceInfo");
  jmethodID constructor = env->GetMethodID(
      cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Z)V");
  jobjectArray result = env->NewObjectArray(
      static_cast<jsize>(devices.size()), cls, nullptr);
  for (jsize i = 0; i < static_cast<jsize>(devices.size()); ++i) {
    const DeviceInfo &device = devices[static_cast<size_t>(i)];
    jstring name = env->NewStringUTF(device.name.c_str());
    jstring description = env->NewStringUTF(device.description.c_str());
    jobject value = env->NewObject(cls, constructor, name, description,
                                   device.is_output ? JNI_TRUE : JNI_FALSE);
    env->SetObjectArrayElement(result, i, value);
    env->DeleteLocalRef(value);
    env->DeleteLocalRef(description);
    env->DeleteLocalRef(name);
  }
  env->DeleteLocalRef(cls);
  return result;
}

std::vector<std::vector<float>> from_float_2d_array(JNIEnv *env, jobjectArray samples) {
  std::vector<std::vector<float>> result;
  if (!samples) {
    return result;
  }

  jsize channels = env->GetArrayLength(samples);
  result.reserve(static_cast<size_t>(channels));
  for (jsize channel = 0; channel < channels; ++channel) {
    auto row = static_cast<jfloatArray>(env->GetObjectArrayElement(samples, channel));
    if (!row) {
      result.emplace_back();
      continue;
    }
    jsize size = env->GetArrayLength(row);
    std::vector<float> channel_samples(static_cast<size_t>(size));
    if (size > 0) {
      env->GetFloatArrayRegion(row, 0, size, channel_samples.data());
    }
    result.emplace_back(std::move(channel_samples));
    env->DeleteLocalRef(row);
  }
  return result;
}

AudioDecoder *decoder_from_handle(jlong handle) {
  return reinterpret_cast<AudioDecoder *>(static_cast<intptr_t>(handle));
}

AudioEncoder *encoder_from_handle(jlong handle) {
  return reinterpret_cast<AudioEncoder *>(static_cast<intptr_t>(handle));
}

AudioResampler *resampler_from_handle(jlong handle) {
  return reinterpret_cast<AudioResampler *>(static_cast<intptr_t>(handle));
}

} // namespace

extern "C" JNIEXPORT void JNICALL
Java_io_github_lxp3_avioflow_Avioflow_nativeSetLogLevel(JNIEnv *env, jclass, jstring level) {
  try {
    std::string value = jstring_to_string(env, level);
    avioflow_set_log_level(value.empty() ? nullptr : value.c_str());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_Avioflow_nativeGetSupportedDecoders(JNIEnv *env, jclass) {
  try {
    return to_string_array(env, get_supported_decoders());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_Avioflow_nativeGetSupportedEncoders(JNIEnv *env, jclass) {
  try {
    return to_string_array(env, get_supported_encoders());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_Avioflow_nativeGetSupportedInputFormats(JNIEnv *env, jclass) {
  try {
    return to_string_array(env, get_supported_input_formats());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_Avioflow_nativeGetSupportedOutputFormats(JNIEnv *env, jclass) {
  try {
    return to_string_array(env, get_supported_output_formats());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_Avioflow_nativeListAudioDevices(JNIEnv *env, jclass) {
  try {
    return to_device_array(env, DeviceManager::list_audio_devices());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeCreate(JNIEnv *env, jclass, jobject options) {
  try {
    return reinterpret_cast<jlong>(new AudioDecoder(to_stream_options(env, options)));
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return 0;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeLoadFile(JNIEnv *env, jclass, jlong handle, jstring path) {
  try {
    Metadata metadata = decoder_from_handle(handle)->load_file(jstring_to_string(env, path));
    return to_metadata(env, metadata);
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeLoadBuffer(JNIEnv *env, jclass, jlong handle, jbyteArray data) {
  try {
    jsize size = env->GetArrayLength(data);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (size > 0) {
      env->GetByteArrayRegion(data, 0, size, reinterpret_cast<jbyte *>(bytes.data()));
    }
    Metadata metadata = decoder_from_handle(handle)->load_buffer(bytes.data(), bytes.size());
    return to_metadata(env, metadata);
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeFeed(JNIEnv *env, jclass, jlong handle, jbyteArray data) {
  try {
    jsize size = env->GetArrayLength(data);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (size > 0) {
      env->GetByteArrayRegion(data, 0, size, reinterpret_cast<jbyte *>(bytes.data()));
    }
    decoder_from_handle(handle)->feed(bytes.data(), bytes.size());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
  }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeFlush(JNIEnv *env, jclass, jlong handle) {
  try {
    decoder_from_handle(handle)->flush();
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeGetSamples(JNIEnv *env, jclass, jlong handle,
                                                            jdouble start_seconds,
                                                            jdouble stop_seconds) {
  try {
    std::optional<double> stop = std::isnan(stop_seconds)
        ? std::nullopt
        : std::optional<double>(stop_seconds);
    return to_float_2d_array(env, decoder_from_handle(handle)->get_samples(start_seconds, stop));
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeGetFrame(JNIEnv *env, jclass, jlong handle) {
  try {
    return to_float_2d_array(env, decoder_from_handle(handle)->get_frame());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeGetMetadata(JNIEnv *env, jclass, jlong handle) {
  try {
    return to_metadata(env, decoder_from_handle(handle)->get_metadata());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeIsFinished(JNIEnv *env, jclass, jlong handle) {
  try {
    return decoder_from_handle(handle)->is_finished() ? JNI_TRUE : JNI_FALSE;
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return JNI_FALSE;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lxp3_avioflow_AudioDecoder_nativeDestroy(JNIEnv *, jclass, jlong handle) {
  delete decoder_from_handle(handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_lxp3_avioflow_AudioEncoder_nativeCreate(JNIEnv *env, jclass, jobject options) {
  try {
    return reinterpret_cast<jlong>(new AudioEncoder(to_write_options(env, options)));
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return 0;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lxp3_avioflow_AudioEncoder_nativeSave(JNIEnv *env, jclass, jlong handle, jstring path, jobjectArray samples) {
  try {
    encoder_from_handle(handle)->save(jstring_to_string(env, path), from_float_2d_array(env, samples));
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
  }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lxp3_avioflow_AudioEncoder_nativeDestroy(JNIEnv *, jclass, jlong handle) {
  delete encoder_from_handle(handle);
}

// --- AudioResampler ---

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_lxp3_avioflow_AudioResampler_nativeCreate(JNIEnv *env, jclass, jobject options) {
  try {
    return reinterpret_cast<jlong>(new AudioResampler(to_resample_options(env, options)));
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return 0;
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_AudioResampler_nativeProcess(JNIEnv *env, jclass, jlong handle,
                                                          jobjectArray samples) {
  try {
    return to_float_2d_array(
        env, resampler_from_handle(handle)->process(from_float_2d_array(env, samples)));
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_AudioResampler_nativeFlush(JNIEnv *env, jclass, jlong handle) {
  try {
    return to_float_2d_array(env, resampler_from_handle(handle)->flush());
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_lxp3_avioflow_AudioResampler_nativeOutputSampleRate(JNIEnv *env, jclass,
                                                                   jlong handle) {
  try {
    return resampler_from_handle(handle)->output_sample_rate();
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return 0;
  }
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_lxp3_avioflow_AudioResampler_nativeOutputNumChannels(JNIEnv *env, jclass,
                                                                     jlong handle) {
  try {
    return resampler_from_handle(handle)->output_num_channels();
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return 0;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_lxp3_avioflow_AudioResampler_nativeDestroy(JNIEnv *, jclass, jlong handle) {
  delete resampler_from_handle(handle);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_io_github_lxp3_avioflow_AudioResampler_nativeResample(JNIEnv *env, jclass,
                                                            jobjectArray samples,
                                                            jint input_sample_rate,
                                                            jint output_sample_rate,
                                                            jint output_num_channels) {
  try {
    // -1 is the documented "keep the input channel count" sentinel.
    std::optional<int> channels;
    if (output_num_channels > 0) {
      channels = output_num_channels;
    }
    return to_float_2d_array(env, resample(from_float_2d_array(env, samples),
                                           input_sample_rate, output_sample_rate, channels));
  } catch (const std::exception &error) {
    throw_avioflow(env, error);
    return nullptr;
  }
}
