/**
 * Node.js test for avioflow WASM module
 * Run with: node --experimental-wasm-modules test-node.mjs
 */
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';
import { dirname, join } from 'path';

const __dirname = dirname(fileURLToPath(import.meta.url));

// Test audio files
const MP3_PATH = join(__dirname, '../../public/wavs/TownTheme.mp3');
const WAV_PATH = join(__dirname, '../../public/wavs/zh.wav');
const OPUS_PATH = join(__dirname, '../../public/wavs/Y0000000000_--5llN02F84.opus');

async function main() {
    console.log('=== avioflow WASM Node.js Test ===\n');
    
    // Import WASM module
    console.log('[1/5] Loading WASM module...');
    const start = performance.now();
    
    const createAvioflow = (await import('../dist/avioflow.js')).default;
    const avioflow = await createAvioflow();
    
    console.log(`  Module loaded in ${(performance.now() - start).toFixed(1)}ms`);
    console.log(`  Available: ${Object.keys(avioflow).join(', ')}\n`);
    
    // Set log level
    avioflow.setLogLevel('warning');
    
    // Test 1: Load MP3 from buffer
    console.log('[2/5] Testing MP3 decode from buffer...');
    try {
        const mp3Data = readFileSync(MP3_PATH);
        console.log(`  File size: ${mp3Data.length} bytes`);
        
        const decodeStart = performance.now();
        const result = avioflow.loadBuffer(new Uint8Array(mp3Data), {});
        const elapsed = (performance.now() - decodeStart).toFixed(1);
        
        console.log(`  Duration: ${result.metadata.duration.toFixed(2)}s`);
        console.log(`  Sample Rate: ${result.metadata.sampleRate}Hz`);
        console.log(`  Channels: ${result.metadata.numChannels}`);
        console.log(`  Codec: ${result.metadata.codec}`);
        console.log(`  Samples: ${result.samples[0]?.length || 0}`);
        console.log(`  Decode time: ${elapsed}ms`);
        console.log('  ✓ MP3 decode PASSED\n');
    } catch (e) {
        console.log(`  ✗ MP3 decode FAILED: ${e.message}\n`);
    }
    
    // Test 2: Load WAV with resampling
    console.log('[3/5] Testing WAV decode with resampling...');
    try {
        const wavData = readFileSync(WAV_PATH);
        console.log(`  File size: ${wavData.length} bytes`);
        
        const decodeStart = performance.now();
        const result = avioflow.loadBuffer(new Uint8Array(wavData), {
            outputSampleRate: 8000,
            outputNumChannels: 1
        });
        const elapsed = (performance.now() - decodeStart).toFixed(1);
        
        console.log(`  Original: ${result.metadata.sampleRate}Hz`);
        console.log(`  Resampled samples: ${result.samples[0]?.length || 0}`);
        console.log(`  Decode time: ${elapsed}ms`);
        console.log('  ✓ WAV resample PASSED\n');
    } catch (e) {
        console.log(`  ✗ WAV resample FAILED: ${e.message}\n`);
    }
    
    // Test 3: Streaming decode
    console.log('[4/5] Testing streaming decode...');
    try {
        const mp3Data = readFileSync(MP3_PATH);
        
        const decoder = new avioflow.AudioDecoder({
            inputFormat: 'mp3'
        });
        
        // Push in 100ms chunks
        const chunkSize = 4096;
        let offset = 0;
        let totalSamples = 0;
        let frameCount = 0;
        
        const streamStart = performance.now();
        
        while (offset < mp3Data.length) {
            const chunk = mp3Data.slice(offset, offset + chunkSize);
            decoder.feed(new Uint8Array(chunk));
            offset += chunkSize;
            
            let frame;
            while ((frame = decoder.getFrame()) !== null) {
                totalSamples += frame[0]?.length || 0;
                frameCount++;
            }
        }
        
        // Drain
        while (!decoder.isFinished()) {
            const frame = decoder.getFrame();
            if (!frame) break;
            totalSamples += frame[0]?.length || 0;
            frameCount++;
        }
        
        const elapsed = (performance.now() - streamStart).toFixed(1);
        
        console.log(`  Chunks pushed: ${Math.ceil(mp3Data.length / chunkSize)}`);
        console.log(`  Frames decoded: ${frameCount}`);
        console.log(`  Total samples: ${totalSamples}`);
        console.log(`  Time: ${elapsed}ms`);
        console.log('  ✓ Streaming decode PASSED\n');
    } catch (e) {
        console.log(`  ✗ Streaming decode FAILED: ${e.message}\n`);
    }
    
    // Test 5: getSamples time-range seeking across a long Ogg/Opus file
    console.log('[5/5] Testing getSamples(start, stop) seeking on a long Opus file...');
    try {
        const opusData = readFileSync(OPUS_PATH);
        const decoder = new avioflow.AudioDecoder();
        decoder.loadBuffer(new Uint8Array(opusData), {});

        const meta = decoder.getMetadata();
        console.log(`  Duration: ${meta.duration.toFixed(2)}s, Sample rate: ${meta.sampleRate}Hz`);

        // Ranges chosen to cross every page boundary, including well past 1300s
        // where a stale/incompatible WASM build previously returned empty samples.
        const ranges = [[0, 50], [700, 750], [1300, 1350], [1900, 1950], [2400, meta.duration]];
        let allPassed = true;

        for (const [startSeconds, stopSeconds] of ranges) {
            const samples = decoder.getSamples(startSeconds, stopSeconds);
            const numSamples = samples[0]?.length || 0;
            const expectedSamples = Math.round((stopSeconds - startSeconds) * meta.sampleRate);
            const withinTolerance = Math.abs(numSamples - expectedSamples) < meta.sampleRate * 0.1;

            console.log(`  [${startSeconds}s, ${stopSeconds.toFixed(1)}s) -> ${numSamples} samples` +
                (withinTolerance ? '' : ` (expected ~${expectedSamples})`));

            if (numSamples === 0 || !withinTolerance) {
                allPassed = false;
            }
        }

        console.log(allPassed ? '  ✓ Opus range seeking PASSED\n' : '  ✗ Opus range seeking FAILED\n');
    } catch (e) {
        console.log(`  ✗ Opus range seeking FAILED: ${e.message}\n`);
    }

    console.log('=== All WASM tests completed ===');
}

main().catch(console.error);
