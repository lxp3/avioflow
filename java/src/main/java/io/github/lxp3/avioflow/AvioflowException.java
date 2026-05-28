package io.github.lxp3.avioflow;

public class AvioflowException extends RuntimeException {
    public AvioflowException(String message) {
        super(message);
    }

    public AvioflowException(String message, Throwable cause) {
        super(message, cause);
    }
}

