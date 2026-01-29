import avioflow from '../../avioflow/nodejs/index.js'
import path from 'path'
import { fileURLToPath } from 'url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))

console.log('--- avioflow Node-API Test ---')

// 1. Test Device Discovery
console.log('\nEnumerating Audio Devices:')
try {
    const devices = avioflow.listAudioDevices()
    if (devices.length === 0) {
        console.log('  No devices found (this is OK in some environments)')
    } else {
        devices.forEach((dev, i) => {
            console.log(`[${i}] ${dev.isOutput ? 'Output' : 'Input'}: ${dev.name} (${dev.description})`)
        })
    }
} catch (err) {
    console.error('Failed to list devices:', err.message)
}

// 2. Test Decoder with MP3 file
const testFile = path.resolve(__dirname, '../../public/wavs/TownTheme.mp3')
console.log(`\nTesting Decoder with: ${testFile}`)

try {
    const decoder = new avioflow.AudioDecoder()
    
    // New API: load() returns metadata
    const meta = decoder.load(testFile)
    
    // Print metadata
    console.log('\nMetadata:')
    console.log(`  Duration: ${meta.duration.toFixed(2)}s`)
    console.log(`  Sample Rate: ${meta.sampleRate} Hz`)
    console.log(`  Channels: ${meta.numChannels}`)
    console.log(`  Codec: ${meta.codec}`)
    console.log(`  Container: ${meta.container}`)
    
    // Decode first frame - returns array of Float32Arrays
    let totalSamples = 0
    let frameCount = 0
    
    while (!decoder.isFinished()) {
        const frame = decoder.decodeNext()  // Returns Float32Array[] or null
        if (frame) {
            frameCount++
            totalSamples += frame[0].length
            if (frameCount === 1) {
                console.log(`\nFirst frame: ${frame.length} channels, ${frame[0].length} samples`)
            }
        } else {
            break
        }
    }
    
    console.log(`\nDecoded ${frameCount} frames, ${totalSamples} total samples per channel`)
    console.log('Decoder test passed!')
} catch (err) {
    console.error('Decoder test failed:', err)
}

console.log('\n--- Test Finished ---')
