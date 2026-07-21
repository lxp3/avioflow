package io.github.lxp3.avioflow;

import java.util.Objects;

public final class Avioflow {
    private Avioflow() {
    }

    public static void setLogLevel(String level) {
        NativeLibraryLoader.load();
        nativeSetLogLevel(Objects.requireNonNull(level, "level"));
    }

    public static String[] getSupportedDecoders() {
        NativeLibraryLoader.load();
        return nativeGetSupportedDecoders();
    }

    public static String[] getSupportedEncoders() {
        NativeLibraryLoader.load();
        return nativeGetSupportedEncoders();
    }

    public static String[] getSupportedInputFormats() {
        NativeLibraryLoader.load();
        return nativeGetSupportedInputFormats();
    }

    public static String[] getSupportedOutputFormats() {
        NativeLibraryLoader.load();
        return nativeGetSupportedOutputFormats();
    }

    public static DeviceInfo[] listAudioDevices() {
        NativeLibraryLoader.load();
        return nativeListAudioDevices();
    }

    static AudioStreamOptions streamOptionsOrDefault(AudioStreamOptions options) {
        return options == null ? new AudioStreamOptions() : options;
    }

    static AudioWriteOptions writeOptionsOrDefault(AudioWriteOptions options) {
        return options == null ? new AudioWriteOptions() : options;
    }

    private static native void nativeSetLogLevel(String level);

    private static native String[] nativeGetSupportedDecoders();

    private static native String[] nativeGetSupportedEncoders();

    private static native String[] nativeGetSupportedInputFormats();

    private static native String[] nativeGetSupportedOutputFormats();

    private static native DeviceInfo[] nativeListAudioDevices();
}
