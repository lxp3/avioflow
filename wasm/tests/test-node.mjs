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

async function main() {
    console.log('=== avioflow WASM Node.js Test ===\n');
    
    // Import WASM module
    console.log('[1/4] Loading WASM module...');
    const start = performance.now();
    
    const createAvioflow = (await import('../dist/avioflow.js')).default;
    const avioflow = await createAvioflow();
    
    console.log(`  Module loaded in ${(performance.now() - start).toFixed(1)}ms`);
    console.log(`  Available: ${Object.keys(avioflow).join(', ')}\n`);
    
    // Set log level
    avioflow.setLogLevel('warning');
    
    // Test 1: Load MP3 from buffer
    console.log('[2/4] Testing MP3 decode from buffer...');
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
    console.log('[3/4] Testing WAV decode with resampling...');
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
    console.log('[4/4] Testing streaming decode...');
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
            decoder.push(new Uint8Array(chunk));
            offset += chunkSize;
            
            let frame;
            while ((frame = decoder.decodeNext()) !== null) {
                totalSamples += frame[0]?.length || 0;
                frameCount++;
            }
        }
        
        // Drain
        while (!decoder.isFinished()) {
            const frame = decoder.decodeNext();
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
    
    console.log('=== All WASM tests completed ===');
}

main().catch(console.error);
