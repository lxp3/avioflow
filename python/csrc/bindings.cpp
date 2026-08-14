/**
 * @file bindings.cpp
 * @brief Python bindings for avioflow audio decoding library.
 * 
 * This module provides high-performance audio decoding capabilities powered by FFmpeg.
 * Audio data is returned as numpy arrays with shape (channels, samples).
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "avioflow-cxx-api.h"
#include "metadata.h"

namespace py = pybind11;
using namespace avioflow;

namespace {

struct SourceInput {
    enum class Kind {
        Path,
        Bytes
    };

    Kind kind;
    std::string path;
    py::object owner;
    const uint8_t* data = nullptr;
    size_t size = 0;
};

std::string source_type_error_message() {
    return "source must be str, PathLike, bytes, bytearray, memoryview, or io.BytesIO";
}

std::string data_type_error_message() {
    return "data must be bytes, bytearray, memoryview, or io.BytesIO";
}

SourceInput parse_source_input(const py::object& source) {
    if (py::isinstance<py::str>(source)) {
        return {
            SourceInput::Kind::Path,
            py::cast<std::string>(source),
            py::none(),
            nullptr,
            0
        };
    }

    if (py::hasattr(source, "__fspath__")) {
        return {
            SourceInput::Kind::Path,
            py::cast<std::string>(source.attr("__fspath__")()),
            py::none(),
            nullptr,
            0
        };
    }

    py::object buffer_owner = source;
    if (py::hasattr(source, "getbuffer")) {
        buffer_owner = source.attr("getbuffer")();
    }

    if (PyObject_CheckBuffer(buffer_owner.ptr()) != 0) {
        py::buffer_info info = py::buffer(buffer_owner).request();
        if (info.ndim > 1) {
            throw py::type_error("buffer input must be 1-dimensional and contiguous");
        }
        if (info.ndim == 1 && !info.strides.empty() && info.strides[0] != info.itemsize) {
            throw py::type_error("buffer input must be contiguous");
        }
        return {
            SourceInput::Kind::Bytes,
            "",
            buffer_owner,
            static_cast<const uint8_t*>(info.ptr),
            static_cast<size_t>(info.size * info.itemsize)
        };
    }

    throw py::type_error(source_type_error_message());
}

std::string parse_path_input(const py::object& source) {
    if (py::isinstance<py::str>(source)) {
        return py::cast<std::string>(source);
    }
    if (py::hasattr(source, "__fspath__")) {
        return py::cast<std::string>(source.attr("__fspath__")());
    }
    throw py::type_error("source must be str or PathLike");
}

SourceInput parse_data_input(const py::object& data) {
    SourceInput input = parse_source_input(data);
    if (input.kind != SourceInput::Kind::Bytes) {
        throw py::type_error(data_type_error_message());
    }
    return input;
}

void open_decoder_source(AudioDecoder& decoder, const py::object& source) {
    SourceInput input = parse_source_input(source);
    py::gil_scoped_release release;
    if (input.kind == SourceInput::Kind::Path) {
        decoder.load_file(input.path);
    } else {
        decoder.load_buffer(input.data, input.size);
    }
}

void push_decoder_data(AudioDecoder& decoder, const py::object& data) {
    SourceInput input = parse_data_input(data);
    if (input.size > 0) {
        py::gil_scoped_release release;
        decoder.feed(input.data, input.size);
    }
}

std::vector<std::vector<float>> get_decoder_samples(AudioDecoder& decoder,
                                                     double start_seconds,
                                                     std::optional<double> stop_seconds) {
    py::gil_scoped_release release;
    return decoder.get_samples(start_seconds, stop_seconds);
}

py::array_t<float> samples_to_array(const std::vector<std::vector<float>>& samples) {
    if (samples.empty()) {
        return py::array_t<float>(std::vector<size_t>{0, 0});
    }

    size_t num_channels = samples.size();
    size_t num_samples = samples[0].size();
    py::array_t<float> result({num_channels, num_samples});
    auto buf = result.mutable_unchecked<2>();
    {
        py::gil_scoped_release release;
        for (size_t c = 0; c < num_channels; ++c) {
            std::copy(samples[c].begin(), samples[c].end(), &buf(c, 0));
        }
    }
    return result;
}

// Converts a (channels, samples) array into the planar vectors the C++ API
// takes. Used by both save() and the resampling entry points.
std::vector<std::vector<float>> array_to_samples(
        const py::array_t<float, py::array::c_style | py::array::forcecast>& samples,
        const char* argument_name) {
    py::buffer_info buf = samples.request();
    if (buf.ndim != 2) {
        throw std::invalid_argument(
            std::string(argument_name) + " must be a 2D array with shape (channels, samples)");
    }

    const auto num_channels = static_cast<size_t>(buf.shape[0]);
    const auto num_samples = static_cast<size_t>(buf.shape[1]);
    std::vector<std::vector<float>> result(num_channels);
    const auto* ptr = static_cast<const float*>(buf.ptr);
    {
        py::gil_scoped_release release;
        for (size_t c = 0; c < num_channels; ++c) {
            result[c].assign(ptr + c * num_samples, ptr + (c + 1) * num_samples);
        }
    }
    return result;
}

std::vector<std::vector<float>> get_decoder_frame(AudioDecoder& decoder) {
    py::gil_scoped_release release;
    FrameData frame = decoder.get_frame();
    if (!frame) {
        return {};
    }

    std::vector<std::vector<float>> samples(frame.num_channels);
    for (size_t channel = 0; channel < frame.num_channels; ++channel) {
        samples[channel].assign(frame.data[channel],
                                frame.data[channel] + frame.num_samples);
    }
    return samples;
}

std::vector<std::string> get_supported_decoders_released() {
    py::gil_scoped_release release;
    return get_supported_decoders();
}

std::vector<std::string> get_supported_encoders_released() {
    py::gil_scoped_release release;
    return get_supported_encoders();
}

std::vector<std::string> get_supported_input_formats_released() {
    py::gil_scoped_release release;
    return get_supported_input_formats();
}

std::vector<std::string> get_supported_output_formats_released() {
    py::gil_scoped_release release;
    return get_supported_output_formats();
}

std::vector<DeviceInfo> list_audio_devices_released() {
    py::gil_scoped_release release;
    return DeviceManager::list_audio_devices();
}

}  // namespace

PYBIND11_MODULE(_avioflow, m) {
    m.doc() = R"pbdoc(
        avioflow: High-performance audio decoding library powered by FFmpeg.
        
        This module provides:
        - AudioDecoder: Main class for decoding audio files and streams
        - DeviceManager: System audio device discovery
        - Metadata: Audio stream metadata container
        
        Example:
            >>> import avioflow
            >>> decoder = avioflow.AudioDecoder(output_sample_rate=44100)
            >>> meta = decoder.load_file("audio.mp3")
            >>> samples = decoder.get_samples()  # shape: (channels, samples)
    )pbdoc";

    // --- Module-level functions ---
    
    m.def("set_log_level", [](const std::string& level) {
        avioflow_set_log_level(level.c_str());
    }, 
    py::arg("level") = "error",
    R"pbdoc(
        Set FFmpeg logging verbosity level.

        Args:
            level (str): Log level, one of:
                - "quiet": No output
                - "fatal": Only fatal errors
                - "error": All errors
                - "warning": Errors and warnings
                - "info": General information
                - "error": Default; errors only
                - "debug": Detailed debugging info
                - "trace": Maximum verbosity
    )pbdoc");

    m.def("get_supported_decoders", &get_supported_decoders_released,
    R"pbdoc(
        Get the list of supported audio decoder names.

        Returns:
            list[str]: Decoder names such as "mp3", "aac", and "pcm_s16le".
    )pbdoc");

    m.def("get_supported_encoders", &get_supported_encoders_released,
    R"pbdoc(
        Get the list of supported audio encoder names.

        Returns:
            list[str]: Encoder names such as "pcm_s16le", "aac", and "libmp3lame".
    )pbdoc");

    m.def("get_supported_input_formats", &get_supported_input_formats_released,
    R"pbdoc(
        Get the list of supported input format (demuxer) names.

        Returns:
            list[str]: Input format names such as "mp3", "wav", "flac", and "s16le".
    )pbdoc");

    // Get metadata only
    m.def("info", [](py::object source) {
        AudioStreamOptions opts;
        AudioDecoder decoder(opts);
        open_decoder_source(decoder, source);
        return decoder.get_metadata();
    },
    py::arg("source"),
    R"pbdoc(
        Get audio metadata without decoding samples.

        Args:
            source (str, PathLike, bytes-like, or BytesIO): Path/URL or encoded audio bytes.

        Returns:
            Metadata: Audio stream metadata (duration, sample_rate, etc.)
    )pbdoc");

    // Quick offline loading helper
    m.def("load", [](py::object source,
                     std::optional<int> output_sample_rate,
                     std::optional<int> output_num_channels) {
        AudioStreamOptions opts;
        opts.output_sample_rate = output_sample_rate;
        opts.output_num_channels = output_num_channels;
        
        AudioDecoder decoder(opts);
        
        // Support both file paths and bytes
        open_decoder_source(decoder, source);
        
        Metadata meta = decoder.get_metadata();
        auto samples = get_decoder_samples(decoder, 0.0, std::nullopt);

        return py::make_tuple(meta, samples_to_array(samples));
    },
    py::arg("source"),
    py::arg("output_sample_rate") = py::none(),
    py::arg("output_num_channels") = py::none(),
    R"pbdoc(
        Load an audio file and decode all samples in one call.
        
        This is a convenience function that combines file loading and full
        decoding in a single call. For large files or when you need frame-by-frame
        control, use AudioDecoder directly.
        
        Args:
            source (str or bytes): Path to audio file, URL, or audio file bytes.
            output_sample_rate (int, optional): Target sample rate in Hz.
                Use -1 or None to preserve the source sample rate.
            output_num_channels (int, optional): Target channel count.
                Use -1 or None to preserve the source channel count.
        
        Returns:
            tuple[Metadata, numpy.ndarray]: A tuple containing:
                - Metadata: Audio stream information
                - samples: Float32 array with shape (channels, samples)
        
        Example:
            >>> import avioflow
            >>> meta, samples = avioflow.load("audio.mp3")
            >>> print(f"Duration: {meta.duration}s, Shape: {samples.shape}")
            
            >>> # With resampling to 16kHz
            >>> meta, samples = avioflow.load("speech.wav", output_sample_rate=16000)
    )pbdoc");

    // --- DeviceInfo ---
    
    py::class_<DeviceInfo>(m, "DeviceInfo", 
        "Container for system audio device information.")
        .def_readonly("name", &DeviceInfo::name, 
            "str: Unique device identifier used for opening.")
        .def_readonly("description", &DeviceInfo::description, 
            "str: Human-readable device name.")
        .def_readonly("is_output", &DeviceInfo::is_output, 
            "bool: True if this is an output/loopback device.")
        .def("__repr__", [](const DeviceInfo& self) {
            return "<avioflow.DeviceInfo name='" + self.name + "'>";
        });

    // --- Metadata ---
    
    py::class_<Metadata>(m, "Metadata", 
        "Container for audio stream metadata.")
        .def_readonly("duration", &Metadata::duration, 
            "float: Duration in seconds. May be 0 for live streams.")
        .def_readonly("num_samples", &Metadata::num_samples, 
            "int: Total number of samples. Updated at EOF for streams.")
        .def_readonly("sample_rate", &Metadata::sample_rate, 
            "int: Sample rate in Hz (e.g., 44100, 48000).")
        .def_readonly("num_channels", &Metadata::num_channels, 
            "int: Number of audio channels (1=mono, 2=stereo).")
        .def_readonly("sample_format", &Metadata::sample_format, 
            "str: Original sample format (e.g., 'fltp', 's16').")
        .def_readonly("codec", &Metadata::codec, 
            "str: Codec name (e.g., 'mp3', 'aac', 'flac').")
        .def_readonly("bit_rate", &Metadata::bit_rate, 
            "int: Bit rate in bits per second.")
        .def_readonly("container", &Metadata::container, 
            "str: Container format (e.g., 'mp3', 'mp4', 'ogg').")
        .def("__repr__", [](const Metadata& self) {
            std::stringstream ss;
            ss << "duration: " << std::fixed << std::setprecision(3) << self.duration << "\n"
               << "num_samples: " << self.num_samples << "\n"
               << "sample_rate: " << self.sample_rate << "\n"
               << "num_channels: " << self.num_channels << "\n"
               << "sample_format: " << self.sample_format << "\n"
               << "codec: " << self.codec << "\n"
               << "bit_rate: " << self.bit_rate << "\n"
               << "container: " << self.container;
            return ss.str();
        });

    // --- AudioDecoder ---
    
    py::class_<AudioDecoder>(m, "AudioDecoder", R"pbdoc(
        High-performance audio decoder with file and streaming support.
        
        Two operation modes:
        
        **File Mode** - Load complete audio files:
            >>> decoder = AudioDecoder(output_sample_rate=44100)
            >>> meta = decoder.load_file("audio.mp3")
            >>> samples = decoder.get_samples()  # numpy array (channels, samples)
        
        **Stream Mode** - Decode real-time byte streams:
            >>> decoder = AudioDecoder(input_format="s16le", input_sample_rate=48000, input_channels=2)
            >>> decoder.feed(raw_bytes)
            >>> samples = decoder.get_samples()
        
        Args:
            output_sample_rate (int, optional): Target output sample rate in Hz.
                Use -1 or None to preserve the source sample rate.
            output_num_channels (int, optional): Target output channel count.
                Use -1 or None to preserve the source channel count.
            input_sample_rate (int, optional): Source sample rate for raw PCM streaming.
                Required for stream mode with raw PCM formats.
            input_channels (int, optional): Source channel count for raw PCM streaming.
                Required for stream mode with raw PCM formats.
            input_format (str, optional): Source format for streaming. Options:
                - "s16le": 16-bit signed little-endian PCM
                - "f32le": 32-bit float little-endian PCM
                - "aac": AAC audio (with ADTS headers)
                - "opus": Opus audio
                Required for stream mode.
        
        Attributes:
            All audio data is returned as float32 in range [-1.0, 1.0].
    )pbdoc")
        .def(py::init([](std::optional<int> output_sample_rate,
                        std::optional<int> output_num_channels,
                        std::optional<int> input_sample_rate,
                        std::optional<int> input_channels,
                        std::optional<std::string> input_format) {
            AudioStreamOptions options;
            options.output_sample_rate = output_sample_rate;
            options.output_num_channels = output_num_channels;
            options.input_sample_rate = input_sample_rate;
            options.input_channels = input_channels;
            options.input_format = input_format;
            return new AudioDecoder(options);
        }),
        py::arg("output_sample_rate") = py::none(),
        py::arg("output_num_channels") = py::none(),
        py::arg("input_sample_rate") = py::none(),
        py::arg("input_channels") = py::none(),
        py::arg("input_format") = py::none())
        
        .def("load_file", [](AudioDecoder& self, py::object source) -> const Metadata& {
            std::string path = parse_path_input(source);
            py::gil_scoped_release release;
            self.load_file(path);
            return self.get_metadata();
        }, 
        py::arg("source"), 
        py::return_value_policy::reference_internal,
        R"pbdoc(
            Load audio from file path, URL, or device.
            
            Args:
                source: Audio source, one of:
                    - str: File path or URL
                    - pathlib.Path: File path object
                    - bytes/bytearray/memoryview: Full encoded audio bytes
                    - io.BytesIO: In-memory encoded audio bytes
                    - "wasapi_loopback": Windows system audio capture
                    - "audio=DeviceName": Microphone/input device
            
            Returns:
                Metadata: Audio stream metadata object.
            
            Raises:
                RuntimeError: If source cannot be opened or decoded.
                TypeError: If source type is not supported.
            
            Example:
                >>> decoder = AudioDecoder()
                >>> meta = decoder.load_file("song.mp3")
                >>> print(f"Duration: {meta.duration}s, Sample rate: {meta.sample_rate}Hz")
        )pbdoc")
        
        .def("load_buffer", [](AudioDecoder& self, py::object source) -> const Metadata& {
            SourceInput input = parse_data_input(source);
            py::gil_scoped_release release;
            self.load_buffer(input.data, input.size);
            return self.get_metadata();
        },
        py::arg("source"),
        py::return_value_policy::reference_internal,
        R"pbdoc(
            Load a complete encoded audio buffer and return metadata.
        )pbdoc")

        .def("get_samples", [](AudioDecoder& self, double start_seconds,
                              std::optional<double> stop_seconds) -> py::array_t<float> {
            return samples_to_array(get_decoder_samples(self, start_seconds, stop_seconds));
        },
        py::arg("start_seconds") = 0.0,
        py::arg("stop_seconds") = py::none(),
        R"pbdoc(
            Decode samples in the half-open range [start_seconds, stop_seconds).

            In File Mode: Decodes the requested range (or until EOF if stop_seconds
            is None). May be called multiple times on the same decoder to fetch
            different ranges; each call seeks independently.
            In Stream Mode: start_seconds/stop_seconds are not supported; decodes
            all buffered data until more input is required.

            Args:
                start_seconds: Range start in seconds (offline mode only). Defaults
                    to the beginning.
                stop_seconds: Range end in seconds, exclusive (offline mode only).
                    Defaults to the end.

            Returns:
                numpy.ndarray: Audio samples with shape (channels, samples).
                    dtype is float32, values in range [-1.0, 1.0].

            Raises:
                ValueError: if start_seconds < 0 or stop_seconds <= start_seconds.

            Note:
                For large files, consider using frame-by-frame decoding (get_frame())
                to manage memory usage.

            Example:
                >>> decoder = AudioDecoder(output_sample_rate=16000)
                >>> decoder.load_file("speech.wav")
                >>> samples = decoder.get_samples()
                >>> ranged = decoder.get_samples(10.3, 20.3)
        )pbdoc")

        .def("feed", [](AudioDecoder& self, py::object data) {
            push_decoder_data(self, data);
        },
        py::arg("data"),
        R"pbdoc(
            Push raw audio data bytes to the decoder (stream mode only).

            Args:
                data (bytes): Raw audio data bytes

            Note:
                This method initializes the decoder on first call when enough data is buffered.
                Use get_frame() or get_samples() to retrieve decoded output.

            Example:
                >>> decoder = AudioDecoder(input_format="s16le", input_sample_rate=16000, input_channels=1)
                >>> with open("audio.raw", "rb") as f:
                ...     chunk = f.read(3200)  # 100ms @ 16kHz mono
                ...     decoder.feed(chunk)
                ...     samples = decoder.get_frame()
        )pbdoc")

        .def("flush", [](AudioDecoder& self) {
            py::gil_scoped_release release;
            self.flush();
        },
        R"pbdoc(
            Mark stream input as complete and allow decoder-delayed frames to drain.
        )pbdoc")

        .def("get_frame", [](AudioDecoder& self) -> py::object {
            auto samples = get_decoder_frame(self);
            if (samples.empty()) {
                return py::none();
            }
            return samples_to_array(samples);
        },
        R"pbdoc(
            Decode next audio frame.

            Returns:
                numpy.ndarray or None: Decoded audio samples with shape (channels, samples),
                    or None if no frame is available (need more data or EOF).

            Example:
                >>> while not decoder.is_finished():
                ...     frame = decoder.get_frame()
                ...     if frame is not None:
                ...         process(frame)
        )pbdoc")
        
        .def("is_finished", &AudioDecoder::is_finished,
        R"pbdoc(
            Check if end of stream has been reached.
            
            Returns:
                bool: True if all audio data has been decoded.
        )pbdoc")
        
        .def("get_metadata", &AudioDecoder::get_metadata,
        py::return_value_policy::reference_internal,
        R"pbdoc(
            Get current audio stream metadata.
            
            Returns:
                Metadata: Audio stream metadata object.
            
            Note:
                For stream mode, metadata is only available after first feed().
        )pbdoc");

    // --- AudioWriteOptions ---

    py::class_<AudioWriteOptions>(m, "AudioWriteOptions",
        "Options for audio encoding and writing.")
        .def(py::init<>())
        .def(py::init<const std::string&, std::optional<int>, std::optional<int>, std::optional<int64_t>>(),
            py::arg("format"),
            py::arg("sample_rate") = py::none(),
            py::arg("num_channels") = py::none(),
            py::arg("bit_rate") = py::none(),
            R"pbdoc(
                Create audio write options with format preset.

                Args:
                    format (str): Format preset - "wav", "flac", "aac", "mp3", "opus"
                    sample_rate (int, optional): Sample rate in Hz
                    num_channels (int, optional): Number of channels
                    bit_rate (int, optional): Bit rate for lossy codecs

                Example:
                    >>> opts = avioflow.AudioWriteOptions("mp3", sample_rate=44100, bit_rate=192000)
            )pbdoc")
        .def_readwrite("codec_name", &AudioWriteOptions::codec_name)
        .def_readwrite("container_format", &AudioWriteOptions::container_format)
        .def_readwrite("sample_rate", &AudioWriteOptions::sample_rate)
        .def_readwrite("num_channels", &AudioWriteOptions::num_channels)
        .def_readwrite("bit_rate", &AudioWriteOptions::bit_rate)
        .def_readwrite("sample_format", &AudioWriteOptions::sample_format)
        .def_readwrite("overwrite", &AudioWriteOptions::overwrite);

    m.def("save", [](const std::string& path,
                     py::array_t<float, py::array::c_style | py::array::forcecast> samples,
                     py::object options_obj) {
        std::vector<std::vector<float>> samples_vec = array_to_samples(samples, "samples");

        AudioWriteOptions options;
        if (!options_obj.is_none()) {
            options = py::cast<AudioWriteOptions>(options_obj);
        } else {
            // Auto-detect format from file extension
            std::string ext = path.substr(path.find_last_of('.') + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (!ext.empty()) {
                options = AudioWriteOptions(ext);
            }
        }

        {
            py::gil_scoped_release release;
            save_audio(path, samples_vec, options);
        }
    },
    py::arg("path"),
    py::arg("samples"),
    py::arg("options") = py::none(),
    R"pbdoc(
        Save audio samples to file.

        Args:
            path (str): Output file path
            samples (numpy.ndarray): Audio samples with shape (channels, samples)
            options (AudioWriteOptions, optional): Encoding options. If None, auto-detects from file extension.

        Example:
            >>> # Auto-detect format from extension
            >>> avioflow.save("output.mp3", samples)

            >>> # With explicit options
            >>> opts = avioflow.AudioWriteOptions("mp3", sample_rate=44100)
            >>> avioflow.save("output.mp3", samples, opts)
    )pbdoc");

    m.def("get_supported_output_formats", &get_supported_output_formats_released,
    R"pbdoc(
        Get the list of supported output format (muxer) names.

        Returns:
            list[str]: Output format names such as "wav", "flac", "mp4", and "adts".
    )pbdoc");

    // --- Resampling ---

    m.def("resample", [](py::array_t<float, py::array::c_style | py::array::forcecast> samples,
                         int input_sample_rate,
                         int output_sample_rate,
                         std::optional<int> output_num_channels) -> py::array_t<float> {
        std::vector<std::vector<float>> input = array_to_samples(samples, "samples");
        std::vector<std::vector<float>> output;
        {
            py::gil_scoped_release release;
            output = resample(input, input_sample_rate, output_sample_rate,
                              output_num_channels);
        }
        return samples_to_array(output);
    },
    py::arg("samples"),
    py::arg("input_sample_rate"),
    py::arg("output_sample_rate"),
    py::arg("output_num_channels") = py::none(),
    R"pbdoc(
        Resample a complete buffer of audio samples in one call.

        Handles the internal flush, so no samples are lost. For audio arriving in
        chunks use AudioResampler instead: calling this per chunk would reset the
        filter state and introduce a discontinuity at every boundary.

        Args:
            samples (numpy.ndarray): Input samples with shape (channels, samples).
            input_sample_rate (int): Source sample rate in Hz. Must be > 0.
            output_sample_rate (int): Target sample rate in Hz. Must be > 0.
            output_num_channels (int, optional): Target channel count. Defaults to
                the input channel count.

        Returns:
            numpy.ndarray: Resampled samples with shape (channels, samples).

        Raises:
            ValueError: If a sample rate is not positive, or the input is ragged.

        Example:
            >>> out = avioflow.resample(samples, 44100, 16000)
            >>> mono = avioflow.resample(samples, 44100, 16000, output_num_channels=1)
    )pbdoc");

    py::class_<AudioResampler>(m, "AudioResampler", R"pbdoc(
        Stateful resampler for audio that arrives in chunks.

        Filter state is preserved across process() calls, so consecutive chunks
        join without discontinuities at the boundaries. For a buffer you already
        hold in full, the resample() function is simpler.

        flush() is not optional: the resampler holds back the last few
        milliseconds of audio internally, and skipping the flush discards them.

        Example:
            >>> resampler = avioflow.AudioResampler(44100, 16000)
            >>> parts = [resampler.process(chunk) for chunk in chunks]
            >>> parts.append(resampler.flush())   # else the tail is lost
            >>> out = numpy.concatenate([p for p in parts if p.size], axis=1)
    )pbdoc")
        .def(py::init([](int input_sample_rate, int output_sample_rate,
                         std::optional<int> output_num_channels) {
            AudioResampleOptions options;
            options.input_sample_rate = input_sample_rate;
            options.output_sample_rate = output_sample_rate;
            options.output_num_channels = output_num_channels;
            return std::make_unique<AudioResampler>(options);
        }),
        py::arg("input_sample_rate"),
        py::arg("output_sample_rate"),
        py::arg("output_num_channels") = py::none(),
        R"pbdoc(
            Create a resampler.

            Args:
                input_sample_rate (int): Source sample rate in Hz. Must be > 0.
                output_sample_rate (int): Target sample rate in Hz. Must be > 0.
                output_num_channels (int, optional): Target channel count.
                    Defaults to the input channel count.

            Raises:
                ValueError: If a sample rate is not positive.
        )pbdoc")

        .def("process", [](AudioResampler& self,
                           py::array_t<float, py::array::c_style | py::array::forcecast> samples)
                        -> py::array_t<float> {
            std::vector<std::vector<float>> input = array_to_samples(samples, "samples");
            std::vector<std::vector<float>> output;
            {
                py::gil_scoped_release release;
                output = self.process(input);
            }
            return samples_to_array(output);
        },
        py::arg("samples"),
        R"pbdoc(
            Resample one chunk of samples.

            May return fewer samples than the rate ratio suggests, because the
            resampler buffers samples internally to keep filter continuity. The
            remainder is emitted by flush().

            Args:
                samples (numpy.ndarray): Input with shape (channels, samples). The
                    channel count must not change between calls.

            Returns:
                numpy.ndarray: Resampled samples with shape (channels, samples).

            Raises:
                ValueError: If the input is ragged or the channel count changed.
        )pbdoc")

        .def("flush", [](AudioResampler& self) -> py::array_t<float> {
            std::vector<std::vector<float>> output;
            {
                py::gil_scoped_release release;
                output = self.flush();
            }
            return samples_to_array(output);
        },
        R"pbdoc(
            Drain the samples still buffered inside the resampler.

            Call once after the final process() call. Skipping this drops the last
            few milliseconds of audio.

            Returns:
                numpy.ndarray: Remaining samples with shape (channels, samples).
        )pbdoc")

        .def_property_readonly("output_sample_rate", &AudioResampler::output_sample_rate,
            "int: The output sample rate the resampler was configured with.")

        .def_property_readonly("output_num_channels", &AudioResampler::output_num_channels,
            "int: Output channel count. Zero until the first process() call.");

    // --- DeviceManager ---

    py::class_<DeviceManager>(m, "DeviceManager",
        "Utility class for discovering system audio devices.")
        .def_static("list_audio_devices", &list_audio_devices_released,
        R"pbdoc(
            List available system audio devices.

            Returns:
                list[DeviceInfo]: List of available audio input/output devices.

            Example:
                >>> devices = avioflow.DeviceManager.list_audio_devices()
                >>> for dev in devices:
                ...     print(f"{dev.name}: {dev.description}")
        )pbdoc");
}
