# avioflow-wasm

High-performance audio decoding library for browsers and Electron (WebAssembly build).

## Features

- Full audio format support: MP3, WAV, AAC, FLAC, OGG, Opus, etc.
- Streaming decode support for real-time applications
- Works in browsers and Electron without native addon issues
- ABI stable - no recompilation needed for different Electron versions
- TypeScript definitions included

## Directory Structure

```
wasm/
├── csrc/
│   ├── bindings.cpp      # Emscripten embind bindings
│   └── CMakeLists.txt    # WASM build configuration
├── src/
│   ├── index.js          # JavaScript wrapper
│   └── index.d.ts        # TypeScript definitions
├── tests/
│   ├── test-browser.html # Browser test page
│   └── test-node.mjs     # Node.js test script
├── dist/                 # Build output (generated)
│   ├── avioflow.js
│   └── avioflow.wasm
├── build.ps1             # Build script
└── package.json          # NPM package config
```

## Build Requirements

### Install Emscripten SDK

```bash
# Clone emsdk
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk

# Install and activate latest version
./emsdk install latest
./emsdk activate latest

# Activate environment (run each session)
source ./emsdk_env.sh   # Linux/macOS
# or
emsdk_env.bat           # Windows
```

### Build WASM Module

```powershell
cd avioflow/wasm
./build.ps1
```

Output files:
- `dist/avioflow.js` - JavaScript loader
- `dist/avioflow.wasm` - WebAssembly binary

## Usage

### Browser

```html
<script type="module">
import createAvioflow from './dist/avioflow.js';

const avioflow = await createAvioflow();

// Decode from ArrayBuffer
const response = await fetch('audio.mp3');
const buffer = await response.arrayBuffer();
const { metadata, samples } = avioflow.loadBuffer(new Uint8Array(buffer));

console.log(`Duration: ${metadata.duration}s`);
console.log(`Channels: ${samples.length}`);
console.log(`Samples: ${samples[0].length}`);
</script>
```

### Node.js

```javascript
import { readFileSync } from 'fs';
import createAvioflow from 'avioflow-wasm';

const avioflow = await createAvioflow();

const audioData = readFileSync('audio.mp3');
const { metadata, samples } = avioflow.loadBuffer(new Uint8Array(audioData));
```

### Streaming Decode

```javascript
const avioflow = await createAvioflow();

// Create streaming decoder
const decoder = new avioflow.AudioDecoder({
    inputFormat: 'mp3',
    outputSampleRate: 16000,
    outputNumChannels: 1
});

// Push chunks as they arrive
decoder.push(chunk1);
decoder.push(chunk2);

// Decode available frames
let frame;
while ((frame = decoder.decodeNext()) !== null) {
    // frame is Float32Array[] (one per channel)
    processAudio(frame);
}
```

## API Reference

### Module Functions

```typescript
// Initialize WASM module
createAvioflow(): Promise<AvioflowModule>

// Set FFmpeg log level
setLogLevel(level: string): void
// levels: "quiet", "fatal", "error", "warning", "info", "debug", "trace"

// Load and decode from URL
load(url: string, options?: DecodeOptions): LoadResult

// Load and decode from buffer
loadBuffer(buffer: ArrayBuffer | Uint8Array, options?: DecodeOptions): LoadResult
```

### AudioDecoder Class

```typescript
class AudioDecoder {
    constructor(options?: StreamOptions);
    
    // Open from URL (uses fetch internally)
    open(url: string): void;
    
    // Open from buffer (complete file)
    openBuffer(buffer: ArrayBuffer | Uint8Array): void;
    
    // Push data for streaming decode
    push(data: Uint8Array): void;
    
    // Decode next available frame
    decodeNext(): Float32Array[] | null;
    
    // Decode all remaining samples
    getAllSamples(): Float32Array[];
    
    // Get audio metadata
    getMetadata(): Metadata;
    
    // Check if decoding finished
    isFinished(): boolean;
}
```

### Types

```typescript
interface Metadata {
    duration: number;      // seconds
    sampleRate: number;    // Hz
    numChannels: number;
    codec: string;
    numSamples: number;
    sampleFormat: string;
    bitRate: number;       // bps
    container: string;
}

interface DecodeOptions {
    outputSampleRate?: number;
    outputNumChannels?: number;
}

interface StreamOptions extends DecodeOptions {
    inputSampleRate?: number;
    inputChannels?: number;
    inputFormat?: string;  // "mp3", "wav", "aac", "s16le", etc.
}

interface LoadResult {
    metadata: Metadata;
    samples: Float32Array[];  // one array per channel
}
```

## Why WASM?

| Feature | Native Addon (node-addon-api) | WASM |
|---------|------------------------------|------|
| ABI Stability | ❌ Requires rebuild per Electron version | ✅ Universal |
| Browser Support | ❌ No | ✅ Yes |
| Performance | ✅ Native speed | ⚠️ ~80-90% native |
| File Size | ~20MB | ~5-10MB |
| Installation | Requires build tools | Just JavaScript |

## Testing

### Browser Test

Open `tests/test-browser.html` in a browser (requires local server for CORS):

```bash
cd wasm
npx serve .
# Open http://localhost:3000/tests/test-browser.html
```

### Node.js Test

```bash
node --experimental-wasm-modules tests/test-node.mjs
```

## License

MIT
