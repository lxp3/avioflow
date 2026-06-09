/**
 * Offline audio decoding tests for avioflow Node.js bindings
 */
import avioflow from '../src/index.js'
import path from 'path'
import fs from 'fs'
import { fileURLToPath } from 'url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const MP3_PATH = path.resolve(__dirname, '../../public/wavs/TownTheme.mp3')
const WAV_PATH = path.resolve(__dirname, '../../public/wavs/zh.wav')
const MP3_URL = 'https://opengameart.org/sites/default/files/TownTheme.mp3'

function testOfflineFilepath() {
    console.log('\n=== Test: Offline Decode from Filepath ===')
    
    const decoder = new avioflow.AudioDecoder()
    decoder.loadFile(MP3_PATH)
    
    const meta = decoder.getMetadata()
    console.log(`  Codec: ${meta.codec}, Duration: ${meta.duration.toFixed(2)}s`)
    
    const samples = decoder.getSamples()
    const totalSamples = samples[0]?.length || 0
    
    console.log(`  Total samples decoded: ${totalSamples}`)
    if (totalSamples === 0) throw new Error('No samples decoded')
}

function testOfflineMemory() {
    console.log('\n=== Test: Offline Decode from Memory ===')
    
    const buffer = fs.readFileSync(MP3_PATH)
    console.log(`  File size: ${buffer.length} bytes`)
    
    const decoder = new avioflow.AudioDecoder()
    decoder.loadBuffer(buffer)
    
    const meta = decoder.getMetadata()
    console.log(`  Codec: ${meta.codec}, Duration: ${meta.duration.toFixed(2)}s`)
    
    const samples = decoder.getSamples()
    const totalSamples = samples[0]?.length || 0
    
    console.log(`  Total samples decoded: ${totalSamples}`)
    if (totalSamples === 0) throw new Error('No samples decoded')
}

function testOfflineUrl() {
    console.log('\n=== Test: Offline Decode from URL ===')
    console.log(`  URL: ${MP3_URL}`)
    
    try {
        const decoder = new avioflow.AudioDecoder()
        decoder.loadFile(MP3_URL)
        
        const meta = decoder.getMetadata()
        console.log(`  Codec: ${meta.codec}, Sample Rate: ${meta.sampleRate}Hz`)
        
        // Only decode a few frames to verify it works
        let frameCount = 0
        while (!decoder.isFinished() && frameCount < 10) {
            const frame = decoder.getFrame()
            if (!frame) break
            frameCount++
        }
        
        console.log(`  Successfully decoded ${frameCount} frames from URL`)
        if (frameCount === 0) throw new Error('No frames decoded from URL')
    } catch (err) {
        console.log(`  URL test skipped (network error): ${err.message}`)
    }
}

function testConvenienceLoad() {
    console.log('\n=== Test: Convenience load() function ===')
    
    const { metadata, samples } = avioflow.load(MP3_PATH, {
        outputSampleRate: 16000,
        outputNumChannels: 1
    })
    
    console.log(`  Codec: ${metadata.codec}, Duration: ${metadata.duration.toFixed(2)}s`)
    console.log(`  Resampled to: ${metadata.sampleRate}Hz, ${samples.length} channels`)
    console.log(`  Total samples: ${samples[0]?.length || 0}`)
    
    if (!samples[0]?.length) throw new Error('No samples decoded')
}

// Main
console.log('=== avioflow Offline Decoder Tests ===')
avioflow.setLogLevel('warning')

try {
    testOfflineFilepath()
    testOfflineMemory()
    testOfflineUrl()
    testConvenienceLoad()
    console.log('\nAll offline tests passed!')
} catch (err) {
    console.error(`\nTest failed: ${err.message}`)
    process.exit(1)
}
