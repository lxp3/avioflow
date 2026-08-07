/**
 * Standalone resampling tests for the avioflow Node.js bindings:
 * resample() and the AudioResampler class.
 */
import avioflow from '../src/index.js'

// Only rounding of the rate ratio may move the count; the resampler tail is
// drained, so a larger gap means samples are being dropped. Mirrors
// SAMPLE_COUNT_TOLERANCE in avioflow/bin/resample-test.cpp.
const SAMPLE_COUNT_TOLERANCE = 2

/** Generate a sine wave so output can be checked for amplitude, not just length. */
function makeSine(numChannels, numSamples, sampleRate, freq = 440) {
    const wave = new Float32Array(numSamples)
    for (let i = 0; i < numSamples; i++) {
        wave[i] = Math.sin((2 * Math.PI * freq * i) / sampleRate)
    }
    return Array.from({ length: numChannels }, () => Float32Array.from(wave))
}

function assertSampleCount(actual, expected, context) {
    const diff = actual - expected
    if (Math.abs(diff) > SAMPLE_COUNT_TOLERANCE) {
        throw new Error(`${context}: expected ~${expected} samples, got ${actual} (diff ${diff})`)
    }
}

function peak(channel) {
    let max = 0
    for (const value of channel) max = Math.max(max, Math.abs(value))
    return max
}

function assertThrows(description, fn) {
    try {
        fn()
    } catch {
        return
    }
    throw new Error(`${description} should have thrown`)
}

function testOneShotDownsample() {
    console.log('\n=== Test: One-shot downsample 44100 -> 16000 ===')
    const input = makeSine(2, 44100, 44100)
    const output = avioflow.resample(input, 44100, 16000)

    if (output.length !== 2) throw new Error(`expected 2 channels, got ${output.length}`)
    assertSampleCount(output[0].length, 16000, '44100 -> 16000')
    if (output[1].length !== output[0].length) throw new Error('channel lengths differ')

    // 440 Hz sits well below the 8 kHz Nyquist limit, so amplitude survives.
    const amplitude = peak(output[0])
    console.log(`  ${output.length} ch x ${output[0].length} samples, peak ${amplitude.toFixed(3)}`)
    if (amplitude < 0.9 || amplitude > 1.1) throw new Error(`peak was ${amplitude}`)
    if (!output[0].every(Number.isFinite)) throw new Error('output contains NaN or Inf')
}

function testUpsample() {
    console.log('\n=== Test: Upsample 16000 -> 44100 ===')
    const output = avioflow.resample(makeSine(2, 16000, 16000), 16000, 44100)
    if (output.length !== 2) throw new Error('channel count changed')
    assertSampleCount(output[0].length, 44100, '16000 -> 44100')
    console.log(`  ${output[0].length} samples`)
}

function testEqualRatesPassThrough() {
    console.log('\n=== Test: Equal rates pass through ===')
    const output = avioflow.resample(makeSine(1, 4096, 16000), 16000, 16000)
    if (output.length !== 1 || output[0].length !== 4096) {
        throw new Error(`expected 1x4096, got ${output.length}x${output[0].length}`)
    }
    console.log('  unchanged')
}

function testDownmixToMono() {
    console.log('\n=== Test: Stereo to mono ===')
    const output = avioflow.resample(makeSine(2, 16000, 16000), 16000, 16000, 1)
    if (output.length !== 1 || output[0].length !== 16000) {
        throw new Error(`expected 1x16000, got ${output.length}x${output[0].length}`)
    }
    console.log('  1 channel')
}

function testFlushRecoversTail() {
    console.log('\n=== Test: flush() recovers the tail ===')
    const resampler = new avioflow.AudioResampler({
        inputSampleRate: 44100,
        outputSampleRate: 16000,
    })

    const body = resampler.process(makeSine(1, 44100, 44100))
    const tail = resampler.flush()
    const withoutFlush = body[0].length
    const withFlush = withoutFlush + tail[0].length

    console.log(`  process()=${withoutFlush}, flush()=${tail[0].length}, total=${withFlush}`)

    // The contract: only after flush() does the count match the rate ratio.
    if (withFlush < withoutFlush) throw new Error('flush lost samples')
    assertSampleCount(withFlush, 16000, 'process + flush')
    if (Math.abs(withoutFlush - 16000) <= SAMPLE_COUNT_TOLERANCE) {
        throw new Error(`expected process() alone to fall short, got ${withoutFlush}`)
    }
}

function testChunkedMatchesOneShot() {
    console.log('\n=== Test: Chunked equals one-shot ===')
    const sampleRate = 48000
    const input = makeSine(1, sampleRate, sampleRate, 220)
    const expected = avioflow.resample(input, sampleRate, 16000)[0]

    const resampler = new avioflow.AudioResampler({
        inputSampleRate: sampleRate,
        outputSampleRate: 16000,
    })

    const parts = []
    for (let i = 0; i < sampleRate; i += 1000) {
        parts.push(resampler.process([input[0].subarray(i, i + 1000)])[0])
    }
    parts.push(resampler.flush()[0])

    let total = 0
    for (const part of parts) total += part.length
    const chunked = new Float32Array(total)
    let offset = 0
    for (const part of parts) {
        chunked.set(part, offset)
        offset += part.length
    }

    // Filter state carries across chunks, so results match sample for sample.
    if (chunked.length !== expected.length) {
        throw new Error(`length differs: ${chunked.length} vs ${expected.length}`)
    }
    let maxDiff = 0
    for (let i = 0; i < chunked.length; i++) {
        maxDiff = Math.max(maxDiff, Math.abs(chunked[i] - expected[i]))
    }
    console.log(`  ${chunked.length} samples, max diff ${maxDiff.toExponential(2)}`)
    if (maxDiff > 1e-6) throw new Error(`max difference was ${maxDiff}`)
}

function testOutputRateAndChannelsReported() {
    console.log('\n=== Test: Output rate and channel count ===')
    const resampler = new avioflow.AudioResampler({
        inputSampleRate: 44100,
        outputSampleRate: 16000,
    })

    if (resampler.outputSampleRate() !== 16000) throw new Error('wrong output rate')
    // Channel count is unknown until the first chunk reveals it.
    if (resampler.outputNumChannels() !== 0) throw new Error('expected 0 channels before process')

    resampler.process(makeSine(2, 1000, 44100))
    if (resampler.outputNumChannels() !== 2) throw new Error('expected 2 channels after process')
    console.log('  16000 Hz, 0 channels before process, 2 after')
}

function testEmptyFlush() {
    console.log('\n=== Test: flush() before any process() ===')
    const resampler = new avioflow.AudioResampler({
        inputSampleRate: 44100,
        outputSampleRate: 16000,
    })
    const tail = resampler.flush()
    if (tail.length !== 0) throw new Error(`expected no channels, got ${tail.length}`)
    console.log('  handled')
}

function testInvalidArguments() {
    console.log('\n=== Test: Invalid arguments ===')
    const input = makeSine(1, 100, 16000)

    assertThrows('zero input rate', () => avioflow.resample(input, 0, 16000))
    assertThrows('negative output rate', () => avioflow.resample(input, 16000, -1))
    assertThrows('non-array samples', () => avioflow.resample('nope', 16000, 8000))
    assertThrows('non-Float32Array channel', () => avioflow.resample([[1, 2, 3]], 16000, 8000))
    assertThrows('constructor zero rate', () => new avioflow.AudioResampler({
        inputSampleRate: 16000,
        outputSampleRate: 0,
    }))

    const resampler = new avioflow.AudioResampler({
        inputSampleRate: 16000,
        outputSampleRate: 8000,
    })
    resampler.process(makeSine(2, 100, 16000))
    assertThrows('channel count change', () => resampler.process(makeSine(1, 100, 16000)))
    console.log('  all rejected')
}

function main() {
    try {
        testOneShotDownsample()
        testUpsample()
        testEqualRatesPassThrough()
        testDownmixToMono()
        testFlushRecoversTail()
        testChunkedMatchesOneShot()
        testOutputRateAndChannelsReported()
        testEmptyFlush()
        testInvalidArguments()
        console.log('\nAll resample tests passed!')
    } catch (error) {
        console.error(`\nTest failed: ${error.message}`)
        process.exit(1)
    }
}

main()
