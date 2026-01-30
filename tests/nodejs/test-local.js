import avioflow from '../../avioflow/nodejs/index.js'
import path from 'path'
import fs from 'fs'
import { fileURLToPath } from 'url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const testFile = path.resolve(__dirname, '../../public/wavs/TownTheme.mp3')

/**
 * Offline Test: Tests the file-based decoding API
 */
async function offline_test() {
    console.log('\n--- Running Offline Test ---')
    console.log(`Testing with file: ${testFile}`)

    try {
        // 1. Test convenience load() function
        console.log('\n[1/2] Testing avioflow.load() convenience function...')
        const { metadata, samples } = avioflow.load(testFile, {
            outputSampleRate: 16000,
            outputNumChannels: 1
        })

        console.log('Metadata:', {
            duration: metadata.duration.toFixed(2) + 's',
            sampleRate: metadata.sampleRate,
            numChannels: metadata.numChannels,
            codec: metadata.codec
        })
        console.log(`Samples decoded: ${samples.length} channels, ${samples[0].length} samples`)

        // 2. Test AudioDecoder instance with getAllSamples()
        console.log('\n[2/2] Testing AudioDecoder.getAllSamples()...')
        const decoder = new avioflow.AudioDecoder({ outputSampleRate: 44100 })
        const meta = decoder.load(testFile)
        const allSamples = decoder.getAllSamples()

        console.log(`Initial Metadata duration: ${meta.duration.toFixed(2)}s`)
        console.log(`Total samples per channel: ${allSamples[0].length}`)

        if (allSamples[0].length > 0) {
            console.log('Offline Test Passed!')
        } else {
            throw new Error('No samples decoded')
        }
    } catch (err) {
        console.error('Offline Test Failed:', err)
        throw err
    }
}

/**
 * Online Test: Tests the streaming/push-based API
 */
async function online_test() {
    console.log('\n--- Running Online Test (Streaming Simulation) ---')

    try {
        // We use the same MP3 file but read it in chunks to simulate a stream.
        // For actual streaming, we need to know the input format if it's raw PCM,
        // but for MP3/AAC, the decoder can auto-detect if the container is supported.
        const decoder = new avioflow.AudioDecoder({
            outputSampleRate: 16000,
            outputNumChannels: 1
        })

        const buffer = fs.readFileSync(testFile)
        const chunkSize = 16 * 1024 // 16KB chunks
        let totalDecodedSamples = 0
        let frameCount = 0

        console.log(`Pushing ${buffer.length} bytes in ${chunkSize} byte chunks...`)

        for (let i = 0; i < buffer.length; i += chunkSize) {
            const chunk = buffer.slice(i, Math.min(i + chunkSize, buffer.length))
            decoder.push(chunk)

            let frame
            while ((frame = decoder.decodeNext()) !== null) {
                frameCount++
                totalDecodedSamples += frame[0].length
            }
        }

        console.log(`Streaming simulation finished.`)
        console.log(`Total frames: ${frameCount}, Total samples: ${totalDecodedSamples}`)

        if (totalDecodedSamples > 0) {
            console.log('Online Test Passed!')
        } else {
            throw new Error('No samples decoded in streaming mode')
        }
    } catch (err) {
        console.error('Online Test Failed:', err)
        throw err
    }
}

// Main execution
(async () => {
    console.log('--- avioflow Node-API Comprehensive Test ---')

    // Set log level to see what's happening internally
    try {
        avioflow.setLogLevel('info')
    } catch (e) {
        console.warn('SetLogLevel not supported or failed')
    }

    try {
        // Enumerate devices helper test
        const devices = avioflow.listAudioDevices()
        console.log(`Available devices: ${devices.length}`)

        await offline_test()
        await online_test()

        console.log('\nALL TESTS PASSED!')
    } catch (err) {
        console.error('\nTests Failed!')
        process.exit(1)
    }
})()
