package io.github.lxp3.avioflow;

public final class AudioStreamOptions {
    Integer outputSampleRate;
    Integer outputNumChannels;
    Integer inputSampleRate;
    Integer inputChannels;
    String inputFormat;
    Integer audioStreamIndex;

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

    /**
     * Sets the container stream index of the audio track to decode, taken from
     * {@link AudioDecoder#getStreams()}. Unset picks the best audio stream.
     */
    public AudioStreamOptions audioStreamIndex(int audioStreamIndex) {
        this.audioStreamIndex = audioStreamIndex;
        return this;
    }
}
