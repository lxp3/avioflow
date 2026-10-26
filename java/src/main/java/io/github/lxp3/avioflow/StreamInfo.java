package io.github.lxp3.avioflow;

/** One stream of an opened container (audio, video, subtitle, ...). */
public final class StreamInfo {
    /** Index to pass to {@link AudioStreamOptions#audioStreamIndex(int)}. */
    public final int index;
    /** "audio", "video", "subtitle", "data" or "attachment". */
    public final String mediaType;
    public final String codec;
    /** ISO 639 language tag from the container; may be empty. */
    public final String language;
    /** Audio streams only, otherwise 0. */
    public final int sampleRate;
    /** Audio streams only, otherwise 0. */
    public final int numChannels;
    /** Bit rate in bits per second, 0 if unknown. */
    public final long bitRate;
    /** Duration in seconds, 0 if unknown. */
    public final double duration;
    /** True if the container flags this as the default stream. */
    public final boolean isDefault;

    StreamInfo(
            int index,
            String mediaType,
            String codec,
            String language,
            int sampleRate,
            int numChannels,
            long bitRate,
            double duration,
            boolean isDefault) {
        this.index = index;
        this.mediaType = mediaType;
        this.codec = codec;
        this.language = language;
        this.sampleRate = sampleRate;
        this.numChannels = numChannels;
        this.bitRate = bitRate;
        this.duration = duration;
        this.isDefault = isDefault;
    }
}
