import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import createAvioflow from '../dist/avioflow.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Path to test audio file
const audioPath = path.resolve(__dirname, '../../public/wavs/TownTheme.mp3');

async function testOfflineLoad() {
    console.log('Loading Avioflow module...');
    const module = await createAvioflow();
    console.log('Avioflow module loaded.');

    console.log(`Reading audio file: ${audioPath}`);
    const buffer = fs.readFileSync(audioPath);
    console.log(`Read ${buffer.length} bytes.`);

    console.log('Decoding audio...');
    // Use loadBuffer for buffer input
    try {
        const result = module.loadBuffer(buffer);

        console.log('Result type:', typeof result);

        // WASM bindings return a JS object { metadata, samples }
        const metadata = result.metadata;
        const samples = result.samples;

        console.log('Metadata:', {
            duration: metadata.duration,
            sampleRate: metadata.sampleRate,
            numChannels: metadata.numChannels,
            codec: metadata.codec
        });

        console.log('Samples info:', {
             isArray: Array.isArray(samples),
             numChannels: samples.length,
             channelLength: samples[0] ? samples[0].length : 0
        });

        if (metadata.duration > 0 && metadata.numChannels === 2) {
            console.log('SUCCESS: Audio decoded successfully with expected metadata.');
        } else {
            console.error('FAILURE: Unexpected metadata values.');
            process.exit(1);
        }

    } catch (error) {
        console.error('Error during decoding:', error);
        process.exit(1);
    }
}

testOfflineLoad();
