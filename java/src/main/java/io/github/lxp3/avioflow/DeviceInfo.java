package io.github.lxp3.avioflow;

public final class DeviceInfo {
    public final String name;
    public final String description;
    public final boolean output;

    DeviceInfo(String name, String description, boolean output) {
        this.name = name;
        this.description = description;
        this.output = output;
    }
}
