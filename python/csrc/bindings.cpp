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

SourceInput parse_data_input(const py::object& data) {
    SourceInput input = parse_source_input(data);
    if (input.kind != SourceInput::Kind::Bytes) {
        throw py::type_error(data_type_error_message());
    }
    return input;
}

void open_decoder_source(AudioDecoder& decoder, const py::object& source) {
    SourceInput input = parse_source_input(source);
    if (input.kind == SourceInput::Kind::Path) {
        decoder.open(input.path);
    } else {
        decoder.open(input.data, input.size);
    }
}

void push_decoder_data(AudioDecoder& decoder, const py::object& data) {
    SourceInput input = parse_data_input(data);
    if (input.size > 0) {
        decoder.push(input.data, input.size);
    }
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
            >>> meta = decoder.load("audio.mp3")
            >>> samples = decoder.get_samples()  # shape: (channels, samples)
    )pbdoc";

    // --- Module-level functions ---
    
    m.def("set_log_level", [](const std::string& level) {
        avioflow_set_log_level(level.c_str());
    }, 
    py::arg("level") = "info",
    R"pbdoc(
        Set FFmpeg logging verbosity level.

        Args:
            level (str): Log level, one of:
                - "quiet": No output
                - "fatal": Only fatal errors
                - "error": All errors
                - "warning": Errors and warnings
                - "info": General information (default)
                - "debug": Detailed debugging info
                - "trace": Maximum verbosity
    )pbdoc");

    m.def("get_supported_decoders", &get_supported_decoders,
    R"pbdoc(
        Get the list of supported audio decoder names.

        Returns:
            list[str]: Decoder names such as "mp3", "aac", and "pcm_s16le".
    )pbdoc");

    m.def("get_supported_encoders", &get_supported_encoders,
    R"pbdoc(
        Get the list of supported audio encoder names.

        Returns:
            list[str]: Encoder names such as "pcm_s16le", "aac", and "libmp3lame".
    )pbdoc");

    m.def("get_supported_input_formats", &get_supported_input_formats,
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
                     std::optional<int> output_sample_rate) {
        AudioStreamOptions opts;
        opts.output_sample_rate = output_sample_rate;
        
        AudioDecoder decoder(opts);
        
        // Support both file paths and bytes
        open_decoder_source(decoder, source);
        
        const auto& meta = decoder.get_metadata();
        auto samples = decoder.get_samples();

        // Convert to numpy array
        if (samples.empty()) {
            std::vector<py::ssize_t> shape = {0, 0};
            auto arr = py::array_t<float>(shape);
            return py::make_tuple(meta, arr);
        }
        
        int num_channels = static_cast<int>(samples.size());
        int num_samples = static_cast<int>(samples[0].size());
        
        auto arr = py::array_t<float>({num_channels, num_samples});
        auto buf = arr.mutable_unchecked<2>();
        
        for (int c = 0; c < num_channels; ++c) {
            std::memcpy(&buf(c, 0), samples[c].data(), num_samples * sizeof(float));
        }
        
        return py::make_tuple(meta, arr);
    },
    py::arg("source"),
    py::arg("output_sample_rate") = py::none(),
    R"pbdoc(
        Load an audio file and decode all samples in one call.
        
        This is a convenience function that combines file loading and full
        decoding in a single call. For large files or when you need frame-by-frame
        control, use AudioDecoder directly.
        
        Args:
            source (str or bytes): Path to audio file, URL, or audio file bytes.
            output_sample_rate (int, optional): Target sample rate in Hz.
                If None or negative, uses source sample rate.
        
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
            >>> meta = decoder.load("audio.mp3")
            >>> samples = decoder.get_samples()  # numpy array (channels, samples)
        
        **Stream Mode** - Decode real-time byte streams:
            >>> decoder = AudioDecoder(input_format="s16le", input_sample_rate=48000, input_channels=2)
            >>> samples = decoder(raw_bytes)  # Returns decoded numpy array
        
        Args:
            output_sample_rate (int, optional): Target output sample rate in Hz.
                If not specified or negative, uses source sample rate.
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
                        std::optional<int> input_sample_rate,
                        std::optional<int> input_channels,
                        std::optional<std::string> input_format) {
            AudioStreamOptions options;
            options.output_sample_rate = output_sample_rate;
            options.input_sample_rate = input_sample_rate;
            options.input_channels = input_channels;
            options.input_format = input_format;
            return new AudioDecoder(options);
        }),
        py::arg("output_sample_rate") = py::none(),
        py::arg("input_sample_rate") = py::none(),
        py::arg("input_channels") = py::none(),
        py::arg("input_format") = py::none())
        
        .def("load", [](AudioDecoder& self, py::object source) -> const Metadata& {
            open_decoder_source(self, source);
            return self.get_metadata();
        }, 
        py::arg("source"), 
        py::return_value_policy::reference_internal,
        R"pbdoc(
            Load audio from file path, URL, bytes-like input, or device.
            
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
                >>> meta = decoder.load("song.mp3")
                >>> print(f"Duration: {meta.duration}s, Sample rate: {meta.sample_rate}Hz")
        )pbdoc")
        
        .def("open", [](AudioDecoder& self, py::object source) {
            open_decoder_source(self, source);
        }, 
        py::arg("source"),
        R"pbdoc(
            Open audio from file path, URL, bytes, or device.
            
            Args:
                source: Audio source, one of:
                    - str: File path or URL
                    - bytes/bytearray/memoryview: Full audio file bytes in memory
                    - io.BytesIO: In-memory encoded audio bytes
                    - pathlib.Path: File path object
            
            Raises:
                RuntimeError: If source cannot be opened.
                TypeError: If source type is not supported.
        )pbdoc")
        
        .def("__call__", [](AudioDecoder& self, py::object data) -> py::array_t<float> {
            push_decoder_data(self, data);

            // Use new C++ get_samples() to get all currently available samples
            std::vector<std::vector<float>> total_samples = self.get_samples();

            if (total_samples.empty()) {
                return py::array_t<float>(std::vector<size_t>{0, 0});
            }

            size_t num_channels = total_samples.size();
            size_t num_samples = total_samples[0].size();
            py::array_t<float> result({num_channels, num_samples});
            auto buf = result.mutable_unchecked<2>();
            for (size_t c = 0; c < num_channels; ++c) {
                std::copy(total_samples[c].begin(), total_samples[c].end(), &buf(c, 0));
            }
            return result;
        },
        py::arg("data"),
        R"pbdoc(
            Push raw bytes and decode immediately (streaming mode).

            This method enables push-based streaming: feed raw encoded bytes
            and receive decoded audio samples.

            Args:
                data (bytes/bytearray/memoryview or io.BytesIO): Raw encoded
                    audio bytes. Format must match the input_format specified
                    in constructor.

            Returns:
                numpy.ndarray: Decoded audio samples with shape (channels, samples).
                    dtype is float32, values in range [-1.0, 1.0].
                    Returns empty array if no complete frames decoded yet.

            Raises:
                RuntimeError: If input_format was not specified in constructor.

            Example:
                >>> decoder = AudioDecoder(input_format="s16le", input_sample_rate=48000, input_channels=2)
                >>> while True:
                ...     raw_data = network_stream.read(4096)
                ...     samples = decoder(raw_data)
                ...     if samples.size > 0:
                ...         process_audio(samples)
        )pbdoc")

        .def("get_samples", [](AudioDecoder& self) -> py::array_t<float> {
            // Directly use new C++ get_samples() implementation
            std::vector<std::vector<float>> total_samples = self.get_samples();

            if (total_samples.empty()) {
                return py::array_t<float>(std::vector<size_t>{0, 0});
            }

            size_t num_channels = total_samples.size();
            size_t num_samples = total_samples[0].size();
            py::array_t<float> result({num_channels, num_samples});
            auto buf = result.mutable_unchecked<2>();
            for (size_t c = 0; c < num_channels; ++c) {
                std::copy(total_samples[c].begin(), total_samples[c].end(), &buf(c, 0));
            }
            return result;
        },
        R"pbdoc(
            Decode all currently available samples.

            In File Mode: Decodes from current position to end of stream.
            In Stream Mode: Decodes all buffered data until more input is required.

            Returns:
                numpy.ndarray: Audio samples with shape (channels, samples).
                    dtype is float32, values in range [-1.0, 1.0].

            Note:
                For large files, consider using frame-by-frame decoding (read())
                to manage memory usage.

            Example:
                >>> decoder = AudioDecoder(output_sample_rate=16000)
                >>> decoder.load("speech.wav")
                >>> samples = decoder.get_samples()
        )pbdoc")

        .def("push", [](AudioDecoder& self, py::object data) {
            push_decoder_data(self, data);
        },
        py::arg("data"),
        R"pbdoc(
            Push raw audio data bytes to the decoder (stream mode only).

            Args:
                data (bytes): Raw audio data bytes

            Note:
                This method initializes the decoder on first call when enough data is buffered.
                Use read() or __call__() to retrieve decoded frames.

            Example:
                >>> decoder = AudioDecoder(input_format="s16le", input_sample_rate=16000, input_channels=1)
                >>> with open("audio.raw", "rb") as f:
                ...     chunk = f.read(3200)  # 100ms @ 16kHz mono
                ...     decoder.push(chunk)
                ...     samples = decoder.read()
        )pbdoc")

        .def("read", [](AudioDecoder& self) -> py::object {
            FrameData frame = self.read();
            if (!frame) {
                return py::none();
            }

            size_t num_channels = frame.num_channels;
            size_t num_samples = frame.num_samples;
            py::array_t<float> result({num_channels, num_samples});
            auto buf = result.mutable_unchecked<2>();
            for (size_t c = 0; c < num_channels; ++c) {
                std::copy(frame.data[c], frame.data[c] + num_samples, &buf(c, 0));
            }
            return result;
        },
        R"pbdoc(
            Decode next audio frame.

            Returns:
                numpy.ndarray or None: Decoded audio samples with shape (channels, samples),
                    or None if no frame is available (need more data or EOF).

            Example:
                >>> while not decoder.is_finished():
                ...     frame = decoder.read()
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
                For stream mode, metadata is only available after first push().
        )pbdoc");

    // --- DeviceManager ---
    
    py::class_<DeviceManager>(m, "DeviceManager", 
        "Utility class for discovering system audio devices.")
        .def_static("list_audio_devices", &DeviceManager::list_audio_devices,
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
