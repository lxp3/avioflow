# AvioFlow

AvioFlow is a high-performance audio decoding and capture library powered by FFmpeg. It provides a modern C++20 API and seamless Python bindings for efficient audio processing, including file decoding, URL streaming, and low-latency system audio capture (WASAPI/DirectShow).

## Features

- **Multi-format Support**: Decodes any format supported by FFmpeg (MP3, WAV, AAC, FLAC, etc.).
- **Flexible Input**: Load audio from files, memory buffers, network URLs, or custom streams.
- **Hardware Capture**: 
    - **WASAPI Loopback**: Capture system output (what you hear).
    - **DirectShow**: Capture from microphones and other input devices.
- **Resampling**: Built-in support for target sample rate and channel conversion.
- **Node-API Support**: Modern ESM-ready Node.js bindings for real-time audio processing.
- **Python Bindings**: High-performance Python module using `pybind11`.
- **Static Linking**: Fully static build support (FFmpeg + CRT) for easy distribution without external DLL dependencies.

---

## 🛠 Build Instructions

### Prerequisites
- **Windows**: Visual Studio 2022+, CMake 3.20+.
- **Linux**: GCC 11+, CMake 3.20+, FFmpeg development headers.

### Windows
Use the provided PowerShell script for a full build (includes Node.js and Python):
```powershell
.\build.ps1
```

Or for a specific Node.js build:
```powershell
npx cmake-js compile
```

### Linux
Compile with shared or static FFmpeg (default is shared):
```bash
cmake -B build -DENABLE_PYTHON=ON -DENABLE_WASAPI=ON
cmake --build build --config Release
```

### 📦 Prebuildify (Node.js)
Generate prebuilt binaries for all Node.js versions:
```bash
npm run prebuild
```
This will strip symbols and tag them for compatibility (Node >= 16).

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

---

## 📦 Node.js Usage

### Installation
Directly from npm:
```bash
npm install avioflow
```

### ESM Example
```javascript
import avioflow from 'avioflow';

const decoder = new avioflow.AudioDecoder();
decoder.open("TownTheme.mp3");

const meta = decoder.getMetadata();
console.log(`Codec: ${meta.codec}, Duration: ${meta.duration}s`);

while (!decoder.isFinished()) {
    const frame = decoder.decodeNext();
    if (frame) {
        // frame.data is an array of Float32Arrays (one per channel)
        console.log(`Decoded ${frame.channels} channels`);
    }
}
```

### Device Discovery
```javascript
const devices = avioflow.listAudioDevices();
devices.forEach(dev => {
    console.log(`${dev.isOutput ? 'Output' : 'Input'}: ${dev.name}`);
});
```

---

## ⚙️ Static Compilation Details

AvioFlow supports fully static builds on Windows to eliminate external dependencies:
- **FFmpeg**: Statically linked into the binary.
- **CRT**: Using `/MT` to statically link the C Runtime Library, avoiding the "VC++ Redistributable" requirement.

To enable this in your own CMake project:
```cmake
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# Link to avioflow (which should be built with BUILD_SHARED_LIBS=OFF)
```

## License
MIT License
