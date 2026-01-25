# AvioFlow

AvioFlow is a high-performance audio decoding and capture library powered by FFmpeg. It provides a modern C++20 API and seamless Python bindings for efficient audio processing, including file decoding, URL streaming, and low-latency system audio capture (WASAPI/DirectShow).

## Features

- **Multi-format Support**: Decodes any format supported by FFmpeg (MP3, WAV, AAC, FLAC, etc.).
- **Flexible Input**: Load audio from files, memory buffers, network URLs, or custom streams.
- **Hardware Capture**: 
    - **WASAPI Loopback**: Capture system output (what you hear).
    - **DirectShow**: Capture from microphones and other input devices.
- **Resampling**: Built-in support for target sample rate and channel conversion.
- **Python Bindings**: High-performance Python module using `pybind11`.

---

## 🛠 Build Instructions

### Prerequisites
- **Windows**: Visual Studio 2022+, CMake 3.20+.
- **Linux**: GCC 11+, CMake 3.20+, FFmpeg development headers.

### Build and Run
Use the provided PowerShell script for automated setup on Windows:
```powershell
.\build.ps1
```

Or manually with CMake:
```bash
cmake -B build -DENABLE_PYTHON=ON -DENABLE_WASAPI=ON
cmake --build build --config Release
```

---

## 🚀 C++ Usage

### Offline File Decoding
```cpp
#include "avioflow-cxx-api.h"
#include <iostream>

int main() {
    avioflow::AudioStreamOptions options;
    options.output_sample_rate = 16000; // Resample to 16kHz
    options.output_num_channels = 1;    // Convert to Mono

    avioflow::AudioDecoder decoder(options);
    decoder.open("audio.mp3");

    auto meta = decoder.get_metadata();
    std::cout << "Codec: " << meta.codec << " Duration: " << meta.duration << "s" << std::endl;

    // Decode all at once
    auto samples = decoder.get_all_samples();
    std::cout << "Decoded " << samples.data[0].size() << " samples." << std::endl;

    return 0;
}
```

### System Audio Capture (WASAPI)
```cpp
decoder.open("wasapi_loopback");
while (!decoder.is_finished()) {
    auto frame = decoder.decode_next();
    if (!frame.data.empty()) {
        // Process real-time float samples...
    }
}
```

---

## 🐍 Python Usage

install command:
```
pip install avioflow
```

### Basic Decoding
```python
import avioflow 

# Initialize and open
decoder = avioflow.AudioDecoder()
decoder.open("music.wav")

# Get info
meta = decoder.get_metadata()
print(f"Container:    {meta.container}")
print(f"Codec:        {meta.codec}")
print(f"Sample Rate:  {meta.sample_rate} Hz")
print(f"Channels:     {meta.num_channels}")
print(f"Duration:     {meta.duration:.3f} s")
print(f"Num Samples:  {meta.num_samples}")

# Decode all samples, with multi-channels
print(f"\nDecoding all samples...")
samples = decoder.get_all_samples()
```

### Real-time Capture
```python
# List available devices
devices = avioflow.DeviceManager.list_audio_devices()
for d in devices:
    print(f"ID: {d.name}, Desc: {d.description}")

decoder = avioflow.AudioDecoder()
decoder.open("audio=@device_cm_...") # Open microphone via DirectShow string

while True:
    frame = decoder.decode_next()
    if frame:
        # data is planar float32
        process(frame.data)
```

## License
MIT License
