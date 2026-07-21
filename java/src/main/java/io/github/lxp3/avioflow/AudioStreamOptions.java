package io.github.lxp3.avioflow;

public final class AudioStreamOptions {
    Integer outputSampleRate;
    Integer outputNumChannels;
    Integer inputSampleRate;
    Integer inputChannels;
    String inputFormat;

    /** Sets the target sample rate; -1 preserves the source sample rate. */
    public AudioStreamOptions outputSampleRate(int outputSampleRate) {
        this.outputSampleRate = outputSampleRate;
        return this;
    }

    /** Sets the target channel count; -1 preserves the source channel count. */
    public AudioStreamOptions outputNumChannels(int outputNumChannels) {
        this.outputNumChannels = outputNumChannels;
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
