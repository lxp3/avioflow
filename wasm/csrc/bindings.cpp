/**
 * @file bindings.cpp
 * @brief WebAssembly bindings for avioflow using Emscripten
 *
 * Provides audio decoding capabilities for browser and Electron environments.
 * This avoids Node.js native addon ABI compatibility issues.
 */

#include "avioflow-cxx-api.h"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

using namespace emscripten;
using namespace avioflow;

// --- Helper Functions ---

/**
 * @brief Convert Metadata to JS object
 */
val MetadataToJs(const Metadata &meta) {
    val obj = val::object();
    obj.set("duration", meta.duration);
    obj.set("sampleRate", meta.sample_rate);
    obj.set("numChannels", meta.num_channels);
    obj.set("codec", std::string(meta.codec));
    obj.set("numSamples", static_cast<double>(meta.num_samples));
    obj.set("sampleFormat", std::string(meta.sample_format));
    obj.set("bitRate", static_cast<double>(meta.bit_rate));
    obj.set("container", std::string(meta.container));
    return obj;
}

val StringsToJs(const std::vector<std::string> &values) {
    val result = val::array();
    for (size_t i = 0; i < values.size(); ++i) {
        result.set(i, values[i]);
    }
    return result;
}

val getSupportedDecoders() { return StringsToJs(get_supported_decoders()); }
val getSupportedEncoders() { return StringsToJs(get_supported_encoders()); }
val getSupportedInputFormats() { return StringsToJs(get_supported_input_formats()); }
val getSupportedOutputFormats() { return StringsToJs(get_supported_output_formats()); }

/**
 * @brief Convert samples to JS Float32Array per channel
 */
val SamplesToJs(const std::vector<std::vector<float>> &samples) {
    val result = val::array();
    for (size_t c = 0; c < samples.size(); ++c) {
        // Create typed array view
        val channelData = val::global("Float32Array").new_(samples[c].size());
        channelData.call<void>("set", val(typed_memory_view(
            samples[c].size(), samples[c].data())));
        result.call<void>("push", channelData);
    }
    return result;
}

// --- AudioDecoder Wrapper Class ---

/**
 * @class AudioDecoderWrapper
 * @brief WASM-friendly wrapper for AudioDecoder
 */
class AudioDecoderWrapper {
public:
    AudioDecoderWrapper() : decoder_(AudioStreamOptions{}) {}
    
    AudioDecoderWrapper(val options) {
        AudioStreamOptions opts;
        if (options.hasOwnProperty("outputSampleRate")) {
            opts.output_sample_rate = options["outputSampleRate"].as<int>();
        }
        if (options.hasOwnProperty("outputNumChannels")) {
            opts.output_num_channels = options["outputNumChannels"].as<int>();
        }
        if (options.hasOwnProperty("inputSampleRate")) {
            opts.input_sample_rate = options["inputSampleRate"].as<int>();
        }
        if (options.hasOwnProperty("inputChannels")) {
            opts.input_channels = options["inputChannels"].as<int>();
        }
        if (options.hasOwnProperty("inputFormat")) {
            opts.input_format = options["inputFormat"].as<std::string>();
        }
        decoder_ = AudioDecoder(opts);
    }

    /**
     * @brief Open audio from URL (uses Emscripten's fetch)
     */
    val loadFile(const std::string &source) {
        auto metadata = decoder_.load_file(source);
        return MetadataToJs(metadata);
    }

    /**
     * @brief Open audio from memory buffer (ArrayBuffer/Uint8Array)
     */
    val loadBuffer(val buffer) {
        // Convert JS ArrayBuffer/Uint8Array to vector
        std::vector<uint8_t> data;
        
        if (buffer.instanceof(val::global("Uint8Array"))) {
            unsigned int length = buffer["length"].as<unsigned int>();
            data.resize(length);
            val(typed_memory_view(data.size(), data.data())).call<void>("set", buffer);
        } else if (buffer.instanceof(val::global("ArrayBuffer"))) {
            val uint8View = val::global("Uint8Array").new_(buffer);
            unsigned int length = uint8View["length"].as<unsigned int>();
            data.resize(length);
            val(typed_memory_view(data.size(), data.data())).call<void>("set", uint8View);
        } else {
            val::global("console").call<void>("error", std::string("Expected Uint8Array or ArrayBuffer"));
            return val::null();
        }

        auto metadata = decoder_.load_buffer(data.data(), data.size());
        return MetadataToJs(metadata);
    }

    /**
     * @brief Push data for streaming decode
     */
    void feed(val buffer) {
        std::vector<uint8_t> data;
        
        if (buffer.instanceof(val::global("Uint8Array"))) {
            unsigned int length = buffer["length"].as<unsigned int>();
            data.resize(length);
            val(typed_memory_view(data.size(), data.data())).call<void>("set", buffer);
        } else {
            val::global("console").call<void>("error", std::string("Expected Uint8Array"));
            return;
        }

        decoder_.feed(data.data(), data.size());
    }

    void flush() {
        decoder_.flush();
    }

    /**
     * @brief Decode next frame
     * @return Float32Array[] or null
     */
    val getFrame() {
        auto frame = decoder_.get_frame();
        if (!frame) {
            return val::null();
        }

        val result = val::array();
        for (int c = 0; c < frame.num_channels; ++c) {
            val channelData = val::global("Float32Array").new_(frame.num_samples);
            for (int i = 0; i < frame.num_samples; ++i) {
                channelData.set(i, frame.data[c][i]);
            }
            result.call<void>("push", channelData);
        }
        return result;
    }

    /**
     * @brief Decode all samples at once
     */
    val getSamples() {
        auto samples = decoder_.get_samples();
        return SamplesToJs(samples);
    }

    /**
     * @brief Get metadata
     */
    val getMetadata() {
        return MetadataToJs(decoder_.get_metadata());
    }

    /**
     * @brief Check if finished
     */
    bool isFinished() {
        return decoder_.is_finished();
    }

private:
    AudioDecoder decoder_;
};

// --- Module-level Functions ---

/**
 * @brief Set FFmpeg log level
 */
void setLogLevel(const std::string &level) {
    avioflow_set_log_level(level.c_str());
}

/**
 * @brief Quick load helper - load and decode entire file
 */
val load(const std::string &path, val options) {
    AudioStreamOptions opts;
    if (!options.isUndefined() && !options.isNull()) {
        if (options.hasOwnProperty("outputSampleRate")) {
            opts.output_sample_rate = options["outputSampleRate"].as<int>();
        }
        if (options.hasOwnProperty("outputNumChannels")) {
            opts.output_num_channels = options["outputNumChannels"].as<int>();
        }
    }
    
    AudioDecoder decoder(opts);
    decoder.load_file(path);

    auto meta = decoder.get_metadata();
    auto samples = decoder.get_samples();

    val result = val::object();
    result.set("metadata", MetadataToJs(meta));
    result.set("samples", SamplesToJs(samples));
    return result;
}

/**
 * @brief Load from ArrayBuffer
 */
val loadBuffer(val buffer, val options) {
    AudioStreamOptions opts;
    if (!options.isUndefined() && !options.isNull()) {
        if (options.hasOwnProperty("outputSampleRate")) {
            opts.output_sample_rate = options["outputSampleRate"].as<int>();
        }
        if (options.hasOwnProperty("outputNumChannels")) {
            opts.output_num_channels = options["outputNumChannels"].as<int>();
        }
    }
    
    // Convert buffer
    std::vector<uint8_t> data;
    if (buffer.instanceof(val::global("Uint8Array"))) {
        unsigned int length = buffer["length"].as<unsigned int>();
        data.resize(length);
        val(typed_memory_view(data.size(), data.data())).call<void>("set", buffer);
    } else if (buffer.instanceof(val::global("ArrayBuffer"))) {
        val uint8View = val::global("Uint8Array").new_(buffer);
        unsigned int length = uint8View["length"].as<unsigned int>();
        data.resize(length);
        val(typed_memory_view(data.size(), data.data())).call<void>("set", uint8View);
    } else {
        val::global("console").call<void>("error", std::string("Expected Uint8Array or ArrayBuffer"));
        return val::null();
    }

    AudioDecoder decoder(opts);
    decoder.load_buffer(data.data(), data.size());

    auto meta = decoder.get_metadata();
    auto samples = decoder.get_samples();

    val result = val::object();
    result.set("metadata", MetadataToJs(meta));
    result.set("samples", SamplesToJs(samples));
    return result;
}

void save(const std::string &path, val channels, val options) {
    const unsigned int num_channels = channels["length"].as<unsigned int>();
    std::vector<std::vector<float>> samples(num_channels);
    for (unsigned int channel = 0; channel < num_channels; ++channel) {
        val source = channels[channel];
        if (!source.instanceof(val::global("Float32Array"))) {
            throw std::invalid_argument("Every channel must be a Float32Array");
        }
        const unsigned int length = source["length"].as<unsigned int>();
        samples[channel].resize(length);
        val(typed_memory_view(samples[channel].size(), samples[channel].data()))
            .call<void>("set", source);
    }

    AudioWriteOptions write_options;
    if (!options.isUndefined() && !options.isNull()) {
        if (options.hasOwnProperty("codecName"))
            write_options.codec_name = options["codecName"].as<std::string>();
        if (options.hasOwnProperty("containerFormat"))
            write_options.container_format = options["containerFormat"].as<std::string>();
        if (options.hasOwnProperty("sampleRate"))
            write_options.sample_rate = options["sampleRate"].as<int>();
        if (options.hasOwnProperty("numChannels"))
            write_options.num_channels = options["numChannels"].as<int>();
        if (options.hasOwnProperty("bitRate"))
            write_options.bit_rate = options["bitRate"].as<int64_t>();
        if (options.hasOwnProperty("sampleFormat"))
            write_options.sample_format = options["sampleFormat"].as<std::string>();
        if (options.hasOwnProperty("overwrite"))
            write_options.overwrite = options["overwrite"].as<bool>();
    }
    save_audio(path, samples, write_options);
}

// --- Emscripten Bindings ---

EMSCRIPTEN_BINDINGS(avioflow) {
    // Module-level functions
    function("setLogLevel", &setLogLevel);
    function("load", &load);
    function("loadBuffer", &loadBuffer);
    function("getSupportedDecoders", &getSupportedDecoders);
    function("getSupportedEncoders", &getSupportedEncoders);
    function("getSupportedInputFormats", &getSupportedInputFormats);
    function("getSupportedOutputFormats", &getSupportedOutputFormats);
    function("save", &save);
    
    // AudioDecoder class
    class_<AudioDecoderWrapper>("AudioDecoder")
        .constructor<>()
        .constructor<val>()
        .function("loadFile", &AudioDecoderWrapper::loadFile)
        .function("loadBuffer", &AudioDecoderWrapper::loadBuffer)
        .function("feed", &AudioDecoderWrapper::feed)
        .function("flush", &AudioDecoderWrapper::flush)
        .function("getFrame", &AudioDecoderWrapper::getFrame)
        .function("getSamples", &AudioDecoderWrapper::getSamples)
        .function("getMetadata", &AudioDecoderWrapper::getMetadata)
        .function("isFinished", &AudioDecoderWrapper::isFinished);
}
