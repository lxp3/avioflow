# Avioflow Audio Previewer for VS Code

<img src="https://raw.githubusercontent.com/lxp3/avioflow/main/vscode-extension/resources/preview.gif">

High-performance audio previewer for VS Code using the `avioflow` native engine. This extension provides a custom editor for various audio formats, allowing users to visualize waveforms and inspect metadata directly within VS Code.

## UI & Controls

- **Waveform View**: Visualizes multiple audio channels (CH 0, CH 1, etc.).
- **Metadata Panel**: Displays File Name, Size, Format, Codec, Duration, Sample Rate, and Bitrate.
- **Decoding Info**: Shows total load time and actual decoding time.

### Shortcuts
- **Space**: Play / Pause audio.
- **Mouse Wheel / Trackpad Scroll**: Zoom in/out of the waveform.
- **Click & Drag**: Navigate through the zoomed waveform.
- **Reset Button**: Restore original zoom level.

## Architecture

This extension uses a high-performance **WebAssembly (WASM)** core built from C++ and FFmpeg. This ensures fast decoding while maintaining cross-platform compatibility without native Node.js binary headaches.

### VSIX Structure
```text
avioflow-0.2.5.vsix
|- extension/
|  |- package.json
|  |- README.md
|  `- out/
|     |- src/ (Extension logic)
|     |- webview/ (Svelte UI)
|     `- wasm/
|        |- avioflow.js
|        `- avioflow.wasm
```

## Installation
```bash
code --install-extension avioflow-0.2.5.vsix
```

## Getting Started

### Prerequisites
- [Node.js](https://nodejs.org/) (installed in your system PATH)
- [pnpm](https://pnpm.io/)

### Installation & Build
```powershell
# Install dependencies
pnpm install

# Build the extension and webview
pnpm compile
```

### Running the Extension
1. Open the project in VS Code.
2. Press `F5` to open a new [Extension Development Host] window.
3. Open any audio file (e.g., `.mp3`, `.wav`, `.flac`) to see the preview.

## Features
- **High Performance**: Leverages native C++ and FFmpeg for fast decoding.
- **WASM Core**: Runs at near-native speed with zero installation overhead.
- **Modern UI**: Clean waveform visualization built with Svelte.
