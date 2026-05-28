package io.github.lxp3.avioflow;

public final class AudioWriteOptions {
    String codecName;
    String containerFormat;
    Integer sampleRate;
    Integer numChannels;
    Long bitRate;
    String sampleFormat;
    boolean overwrite = true;

    public static AudioWriteOptions format(String format) {
        return new AudioWriteOptions().containerFormat(format);
    }

    public AudioWriteOptions codecName(String codecName) {
        this.codecName = codecName;
        return this;
    }

    public AudioWriteOptions containerFormat(String containerFormat) {
        this.containerFormat = containerFormat;
        return this;
    }

    public AudioWriteOptions sampleRate(int sampleRate) {
        this.sampleRate = sampleRate;
        return this;
    }

    public AudioWriteOptions numChannels(int numChannels) {
        this.numChannels = numChannels;
        return this;
    }

    public AudioWriteOptions bitRate(long bitRate) {
        this.bitRate = bitRate;
        return this;
    }

    public AudioWriteOptions sampleFormat(String sampleFormat) {
        this.sampleFormat = sampleFormat;
        return this;
    }

    public AudioWriteOptions overwrite(boolean overwrite) {
        this.overwrite = overwrite;
        return this;
    }
}

