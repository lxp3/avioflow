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
    m.doc() = "avioflow: High-performance audio decoding library powered by FFmpeg";

    // --- Global Functions ---
    m.def("set_log_level", [](const std::string& level) {
        avioflow_set_log_level(level.c_str());
    }, py::arg("level") = "info", 
       "Set FFmpeg log level. Options: quiet, fatal, error, warning, info, debug, trace");

    // --- Structs ---
    py::class_<AudioStreamOptions>(m, "AudioStreamOptions", "Configuration options for audio decoding and resampling")
        .def(py::init<>())
        .def_readwrite("output_sample_rate", &AudioStreamOptions::output_sample_rate, "(int or None): Target output sample rate (Hz). If null, keeps original.")
        .def_readwrite("output_num_channels", &AudioStreamOptions::output_num_channels, "(int or None): Target output channel count. If null, keeps original.")
        .def_readwrite("input_sample_rate", &AudioStreamOptions::input_sample_rate, "(int or None): Force input sample rate (only for raw PCM).")
        .def_readwrite("input_channels", &AudioStreamOptions::input_channels, "(int or None): Force input channel count (only for raw PCM).")
        .def_readwrite("input_format", &AudioStreamOptions::input_format, "(str or None): Force input format hint (e.g., 'wav', 'mp3', 's16le').")
        .def("__repr__", [](const AudioStreamOptions& self) {
            std::stringstream ss;
            ss << "<avioflow.AudioStreamOptions"
               << " output_sample_rate=" << (self.output_sample_rate ? std::to_string(*self.output_sample_rate) : "None")
               << " output_num_channels=" << (self.output_num_channels ? std::to_string(*self.output_num_channels) : "None")
               << " input_sample_rate=" << (self.input_sample_rate ? std::to_string(*self.input_sample_rate) : "None")
               << " input_channels=" << (self.input_channels ? std::to_string(*self.input_channels) : "None")
               << " input_format=" << (self.input_format ? *self.input_format : "None")
               << ">";
            return ss.str();
        });

    py::class_<DeviceInfo>(m, "DeviceInfo", "Information about a system audio device")
        .def(py::init<>())
        .def_readonly("name", &DeviceInfo::name, "(str): DirectShow/WASAPI device name identifier")
        .def_readonly("description", &DeviceInfo::description, "(str): Human-readable device description")
        .def_readonly("is_output", &DeviceInfo::is_output, "(bool): True if this is an output/loopback device")
        .def("__repr__", [](const DeviceInfo& self) {
            std::stringstream ss;
            ss << "<avioflow.DeviceInfo"
               << " name='" << self.name << "'"
               << " description='" << self.description << "'"
               << " is_output=" << (self.is_output ? "True" : "False")
               << ">";
            return ss.str();
        });

    py::class_<Metadata>(m, "Metadata", "Audio stream information")
        .def(py::init<>())
        .def_readonly("duration", &Metadata::duration, "(float): Duration in seconds")
        .def_readonly("num_samples", &Metadata::num_samples, "(int): Total number of samples (if known)")
        .def_readonly("sample_rate", &Metadata::sample_rate, "(int): Original sampling rate (Hz)")
        .def_readonly("num_channels", &Metadata::num_channels, "(int): Original number of channels")
        .def_readonly("sample_format", &Metadata::sample_format, "(str): Original sample format (e.g., 'fltp', 's16')")
        .def_readonly("codec", &Metadata::codec, "(str): Codec name (e.g., 'mp3', 'aac')")
        .def_readonly("bit_rate", &Metadata::bit_rate, "(int): Bit rate in bits per second")
        .def_readonly("container", &Metadata::container, "(str): Format container name")
        .def("__repr__", [](const Metadata& self) {
            std::stringstream ss;
            ss << "<avioflow.Metadata"
               << " duration=" << std::fixed << std::setprecision(2) << self.duration
               << " sample_rate=" << self.sample_rate
               << " num_channels=" << self.num_channels
               << " codec='" << self.codec << "'"
               << " bit_rate=" << self.bit_rate
               << " container='" << self.container << "'"
               << ">";
            return ss.str();
        });

    py::class_<AudioSamples>(m, "AudioSamples", "Buffer containing decoded audio data")
        .def(py::init<>())
        .def_readonly("data", &AudioSamples::data, "(list[list[float]]): Planar float data: (channels, samples)")
        .def_readonly("sample_rate", &AudioSamples::sample_rate, "(int): The sample rate of this data")
        .def("__repr__", [](const AudioSamples& self) {
            std::stringstream ss;
            ss << "<avioflow.AudioSamples"
               << " channels=" << self.data.size()
               << " samples_per_channel=" << (self.data.empty() ? 0 : self.data[0].size())
               << " sample_rate=" << self.sample_rate
               << ">";
            return ss.str();
        });

    // --- Main Decoder Class ---
    py::class_<AudioDecoder>(m, "AudioDecoder", "Main class for audio decoding and device capture")
        .def(py::init<const AudioStreamOptions&>(), py::arg("options") = AudioStreamOptions(), "Initialize decoder with optional resampling settings")
        .def("open", &AudioDecoder::open, py::arg("source"), 
             "Open an audio source (file path, URL, or wasapi_loopback/audio=...)")
        .def("open_memory", [](AudioDecoder& self, py::bytes data) {
            std::string s = data;
            self.open_memory(reinterpret_cast<const uint8_t*>(s.data()), s.size());
        }, py::arg("data"), "Open audio from a memory buffer")
        .def("open_stream", &AudioDecoder::open_stream, py::arg("callback"), py::arg("options") = AudioStreamOptions(), 
             "Open audio from a custom stream-like object with a read callback")
        .def("decode_next", [](AudioDecoder& self) -> py::object {
            auto samples = self.decode_next();
            if (samples.data.empty()) return py::none();
            return py::cast(samples);
        }, "Decode next available frame. Returns AudioSamples or None if end of stream reached.")
        .def("get_all_samples", &AudioDecoder::get_all_samples, "Synchronously decode the entire source and return all samples.")
        .def("is_finished", &AudioDecoder::is_finished, "Check if the stream has reached the end")
        .def("get_metadata", &AudioDecoder::get_metadata, py::return_value_policy::reference_internal, "Get detected audio metadata");

    // --- Device Manager ---
    py::class_<DeviceManager>(m, "DeviceManager", "Static utility for audio device management")
        .def_static("list_audio_devices", &DeviceManager::list_audio_devices, "Enumerate all available audio input and loopback devices");
}
