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
            >>> samples = decoder.get_all_samples()  # shape: (channels, samples)
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

    // Quick offline loading helper
    m.def("load", [](const std::string& path,
                     std::optional<int> output_sample_rate,
                     std::optional<int> output_num_channels) {
        AudioStreamOptions opts;
        opts.output_sample_rate = output_sample_rate;
        opts.output_num_channels = output_num_channels;
        
        AudioDecoder decoder(opts);
        decoder.open(path);
        
        const auto& meta = decoder.get_metadata();
        auto samples = decoder.get_all_samples();
        
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
    py::arg("path"),
    py::arg("output_sample_rate") = py::none(),
    py::arg("output_num_channels") = py::none(),
    R"pbdoc(
        Load an audio file and decode all samples in one call.
        
        This is a convenience function that combines file loading and full
        decoding in a single call. For large files or when you need frame-by-frame
        control, use AudioDecoder directly.
        
        Args:
            path (str): Path to audio file or URL.
            output_sample_rate (int, optional): Target sample rate in Hz.
                If None, uses source sample rate.
            output_num_channels (int, optional): Target number of channels.
                If None, uses source channel count.
        
        Returns:
            tuple[Metadata, numpy.ndarray]: A tuple containing:
                - Metadata: Audio stream information
                - samples: Float32 array with shape (channels, samples)
        
        Example:
            >>> import avioflow
            >>> meta, samples = avioflow.load("audio.mp3")
            >>> print(f"Duration: {meta.duration}s, Shape: {samples.shape}")
            
            >>> # With resampling to 16kHz mono
            >>> meta, samples = avioflow.load("speech.wav", output_sample_rate=16000, output_num_channels=1)
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
            ss << "<avioflow.Metadata "
               << "codec='" << self.codec << "' "
               << "sample_rate=" << self.sample_rate << " "
               << "channels=" << self.num_channels << " "
               << "duration=" << std::fixed << std::setprecision(2) << self.duration << "s>";
            return ss.str();
        });

    // --- AudioDecoder ---
    
    py::class_<AudioDecoder>(m, "AudioDecoder", R"pbdoc(
        High-performance audio decoder with file and streaming support.
        
        Two operation modes:
        
        **File Mode** - Load complete audio files:
            >>> decoder = AudioDecoder(output_sample_rate=44100)
            >>> meta = decoder.load("audio.mp3")
            >>> samples = decoder.get_all_samples()  # numpy array (channels, samples)
        
        **Stream Mode** - Decode real-time byte streams:
            >>> decoder = AudioDecoder(input_format="s16le", input_sample_rate=48000, input_channels=2)
            >>> samples = decoder(raw_bytes)  # Returns decoded numpy array
        
        Args:
            output_sample_rate (int, optional): Target output sample rate in Hz.
                If not specified, uses source sample rate.
            output_num_channels (int, optional): Target number of output channels.
                If not specified, uses source channel count.
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
        .def(py::init([](py::kwargs kwargs) {
            AudioStreamOptions options;
            if (kwargs.contains("output_sample_rate")) 
                options.output_sample_rate = py::cast<int>(kwargs["output_sample_rate"]);
            if (kwargs.contains("output_num_channels")) 
                options.output_num_channels = py::cast<int>(kwargs["output_num_channels"]);
            if (kwargs.contains("input_sample_rate")) 
                options.input_sample_rate = py::cast<int>(kwargs["input_sample_rate"]);
            if (kwargs.contains("input_channels")) 
                options.input_channels = py::cast<int>(kwargs["input_channels"]);
            if (kwargs.contains("input_format")) 
                options.input_format = py::cast<std::string>(kwargs["input_format"]);
            return new AudioDecoder(options);
        }))
        
        .def("load", [](AudioDecoder& self, py::object source) -> const Metadata& {
            if (py::isinstance<py::str>(source)) {
                self.open(py::cast<std::string>(source));
            } else if (py::hasattr(source, "__fspath__")) {
                self.open(py::cast<std::string>(source.attr("__fspath__")()));
            } else {
                throw py::type_error("source must be str, bytes, or PathLike object");
            }
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
        
        .def("__call__", [](AudioDecoder& self, py::bytes data) -> py::array_t<float> {
            std::string s = data;
            if (s.size() > 0) {
                self.push(reinterpret_cast<const uint8_t*>(s.data()), s.size());
            }
            
            std::vector<std::vector<float>> total_samples;
            int max_frames_per_call = 100; // Prevent infinite loop
            int frames_decoded = 0;

            while (frames_decoded < max_frames_per_call) {
                FrameData frame = self.decode_next();
                if (!frame) break;
                
                frames_decoded++;
                if (total_samples.empty()) {
                    total_samples.resize(frame.num_channels);
                }
                for (int c = 0; c < frame.num_channels; ++c) {
                    const float* src = frame.data[c];
                    total_samples[c].insert(total_samples[c].end(), src, src + frame.num_samples);
                }
            }
            
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
                data (bytes): Raw encoded audio bytes. Format must match
                    the input_format specified in constructor.
            
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
        
        .def("get_all_samples", [](AudioDecoder& self) -> py::array_t<float> {
            std::vector<std::vector<float>> total_samples;
            while (!self.is_finished()) {
                FrameData frame = self.decode_next();
                if (!frame) break;
                
                if (total_samples.empty()) {
                    total_samples.resize(frame.num_channels);
                }
                for (int c = 0; c < frame.num_channels; ++c) {
                    const float* src = frame.data[c];
                    total_samples[c].insert(total_samples[c].end(), src, src + frame.num_samples);
                }
            }
            
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
            Decode entire audio source and return all samples.
            
            This is a convenience method for offline/batch processing.
            Decodes from current position to end of stream.
            
            Returns:
                numpy.ndarray: All audio samples with shape (channels, samples).
                    dtype is float32, values in range [-1.0, 1.0].
            
            Note:
                For large files, consider using frame-by-frame decoding
                to manage memory usage.
            
            Example:
                >>> decoder = AudioDecoder(output_sample_rate=16000)
                >>> decoder.load("speech.wav")
                >>> samples = decoder.get_all_samples()
                >>> print(f"Shape: {samples.shape}")  # e.g., (1, 160000) for 10s mono
        )pbdoc")
        
        .def("push", [](AudioDecoder& self, py::bytes data, size_t size) {
            std::string s = data;
            if (s.size() < size) {
                throw std::invalid_argument("Data size smaller than specified size");
            }
            self.push(reinterpret_cast<const uint8_t*>(s.data()), size);
        },
        py::arg("data"),
        py::arg("size"),
        R"pbdoc(
            Push raw audio data bytes to the decoder (stream mode only).
            
            Args:
                data (bytes): Raw audio data bytes
                size (int): Number of bytes to push
            
            Note:
                This method initializes the decoder on first call.
                Use decode_next() or __call__() to retrieve decoded frames.
            
            Example:
                >>> decoder = AudioDecoder(input_format="s16le", input_sample_rate=16000, input_channels=1)
                >>> with open("audio.raw", "rb") as f:
                ...     chunk = f.read(3200)  # 100ms @ 16kHz mono
                ...     decoder.push(chunk, len(chunk))
                ...     samples = decoder.decode_next()
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
