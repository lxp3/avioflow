package io.github.lxp3.avioflow;

import java.util.Objects;

public final class AudioDecoder implements AutoCloseable {
    private long handle;

    public AudioDecoder() {
        this(null);
    }

    public AudioDecoder(AudioStreamOptions options) {
        NativeLibraryLoader.load();
        this.handle = nativeCreate(Avioflow.streamOptionsOrDefault(options));
    }

    public synchronized Metadata loadFile(String path) {
        ensureOpen();
        return nativeLoadFile(handle, Objects.requireNonNull(path, "path"));
    }

    public synchronized Metadata loadBuffer(byte[] data) {
        ensureOpen();
        return nativeLoadBuffer(handle, Objects.requireNonNull(data, "data"));
    }

    public synchronized void feed(byte[] data) {
        ensureOpen();
        nativeFeed(handle, Objects.requireNonNull(data, "data"));
    }

    public synchronized void flush() {
        ensureOpen();
        nativeFlush(handle);
    }

    public synchronized float[][] getSamples() {
        return getSamples(0.0, Double.NaN);
    }

    /**
     * Decode samples in the half-open range [startSeconds, stopSeconds).
     * Offline mode only; pass {@link Double#NaN} for stopSeconds to decode to the end.
     */
    public synchronized float[][] getSamples(double startSeconds, double stopSeconds) {
        ensureOpen();
        return nativeGetSamples(handle, startSeconds, stopSeconds);
    }

    public synchronized float[][] getFrame() {
        ensureOpen();
        return nativeGetFrame(handle);
    }

    public synchronized Metadata getMetadata() {
        ensureOpen();
        return nativeGetMetadata(handle);
    }

    public synchronized boolean isFinished() {
        ensureOpen();
        return nativeIsFinished(handle);
    }

    @Override
    public synchronized void close() {
        long current = handle;
        if (current != 0) {
            handle = 0;
            nativeDestroy(current);
        }
    }

    private void ensureOpen() {
        if (handle == 0) {
            throw new IllegalStateException("AudioDecoder is closed");
        }
    }

    private static native long nativeCreate(AudioStreamOptions options);

    private static native Metadata nativeLoadFile(long handle, String path);

    private static native Metadata nativeLoadBuffer(long handle, byte[] data);

    private static native void nativeFeed(long handle, byte[] data);

    private static native void nativeFlush(long handle);

    private static native float[][] nativeGetSamples(long handle, double startSeconds, double stopSeconds);

    private static native float[][] nativeGetFrame(long handle);

    private static native Metadata nativeGetMetadata(long handle);

    private static native boolean nativeIsFinished(long handle);

    private static native void nativeDestroy(long handle);
}
