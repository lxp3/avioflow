import avioflow from 'avioflow'
import path from 'path'
import { fileURLToPath } from 'url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))

console.log('--- avioflow NPM Package Test ---')

// 1. Test Device Discovery
console.log('\nEnumerating Audio Devices:')
try {
    const devices = avioflow.listAudioDevices()
    devices.forEach((dev, i) => {
        console.log(`[${i}] ${dev.isOutput ? 'Output' : 'Input'}: ${dev.name} (${dev.description})`)
    })
} catch (err) {
    console.error('Failed to list devices:', err)
}

// 2. Test Decoder with MP3 file
const testFile = path.resolve(__dirname, '../../public/wavs/TownTheme.mp3')
console.log(`\nTesting Decoder with: ${testFile}`)

try {
    const decoder = new avioflow.AudioDecoder()
    decoder.open(testFile)

    // Print metadata
    const meta = decoder.getMetadata()
    console.log('\nMetadata:')
    console.log(`  Duration: ${meta.duration.toFixed(2)}s`)
    console.log(`  Sample Rate: ${meta.sampleRate} Hz`)
    console.log(`  Channels: ${meta.numChannels}`)
    console.log(`  Codec: ${meta.codec}`)

    // Decode first frame
    const frame = decoder.decodeNext()
    if (frame) {
        console.log(`\nFirst frame: ${frame.channels} channels, ${frame.data[0].length} samples`)
    }

    console.log('Decoder test passed!')
} catch (err) {
    console.error('Decoder test failed:', err)
}

console.log('\n--- Test Finished ---')
