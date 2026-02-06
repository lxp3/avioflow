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

/**
 * @brief Convert samples to JS Float32Array per channel
 */
val SamplesToJs(const std::vector<std::vector<float>> &samples) {
    val result = val::array();
    for (size_t c = 0; c < samples.size(); ++c) {
        // Create typed array view
        val channelData = val::global("Float32Array").new_(samples[c].size());
        // Copy data
        for (size_t i = 0; i < samples[c].size(); ++i) {
            channelData.set(i, samples[c][i]);
        }
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
    void open(const std::string &source) {
        decoder_.open(source);
    }

    /**
     * @brief Open audio from memory buffer (ArrayBuffer/Uint8Array)
     */
    void openBuffer(val buffer) {
        // Convert JS ArrayBuffer/Uint8Array to vector
        std::vector<uint8_t> data;
        
        if (buffer.instanceof(val::global("Uint8Array"))) {
            unsigned int length = buffer["length"].as<unsigned int>();
            data.resize(length);
            for (unsigned int i = 0; i < length; ++i) {
                data[i] = buffer[i].as<uint8_t>();
            }
        } else if (buffer.instanceof(val::global("ArrayBuffer"))) {
            val uint8View = val::global("Uint8Array").new_(buffer);
            unsigned int length = uint8View["length"].as<unsigned int>();
            data.resize(length);
            for (unsigned int i = 0; i < length; ++i) {
                data[i] = uint8View[i].as<uint8_t>();
            }
        } else {
            val::global("console").call<void>("error", std::string("Expected Uint8Array or ArrayBuffer"));
            return;
        }

        decoder_.open(data.data(), data.size());
    }

    /**
     * @brief Push data for streaming decode
     */
    void push(val buffer) {
        std::vector<uint8_t> data;
        
        if (buffer.instanceof(val::global("Uint8Array"))) {
            unsigned int length = buffer["length"].as<unsigned int>();
            data.resize(length);
            for (unsigned int i = 0; i < length; ++i) {
                data[i] = buffer[i].as<uint8_t>();
            }
        } else {
            val::global("console").call<void>("error", std::string("Expected Uint8Array"));
            return;
        }

        decoder_.push(data.data(), data.size());
    }

    /**
     * @brief Decode next frame
     * @return Float32Array[] or null
     */
    val decodeNext() {
        auto frame = decoder_.read();
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
    val getAllSamples() {
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
    decoder.open(path);

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
        for (unsigned int i = 0; i < length; ++i) {
            data[i] = buffer[i].as<uint8_t>();
        }
    } else if (buffer.instanceof(val::global("ArrayBuffer"))) {
        val uint8View = val::global("Uint8Array").new_(buffer);
        unsigned int length = uint8View["length"].as<unsigned int>();
        data.resize(length);
        for (unsigned int i = 0; i < length; ++i) {
            data[i] = uint8View[i].as<uint8_t>();
        }
    } else {
        val::global("console").call<void>("error", std::string("Expected Uint8Array or ArrayBuffer"));
        return val::null();
    }

    AudioDecoder decoder(opts);
    decoder.open(data.data(), data.size());

    auto meta = decoder.get_metadata();
    auto samples = decoder.get_samples();

    val result = val::object();
    result.set("metadata", MetadataToJs(meta));
    result.set("samples", SamplesToJs(samples));
    return result;
}

// --- Emscripten Bindings ---

EMSCRIPTEN_BINDINGS(avioflow) {
    // Module-level functions
    function("setLogLevel", &setLogLevel);
    function("load", &load);
    function("loadBuffer", &loadBuffer);
    
    // AudioDecoder class
    class_<AudioDecoderWrapper>("AudioDecoder")
        .constructor<>()
        .constructor<val>()
        .function("open", &AudioDecoderWrapper::open)
        .function("openBuffer", &AudioDecoderWrapper::openBuffer)
        .function("push", &AudioDecoderWrapper::push)
        .function("decodeNext", &AudioDecoderWrapper::decodeNext)
        .function("getAllSamples", &AudioDecoderWrapper::getAllSamples)
        .function("getMetadata", &AudioDecoderWrapper::getMetadata)
        .function("isFinished", &AudioDecoderWrapper::isFinished);
}
