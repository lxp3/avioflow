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

The Python package is built into `build/bin/Release/avioflow`. Add this path to your `PYTHONPATH` or copy the folder to your project.

### Basic Decoding
```python
import avioflow as av

# Set log level for debugging
av.set_log_level("warning")

# Initialize and open
decoder = av.AudioDecoder()
decoder.open("music.wav")

# Get info
meta = decoder.get_metadata()
print(f"Format: {meta.container}, {meta.sample_rate}Hz")

# Get all samples as nested lists [[ch1...], [ch2...]]
samples = decoder.get_all_samples()
print(f"Total samples: {len(samples.data[0])}")
```

### Real-time Capture
```python
# List available devices
devices = av.DeviceManager.list_audio_devices()
for d in devices:
    print(f"ID: {d.name}, Desc: {d.description}")

decoder = av.AudioDecoder()
decoder.open("audio=@device_cm_...") # Open microphone via DirectShow string

while True:
    frame = decoder.decode_next()
    if frame:
        # data is planar float32
        process(frame.data)
```

### Installation for Developers
To use the local build in your script:
```python
import sys
sys.path.append("path/to/avioflow/build/bin/Release")
import avioflow
```

---

## License
MIT License
