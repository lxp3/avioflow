# Avioflow Audio Previewer for VS Code

High-performance audio previewer for VS Code using the `avioflow` native engine. This extension provides a custom editor for various audio formats, allowing users to visualize waveforms and inspect metadata directly within VS Code.

## Architecture

To ensure stability and performance, this extension uses a decoupled architecture where the heavy audio decoding is offloaded to a persistent external Node.js process. This bypasses Electron ABI compatibility issues and prevents the extension host from crashing.

```mermaid
avioflow-0.1.0.vsix
├─ [Content_Types].xml
├─ extension.vsixmanifest
└─ extension/
   ├─ LICENSE.txt [0.01 KB]
   ├─ package.json [2.07 KB]
   ├─ readme.md [2.17 KB]
   └─ out/
      ├─ node_modules/
      │  └─ avioflow/
      │     ├─ README.md [9.29 KB]
      │     ├─ package.json [1.37 KB]
      │     ├─ avioflow/ (2 files) [8.5 KB]
      │     ├─ node_modules/ (9 files) [11.61 KB]
      │     └─ prebuilds/ (2 files) [23.95 MB]
      ├─ src/
      │  ├─ AudioPreviewProvider.js [4.42 KB]
      │  ├─ AudioPreviewProvider.js.map [2.36 KB]
      │  ├─ AvioflowWorkerService.js [6.01 KB]
      │  ├─ AvioflowWorkerService.js.map [4.02 KB]
      │  ├─ HomeViewProvider.js [4.3 KB]
      │  ├─ HomeViewProvider.js.map [1.04 KB]
      │  ├─ avioflowWorker.js [1.83 KB]
      │  ├─ avioflowWorker.js.map [1.28 KB]
      │  ├─ extension.js [2.55 KB]
      │  └─ extension.js.map [0.99 KB]
      └─ webview/
         ├─ main.js [16.13 KB]
         ├─ style.css [5.06 KB]
         ├─ vite.config.mjs [1.02 KB]
         ├─ vite.config.mjs.map [0.89 KB]
         └─ src/
            ├─ main.js [0.39 KB]
            └─ main.js.map [0.23 KB]
```

### Key Components
- **`AvioflowWorkerService`**: A singleton in the main extension process that manages the lifecycle of the persistent background worker.
- **`avioflowWorker`**: A standalone Node.js process that performs native audio decoding using the `avioflow` library.
- **`AudioPreviewProvider`**: Implements the `CustomReadonlyEditorProvider` to bridge between VS Code and the background worker.

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
- **Isolated Process**: Decoding runs in a separate process, keeping VS Code responsive.
- **Persistent Worker**: Minimal overhead for opening subsequent files.
- **Modern UI**: Clean waveform visualization built with Svelte.
