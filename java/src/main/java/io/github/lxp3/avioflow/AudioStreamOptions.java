package io.github.lxp3.avioflow;

public final class AudioStreamOptions {
    Integer outputSampleRate;
    Integer inputSampleRate;
    Integer inputChannels;
    String inputFormat;

    public AudioStreamOptions outputSampleRate(int outputSampleRate) {
        this.outputSampleRate = outputSampleRate;
        return this;
    }

    public AudioStreamOptions inputSampleRate(int inputSampleRate) {
        this.inputSampleRate = inputSampleRate;
        return this;
    }

    public AudioStreamOptions inputChannels(int inputChannels) {
        this.inputChannels = inputChannels;
        return this;
    }

    public AudioStreamOptions inputFormat(String inputFormat) {
        this.inputFormat = inputFormat;
        return this;
    }
}

