/**
 * Online (streaming) audio decoding tests for avioflow Node.js bindings
 */
import avioflow from '../src/index.js'
import path from 'path'
import fs from 'fs'
import { fileURLToPath } from 'url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const MP3_PATH = path.resolve(__dirname, '../../public/wavs/TownTheme.mp3')
const WAV_PATH = path.resolve(__dirname, '../../public/wavs/zh.wav')

function simulateStreaming(testName, filePath, format, sampleRate = 0, channels = 0) {
    console.log(`\n=== Running ${testName} (100ms chunks) ===`)
    
    const buffer = fs.readFileSync(filePath)
    console.log(`  File size: ${buffer.length} bytes`)
    
    // Initialize decoder with streaming options
    const options = { inputFormat: format }
    if (sampleRate > 0) options.inputSampleRate = sampleRate
    if (channels > 0) options.inputChannels = channels
    
    const decoder = new avioflow.AudioDecoder(options)
    
    // Calculate chunk size for 100ms
    let chunkSize
    if (format === 's16le' || format === 'wav') {
        const sr = sampleRate > 0 ? sampleRate : 16000
        const ch = channels > 0 ? channels : 1
        chunkSize = Math.floor(sr * ch * 2 * 0.1) // 16-bit = 2 bytes
    } else {
        chunkSize = 4096 // ~100ms for typical MP3 bitrate
    }
    
    let offset = 0
    let totalDecoded = 0
    let pushCount = 0
    const startTime = Date.now()
    
    // Push all data in chunks, decoding after each push
    while (offset < buffer.length) {
        const toPush = Math.min(chunkSize, buffer.length - offset)
        decoder.push(buffer.slice(offset, offset + toPush))
        offset += toPush
        pushCount++
        
        // Try to decode available frames
        let frame
        while ((frame = decoder.read()) !== null) {
            totalDecoded += frame[0]?.length || 0
        }
    }
    
    // Continue decoding until finished
    while (!decoder.isFinished()) {
        const frame = decoder.read()
        if (!frame) break
        totalDecoded += frame[0]?.length || 0
    }
    
    const elapsed = Date.now() - startTime
    console.log(`  Push count: ${pushCount}, Total samples: ${totalDecoded}`)
    console.log(`  Time: ${elapsed}ms`)
    
    if (totalDecoded === 0) throw new Error(`${testName} failed: No samples decoded`)
}

function testOnlineMp3() {
    // MP3 streaming (TownTheme.mp3 is 44100Hz, 2ch)
    simulateStreaming('Online MP3 Test', MP3_PATH, 'mp3', 44100, 2)
}

function testOnlineWav() {
    // WAV streaming (zh.wav is 16000Hz, 1ch)
    simulateStreaming('Online WAV Test', WAV_PATH, 'wav', 16000, 1)
}

function testOnlinePcmFallback() {
    console.log('\n=== Running Online PCM Fallback Test ===')
    
    let buffer = fs.readFileSync(WAV_PATH)
    
    // Strip 44 bytes WAV header to simulate raw PCM
    if (buffer.length > 44) {
        buffer = buffer.slice(44)
    }
    
    console.log(`  PCM data size: ${buffer.length} bytes (WAV header stripped)`)
    
    // Use "wav" format, but input is actually raw PCM
    // Decoder should automatically fallback to s16le
    const decoder = new avioflow.AudioDecoder({
        inputFormat: 'wav', // Intentional: should fallback to s16le
        inputSampleRate: 16000,
        inputChannels: 1
    })
    
    // Push in 100ms chunks
    const chunkSize = Math.floor(16000 * 1 * 2 * 0.1)
    let offset = 0
    let totalDecoded = 0
    const startTime = Date.now()
    
    while (offset < buffer.length) {
        const toPush = Math.min(chunkSize, buffer.length - offset)
        decoder.push(buffer.slice(offset, offset + toPush))
        offset += toPush
        
        let frame
        while ((frame = decoder.read()) !== null) {
            totalDecoded += frame[0]?.length || 0
        }
    }
    
    const elapsed = Date.now() - startTime
    console.log(`  Total samples: ${totalDecoded}`)
    console.log(`  Time: ${elapsed}ms`)
    
    if (totalDecoded === 0) throw new Error('PCM fallback test failed: No samples decoded')
}

// Main
console.log('=== avioflow Online (Streaming) Decoder Tests ===')
avioflow.setLogLevel('warning')

try {
    testOnlineMp3()
    testOnlineWav()
    testOnlinePcmFallback()
    console.log('\nAll online tests passed!')
} catch (err) {
    console.error(`\nTest failed: ${err.message}`)
    process.exit(1)
}
