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
└── package.json          # NPM package config
```

## Build Requirements

### 1. Install Emscripten SDK

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

### 2. Build avioflow WASM Module

From the repository root, with the Emscripten environment activated:

```bash
emcmake cmake -B build-wasm -S . -DENABLE_WASM=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm --config Release
```

A prebuilt FFmpeg 7.1 WASM package (audio decoders only) is downloaded
automatically during the CMake configure step, so FFmpeg does not need to be
compiled from source.

Output files:
- `dist/avioflow.js` - JavaScript loader + glue code (~120KB)
- `dist/avioflow.wasm` - WebAssembly binary (~3MB)

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
decoder.feed(chunk1);
decoder.feed(chunk2);

// Decode available frames
let frame;
while ((frame = decoder.getFrame()) !== null) {
    // frame is Float32Array[] (one per channel)
    processAudio(frame);
}

// No more input: drain buffered and codec-delayed frames
decoder.flush();
while ((frame = decoder.getFrame()) !== null) {
    processAudio(frame);
}
```

### Time-Range Decode (Offline Seek)

```javascript
const decoder = new avioflow.AudioDecoder();
decoder.loadBuffer(new Uint8Array(buffer));

// Decode only seconds 10.3 to 20.3
const samples = decoder.getSamples(10.3, 20.3);

// Pass -1 as stopSeconds to decode from an offset to the end
const tail = decoder.getSamples(30.0, -1);
```

## API Reference

### Module Functions

```typescript
// Initialize WASM module
createAvioflow(): Promise<AvioflowModule>

// Set FFmpeg log level
setLogLevel(level: string): void
// levels: "quiet", "fatal", "error", "warning", "info", "debug", "trace"

// Load and decode a path in the WASM filesystem
load(path: string, options?: DecodeOptions): LoadResult

// Load and decode from buffer
loadBuffer(buffer: ArrayBuffer | Uint8Array, options?: DecodeOptions): LoadResult

// Encode samples to a path in the WASM filesystem
save(path: string, channels: Float32Array[], options?: WriteOptions): void

// Resample a complete buffer; pass -1 for outputNumChannels to keep the input count
resample(channels: Float32Array[], inputSampleRate: number,
         outputSampleRate: number, outputNumChannels: number): Float32Array[]

// Query FFmpeg capabilities
getSupportedDecoders(): string[];
getSupportedEncoders(): string[];
getSupportedInputFormats(): string[];
getSupportedOutputFormats(): string[];
```

> `load()` and `AudioDecoder.loadFile()` read through Emscripten's virtual
> filesystem, not the network. To decode a remote file in the browser, `fetch()`
> it yourself and pass the bytes to `loadBuffer()`.

### AudioDecoder Class

```typescript
class AudioDecoder {
    constructor(options?: StreamOptions);

    // Open a path in the WASM filesystem; returns metadata
    loadFile(path: string): Metadata;

    // Open from buffer (complete file); returns metadata
    loadBuffer(buffer: ArrayBuffer | Uint8Array): Metadata;

    // Push data for streaming decode
    feed(data: ArrayBuffer | Uint8Array): void;

    // Mark stream input complete, so buffered and codec-delayed frames can drain
    flush(): void;

    // Decode next available frame
    getFrame(): Float32Array[] | null;

    // Decode samples in [startSeconds, stopSeconds). Pass -1 for stopSeconds to decode to the end.
    getSamples(startSeconds?: number, stopSeconds?: number): Float32Array[];

    // Get audio metadata
    getMetadata(): Metadata;

    // Check if decoding finished
    isFinished(): boolean;
}
```

### AudioResampler Class

```typescript
class AudioResampler {
    constructor(options: {
        inputSampleRate: number;      // required, > 0
        outputSampleRate: number;     // required, > 0
        outputNumChannels?: number;   // defaults to the input count
    });

    process(channels: Float32Array[]): Float32Array[];
    flush(): Float32Array[];
    outputSampleRate(): number;
    outputNumChannels(): number;     // 0 until the first process() call
    delete(): void;                  // release the WASM object
}
```

For audio arriving in chunks, use this rather than calling `resample()` per
chunk: filter state carries across `process()` calls, so the output matches a
one-shot conversion sample for sample, whereas per-chunk `resample()` would
introduce a discontinuity at every boundary.

```javascript
const resampler = new avioflow.AudioResampler({
    inputSampleRate: 44100,
    outputSampleRate: 16000,
});

const parts = [];
for (const chunk of chunks) parts.push(resampler.process(chunk));
parts.push(resampler.flush());   // else the tail is lost
resampler.delete();              // embind objects are not garbage collected
```

`flush()` is not optional: the resampler holds back the last few milliseconds to
keep filter continuity, and skipping it discards them.

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

interface WriteOptions {
    codecName?: string;        // e.g. "pcm_s16le", "libmp3lame"
    containerFormat?: string;  // e.g. "wav", "mp3"
    sampleRate?: number;
    numChannels?: number;
    bitRate?: number;
    sampleFormat?: string;
    overwrite?: boolean;
}
```

## Why WASM?

| Feature | Native Addon (node-addon-api) | WASM |
|---------|------------------------------|------|
| ABI Stability | ❌ Requires rebuild per Electron version | ✅ Universal |
| Browser Support | ❌ No | ✅ Yes |
| Performance | ✅ Native speed | ⚠️ Slower than native |
| File Size | Larger | ~3MB |
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
