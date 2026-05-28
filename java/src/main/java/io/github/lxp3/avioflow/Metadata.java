package io.github.lxp3.avioflow;

public final class Metadata {
    public final double duration;
    public final long numSamples;
    public final int sampleRate;
    public final int numChannels;
    public final String sampleFormat;
    public final String codec;
    public final long bitRate;
    public final String container;

    public Metadata(
            double duration,
            long numSamples,
            int sampleRate,
            int numChannels,
            String sampleFormat,
            String codec,
            long bitRate,
            String container) {
        this.duration = duration;
        this.numSamples = numSamples;
        this.sampleRate = sampleRate;
        this.numChannels = numChannels;
        this.sampleFormat = sampleFormat;
        this.codec = codec;
        this.bitRate = bitRate;
        this.container = container;
    }
}

