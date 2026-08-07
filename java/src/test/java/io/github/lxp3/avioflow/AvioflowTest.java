package io.github.lxp3.avioflow;

import org.junit.jupiter.api.Test;

import java.nio.file.Files;
import java.nio.file.Path;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

final class AvioflowTest {
    /**
     * Only rounding of the rate ratio may move the count; the resampler tail is
     * drained, so a larger gap means samples are being dropped. Mirrors
     * SAMPLE_COUNT_TOLERANCE in avioflow/bin/resample-test.cpp.
     */
    private static final int SAMPLE_COUNT_TOLERANCE = 2;

    private static float[][] makeSine(int numChannels, int numSamples, int sampleRate, double freq) {
        float[][] samples = new float[numChannels][numSamples];
        for (int i = 0; i < numSamples; i++) {
            float value = (float) Math.sin(2 * Math.PI * freq * i / sampleRate);
            for (int c = 0; c < numChannels; c++) {
                samples[c][i] = value;
            }
        }
        return samples;
    }

    @Test
    void decodesAudioFile() {
        String audio = System.getProperty("avioflow.test.audio");
        try (AudioDecoder decoder = new AudioDecoder(new AudioStreamOptions().outputSampleRate(16000))) {
            decoder.loadFile(audio);
            Metadata metadata = decoder.getMetadata();
            float[][] samples = decoder.getSamples();

            assertTrue(metadata.sampleRate > 0);
            assertTrue(samples.length > 0);
            assertTrue(samples[0].length > 0);
        }
    }

    @Test
    void encodesAudioFile() throws Exception {
        String audio = System.getProperty("avioflow.test.audio");
        float[][] samples;
        try (AudioDecoder decoder = new AudioDecoder(new AudioStreamOptions().outputSampleRate(16000))) {
            decoder.loadFile(audio);
            samples = decoder.getSamples();
        }

        Path output = Files.createTempFile("avioflow-java-", ".wav");
        AudioEncoder.saveAudio(
                output.toString(),
                samples,
                new AudioWriteOptions().containerFormat("wav").codecName("pcm_s16le").sampleRate(16000));

        assertTrue(Files.size(output) > 0);
        try (AudioDecoder decoder = new AudioDecoder()) {
            decoder.loadFile(output.toString());
            assertNotNull(decoder.getMetadata());
            assertTrue(decoder.getSamples()[0].length > 0);
        }
    }

    @Test
    void resamplesCompleteBuffer() {
        float[][] input = makeSine(2, 44100, 44100, 440.0);

        float[][] output = AudioResampler.resample(input, 44100, 16000);

        assertEquals(2, output.length);
        assertTrue(Math.abs(output[0].length - 16000) <= SAMPLE_COUNT_TOLERANCE,
                "expected ~16000 samples, got " + output[0].length);

        // 440 Hz sits well below the 8 kHz Nyquist limit, so amplitude survives.
        float peak = 0;
        for (float value : output[0]) {
            peak = Math.max(peak, Math.abs(value));
            assertTrue(Float.isFinite(value), "output contains NaN or Inf");
        }
        assertTrue(peak > 0.9f && peak < 1.1f, "peak amplitude was " + peak);
    }

    @Test
    void resampleDownmixesToMono() {
        float[][] output = AudioResampler.resample(makeSine(2, 16000, 16000, 440.0), 16000, 16000, 1);

        assertEquals(1, output.length);
        assertEquals(16000, output[0].length);
    }

    @Test
    void flushRecoversTheTail() {
        try (AudioResampler resampler = new AudioResampler(new AudioResampleOptions(44100, 16000))) {
            float[][] body = resampler.process(makeSine(1, 44100, 44100, 440.0));
            float[][] tail = resampler.flush();

            int withoutFlush = body[0].length;
            int withFlush = withoutFlush + tail[0].length;

            // The contract: only after flush() does the count match the rate ratio.
            assertTrue(withFlush >= withoutFlush);
            assertTrue(Math.abs(withFlush - 16000) <= SAMPLE_COUNT_TOLERANCE,
                    "expected ~16000 samples after flush, got " + withFlush);
            assertTrue(Math.abs(withoutFlush - 16000) > SAMPLE_COUNT_TOLERANCE,
                    "expected process() alone to fall short, got " + withoutFlush);
        }
    }

    @Test
    void chunkedResamplingMatchesOneShot() {
        int sampleRate = 48000;
        float[][] input = makeSine(1, sampleRate, sampleRate, 220.0);
        float[] expected = AudioResampler.resample(input, sampleRate, 16000)[0];

        float[] chunked = new float[expected.length + 64];
        int written = 0;
        try (AudioResampler resampler = new AudioResampler(new AudioResampleOptions(sampleRate, 16000))) {
            for (int offset = 0; offset < sampleRate; offset += 1000) {
                int length = Math.min(1000, sampleRate - offset);
                float[][] chunk = new float[1][length];
                System.arraycopy(input[0], offset, chunk[0], 0, length);
                float[][] part = resampler.process(chunk);
                if (part.length > 0) {
                    System.arraycopy(part[0], 0, chunked, written, part[0].length);
                    written += part[0].length;
                }
            }
            float[][] tail = resampler.flush();
            if (tail.length > 0) {
                System.arraycopy(tail[0], 0, chunked, written, tail[0].length);
                written += tail[0].length;
            }
        }

        // Filter state carries across chunks, so results match sample for sample.
        assertEquals(expected.length, written);
        float maxDiff = 0;
        for (int i = 0; i < expected.length; i++) {
            maxDiff = Math.max(maxDiff, Math.abs(chunked[i] - expected[i]));
        }
        assertTrue(maxDiff < 1e-6f, "max sample difference was " + maxDiff);
    }

    @Test
    void reportsOutputRateAndChannels() {
        try (AudioResampler resampler = new AudioResampler(new AudioResampleOptions(44100, 16000))) {
            assertEquals(16000, resampler.outputSampleRate());
            // Channel count is unknown until the first chunk reveals it.
            assertEquals(0, resampler.outputNumChannels());

            resampler.process(makeSine(2, 1000, 44100, 440.0));
            assertEquals(2, resampler.outputNumChannels());
        }
    }

    @Test
    void rejectsInvalidResampleArguments() {
        float[][] input = makeSine(1, 100, 16000, 440.0);

        assertThrows(AvioflowException.class, () -> AudioResampler.resample(input, 0, 16000));
        assertThrows(AvioflowException.class, () -> AudioResampler.resample(input, 16000, -1));
        assertThrows(AvioflowException.class,
                () -> new AudioResampler(new AudioResampleOptions(16000, 0)));

        try (AudioResampler resampler = new AudioResampler(new AudioResampleOptions(16000, 8000))) {
            resampler.process(makeSine(2, 100, 16000, 440.0));
            assertThrows(AvioflowException.class,
                    () -> resampler.process(makeSine(1, 100, 16000, 440.0)));
        }
    }

    @Test
    void usingAClosedResamplerFails() {
        AudioResampler resampler = new AudioResampler(new AudioResampleOptions(16000, 8000));
        resampler.close();
        assertThrows(IllegalStateException.class, resampler::flush);
    }
}

