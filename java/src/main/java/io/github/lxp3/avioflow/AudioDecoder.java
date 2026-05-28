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

    public void open(String path) {
        ensureOpen();
        nativeOpenPath(handle, Objects.requireNonNull(path, "path"));
    }

    public void open(byte[] data) {
        ensureOpen();
        nativeOpenBytes(handle, Objects.requireNonNull(data, "data"));
    }

    public float[][] getSamples() {
        ensureOpen();
        return nativeGetSamples(handle);
    }

    public Metadata getMetadata() {
        ensureOpen();
        return nativeGetMetadata(handle);
    }

    public boolean isFinished() {
        ensureOpen();
        return nativeIsFinished(handle);
    }

    @Override
    public void close() {
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

    private static native void nativeOpenPath(long handle, String path);

    private static native void nativeOpenBytes(long handle, byte[] data);

    private static native float[][] nativeGetSamples(long handle);

    private static native Metadata nativeGetMetadata(long handle);

    private static native boolean nativeIsFinished(long handle);

    private static native void nativeDestroy(long handle);
}
