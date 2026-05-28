package io.github.lxp3.avioflow;

import org.junit.jupiter.api.Test;

import java.nio.file.Files;
import java.nio.file.Path;

import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

final class AvioflowTest {
    @Test
    void decodesAudioFile() {
        String audio = System.getProperty("avioflow.test.audio");
        try (AudioDecoder decoder = new AudioDecoder(new AudioStreamOptions().outputSampleRate(16000))) {
            decoder.open(audio);
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
            decoder.open(audio);
            samples = decoder.getSamples();
        }

        Path output = Files.createTempFile("avioflow-java-", ".wav");
        AudioEncoder.saveAudio(
                output.toString(),
                samples,
                new AudioWriteOptions().containerFormat("wav").codecName("pcm_s16le").sampleRate(16000));

        assertTrue(Files.size(output) > 0);
        try (AudioDecoder decoder = new AudioDecoder()) {
            decoder.open(output.toString());
            assertNotNull(decoder.getMetadata());
            assertTrue(decoder.getSamples()[0].length > 0);
        }
    }
}

