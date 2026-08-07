package io.github.lxp3.avioflow;

/** Options for {@link AudioResampler}. Both sample rates are required. */
public final class AudioResampleOptions {
    int inputSampleRate;
    int outputSampleRate;
    Integer outputNumChannels;

    public AudioResampleOptions() {
    }

    /**
     * @param inputSampleRate source sample rate in Hz, must be greater than zero
     * @param outputSampleRate target sample rate in Hz, must be greater than zero
     */
    public AudioResampleOptions(int inputSampleRate, int outputSampleRate) {
        this.inputSampleRate = inputSampleRate;
        this.outputSampleRate = outputSampleRate;
    }

    /** Sets the source sample rate in Hz; must be greater than zero. */
    public AudioResampleOptions inputSampleRate(int inputSampleRate) {
        this.inputSampleRate = inputSampleRate;
        return this;
    }

    /** Sets the target sample rate in Hz; must be greater than zero. */
    public AudioResampleOptions outputSampleRate(int outputSampleRate) {
        this.outputSampleRate = outputSampleRate;
        return this;
    }

    /** Sets the target channel count; defaults to the input channel count. */
    public AudioResampleOptions outputNumChannels(int outputNumChannels) {
        this.outputNumChannels = outputNumChannels;
        return this;
    }
}
