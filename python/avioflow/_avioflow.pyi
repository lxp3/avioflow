from typing import Optional, Union, Tuple, List, overload
import numpy as np
import os
import io

ReadableBuffer = Union[bytes, bytearray, memoryview, io.BytesIO]
PathInput = Union[str, os.PathLike]
SourceInput = Union[str, os.PathLike, ReadableBuffer]

def set_log_level(level: str = "error") -> None:
    """
    Set FFmpeg logging verbosity level.

    Args:
        level (str): Log level, one of:
            - "quiet": No output
            - "fatal": Only fatal errors
            - "error": All errors
            - "warning": Errors and warnings
            - "info": General information
            - "error": Default; errors only
            - "debug": Detailed debugging info
            - "trace": Maximum verbosity
    """
    ...

def get_supported_decoders() -> List[str]:
    """
    Get the list of supported audio decoder names.

    Returns:
        list[str]: Decoder names such as "mp3", "aac", and "pcm_s16le".
    """
    ...

def get_supported_encoders() -> List[str]:
    """
    Get the list of supported audio encoder names.

    Returns:
        list[str]: Encoder names such as "pcm_s16le", "aac", and "libmp3lame".
    """
    ...

def get_supported_input_formats() -> List[str]:
    """
    Get the list of supported input format (demuxer) names.

    Returns:
        list[str]: Input format names such as "mp3", "wav", "flac", and "s16le".
    """
    ...

def info(source: SourceInput) -> "Metadata":
    """
    Get audio metadata without decoding samples.

    This function opens the audio source and parses its header information
    without performing any audio decoding, making it very fast for simply
    inspecting file properties.

    Args:
        source (str, PathLike, bytes-like, or BytesIO): Path/URL or encoded audio bytes.

    Returns:
        Metadata: Audio stream metadata (duration, sample_rate, channels, etc.)

    Example:
        >>> meta = avioflow.info("large_audio.wav")
        >>> print(f"{meta.codec} | {meta.sample_rate}Hz | {meta.duration}s")
    """
    ...

def load(
    source: SourceInput,
    output_sample_rate: Optional[int] = None
) -> Tuple["Metadata", np.ndarray]:
    """
    Load an audio file and decode all samples in one call.

    This is a convenience function that combines file loading and full
    decoding in a single call. For large files or when you need frame-by-frame
    control, use AudioDecoder directly.

    Args:
        source (str, PathLike, bytes-like, or BytesIO): Path/URL or encoded audio bytes.
        output_sample_rate (int, optional): Target sample rate in Hz.
            If None or negative, uses source sample rate.

    Returns:
        tuple[Metadata, numpy.ndarray]: A tuple containing:
            - Metadata: Audio stream information
            - samples: Float32 array with shape (channels, samples)

    Example:
        >>> import avioflow
        >>> meta, samples = avioflow.load("audio.mp3")
        >>> print(f"Duration: {meta.duration}s, Shape: {samples.shape}")

        >>> # With resampling to 16kHz
        >>> meta, samples = avioflow.load("speech.wav", output_sample_rate=16000)
    """
    ...

class DeviceInfo:
    """Container for system audio device information."""
    name: str
    """str: Unique device identifier used for opening."""

    description: str
    """str: Human-readable device name."""

    is_output: bool
    """bool: True if this is an output/loopback device."""

class Metadata:
    """Container for audio stream metadata."""
    duration: float
    """float: Duration in seconds. May be 0 for live streams."""

    num_samples: int
    """int: Total number of samples. Updated at EOF for streams."""

    sample_rate: int
    """int: Sample rate in Hz (e.g., 44100, 48000)."""

    num_channels: int
    """int: Number of audio channels (1=mono, 2=stereo)."""

    sample_format: str
    """str: Original sample format (e.g., 'fltp', 's16')."""

    codec: str
    """str: Codec name (e.g., 'mp3', 'aac', 'flac')."""

    bit_rate: int
    """int: Bit rate in bits per second."""

    container: str
    """str: Container format (e.g., 'mp3', 'mp4', 'ogg')."""

class AudioDecoder:
    """
    High-performance audio decoder with file and streaming support.

    Two operation modes:

    **File Mode** - Load complete audio files:
        >>> decoder = AudioDecoder(output_sample_rate=44100)
        >>> meta = decoder.load_file("audio.mp3")
        >>> samples = decoder.get_samples()  # numpy array (channels, samples)

    **Stream Mode** - Decode real-time byte streams:
        >>> decoder = AudioDecoder(input_format="s16le", input_sample_rate=48000, input_channels=2)
        >>> decoder.feed(raw_bytes)
            >>> samples = decoder.get_samples()

    Attributes:
        All audio data is returned as float32 in range [-1.0, 1.0].
    """

    def __init__(
        self,
        output_sample_rate: Optional[int] = None,
        output_num_channels: Optional[int] = None,
        input_sample_rate: Optional[int] = None,
        input_channels: Optional[int] = None,
        input_format: Optional[str] = None
    ) -> None:
        """
        Initialize the AudioDecoder.

        Args:
            output_sample_rate (int, optional): Target output sample rate in Hz.
                If not specified, uses source sample rate. Must be positive.
            output_num_channels (int, optional): Target output channel count.
                If not specified, uses the source channel count. Must be positive.
            input_sample_rate (int, optional): Source sample rate for raw PCM streaming.
                Required for stream mode with raw PCM formats.
            input_channels (int, optional): Source channel count for raw PCM streaming.
                Required for stream mode with raw PCM formats.
            input_format (str, optional): Source format for streaming. Options:
                - "s16le": 16-bit signed little-endian PCM
                - "f32le": 32-bit float little-endian PCM
                - "aac": AAC audio (with ADTS headers)
                - "opus": Opus audio
                Required for stream mode.
        """
        ...

    def load_file(self, source: PathInput) -> Metadata:
        """
        Load audio from file path, URL, or device.

        Args:
            source: Audio source, one of:
                - str: File path or URL
                - pathlib.Path: File path object
                - "wasapi_loopback": Windows system audio capture
                - "audio=DeviceName": Microphone/input device

        Returns:
            Metadata: Audio stream metadata object.

        Raises:
            RuntimeError: If source cannot be opened or decoded.
            TypeError: If source type is not supported.

        Example:
            >>> decoder = AudioDecoder()
            >>> meta = decoder.load_file("song.mp3")
            >>> print(f"Duration: {meta.duration}s, Sample rate: {meta.sample_rate}Hz")
        """
        ...

    def load_buffer(self, source: ReadableBuffer) -> Metadata:
        """Load complete encoded audio bytes and return metadata."""
        ...

    def get_samples(self) -> np.ndarray:
        """
        Decode all currently available samples.

        In File Mode: Decodes from current position to end of stream.
        In Stream Mode: Decodes all buffered data until more input is required.

        Returns:
            numpy.ndarray: Audio samples with shape (channels, samples).
                dtype is float32, values in range [-1.0, 1.0].

        Note:
            For large files, consider using frame-by-frame decoding (get_frame())
            to manage memory usage.

        Example:
            >>> decoder = AudioDecoder(output_sample_rate=16000)
            >>> decoder.load_file("speech.wav")
            >>> samples = decoder.get_samples()
            >>> print(f"Shape: {samples.shape}")  # e.g., (1, 160000) for 10s mono
        """
        ...

    def feed(self, data: ReadableBuffer) -> None:
        """
        Push raw audio data bytes to the decoder (stream mode only).

        Args:
            data (bytes-like or BytesIO): Raw audio data bytes

        Note:
            First feed starts stream mode. Use get_frame() or get_samples()
            to retrieve decoded output.

        Example:
            >>> decoder = AudioDecoder(input_format="s16le", input_sample_rate=16000, input_channels=1)
            >>> with open("audio.raw", "rb") as f:
            ...     chunk = f.read(3200)  # 100ms @ 16kHz mono
            ...     decoder.feed(chunk)
            ...     samples = decoder.get_samples()
        """
        ...

    def flush(self) -> None:
        """Mark stream input as complete and allow delayed frames to drain."""
        ...

    def get_frame(self) -> Optional[np.ndarray]:
        """
        Decode next audio frame.

        Returns:
            numpy.ndarray or None: Decoded audio samples with shape (channels, samples),
                or None if no frame is available (need more data or EOF).

        Example:
            >>> while not decoder.is_finished():
            ...     frame = decoder.get_frame()
            ...     if frame is not None:
            ...         process(frame)
        """
        ...

    def is_finished(self) -> bool:
        """
        Check if end of stream has been reached.

        Returns:
            bool: True if all audio data has been decoded.
        """
        ...

    def get_metadata(self) -> Metadata:
        """
        Get current audio stream metadata.

        Returns:
            Metadata: Audio stream metadata object.

        Note:
            For stream mode, metadata is only available after first feed().
        """
        ...

class DeviceManager:
    """Utility class for discovering system audio devices."""

    @staticmethod
    def list_audio_devices() -> List[DeviceInfo]:
        """
        List available system audio devices.

        Returns:
            list[DeviceInfo]: List of available audio input/output devices.

        Example:
            >>> devices = avioflow.DeviceManager.list_audio_devices()
            >>> for dev in devices:
            ...     print(f"{dev.name}: {dev.description}")
        """
        ...
