package io.github.lxp3.avioflow;

import java.util.Objects;

/**
 * Stateful resampler for audio that arrives in chunks.
 *
 * <p>Filter state is preserved across {@link #process} calls, so consecutive
 * chunks join without discontinuities at the boundaries. For a buffer already
 * held in full, {@link #resample} is simpler.
 *
 * <p>{@link #flush} is not optional: the resampler holds back the last few
 * milliseconds of audio internally, and skipping the flush discards them.
 *
 * <pre>{@code
 * try (AudioResampler resampler = new AudioResampler(
 *         new AudioResampleOptions(44100, 16000))) {
 *     for (float[][] chunk : chunks) {
 *         append(output, resampler.process(chunk));
 *     }
 *     append(output, resampler.flush());   // else the tail is lost
 * }
 * }</pre>
 */
public final class AudioResampler implements AutoCloseable {
    private long handle;

    public AudioResampler(AudioResampleOptions options) {
        NativeLibraryLoader.load();
        this.handle = nativeCreate(Objects.requireNonNull(options, "options"));
    }

    /**
     * Resamples one chunk of {@code samples[channel][sample]}.
     *
     * <p>May return fewer samples than the rate ratio suggests, because the
     * resampler buffers samples internally to keep filter continuity. The
     * remainder is emitted by {@link #flush}.
     *
     * @param samples input; all channels must have the same length, and the
     *     channel count must not change between calls
     * @return resampled samples as {@code [channel][sample]}
     */
    public synchronized float[][] process(float[][] samples) {
        ensureOpen();
        return nativeProcess(handle, Objects.requireNonNull(samples, "samples"));
    }

    /**
     * Drains the samples still buffered inside the resampler.
     *
     * <p>Call once after the final {@link #process} call.
     *
     * @return the remaining samples as {@code [channel][sample]}
     */
    public synchronized float[][] flush() {
        ensureOpen();
        return nativeFlush(handle);
    }

    /** Returns the output sample rate the resampler was configured with. */
    public synchronized int outputSampleRate() {
        ensureOpen();
        return nativeOutputSampleRate(handle);
    }

    /** Returns the output channel count; zero until the first {@link #process} call. */
    public synchronized int outputNumChannels() {
        ensureOpen();
        return nativeOutputNumChannels(handle);
    }

    @Override
    public synchronized void close() {
        long current = handle;
        if (current != 0) {
            handle = 0;
            nativeDestroy(current);
        }
    }

    /**
     * Resamples a complete buffer in one call, flushing internally so no samples
     * are lost.
     *
     * <p>For audio arriving in chunks use {@link AudioResampler} instead: calling
     * this per chunk would reset the filter state and introduce a discontinuity
     * at every boundary.
     *
     * @param samples input as {@code [channel][sample]}
     * @param inputSampleRate source sample rate in Hz, must be greater than zero
     * @param outputSampleRate target sample rate in Hz, must be greater than zero
     * @return resampled samples as {@code [channel][sample]}
     */
    public static float[][] resample(float[][] samples, int inputSampleRate, int outputSampleRate) {
        return resample(samples, inputSampleRate, outputSampleRate, -1);
    }

    /**
     * Resamples a complete buffer in one call, also setting the channel count.
     *
     * @param outputNumChannels target channel count; -1 keeps the input count
     */
    public static float[][] resample(float[][] samples, int inputSampleRate, int outputSampleRate,
                                     int outputNumChannels) {
        NativeLibraryLoader.load();
        return nativeResample(Objects.requireNonNull(samples, "samples"),
                inputSampleRate, outputSampleRate, outputNumChannels);
    }

    private void ensureOpen() {
        if (handle == 0) {
            throw new IllegalStateException("AudioResampler is closed");
        }
    }

    private static native long nativeCreate(AudioResampleOptions options);

    private static native float[][] nativeProcess(long handle, float[][] samples);

    private static native float[][] nativeFlush(long handle);

    private static native int nativeOutputSampleRate(long handle);

    private static native int nativeOutputNumChannels(long handle);

    private static native void nativeDestroy(long handle);

    private static native float[][] nativeResample(float[][] samples, int inputSampleRate,
                                                   int outputSampleRate, int outputNumChannels);
}
