"""Tests for offline audio loading with avioflow."""
import io
from pathlib import Path

import avioflow
import numpy as np
import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
MP3_PATH = REPO_ROOT / "public/wavs/TownTheme.mp3"
WAV_PATH = REPO_ROOT / "public/wavs/zh.wav"


def require_audio_file(path):
    if not path.exists():
        pytest.skip(f"audio fixture not found: {path}")


def get_sample_count(samples):
    """Get total sample count from numpy array."""
    if samples is None or samples.size == 0:
        return 0
    return samples.shape[1] if len(samples.shape) > 1 else samples.shape[0]


def get_test_buffers(file_path):
    with open(file_path, "rb") as f:
        raw = f.read()
    return {
        "bytes": raw,
        "bytearray": bytearray(raw),
        "memoryview": memoryview(raw),
        "bytesio": io.BytesIO(raw),
    }


def test_offline_filepath():
    """Test: Offline decode from file path."""
    print("\n=== Test: Offline Decode from Filepath ===")
    
    require_audio_file(MP3_PATH)
    decoder = avioflow.AudioDecoder()
    decoder.load_file(str(MP3_PATH))
    
    meta = decoder.get_metadata()
    print(f"  Codec: {meta.codec}, Duration: {meta.duration:.2f}s")
    
    samples = decoder.get_samples()
    total_samples = get_sample_count(samples)

    print(f"  Total samples decoded: {total_samples}")
    assert total_samples > 0, "No samples decoded"


def test_offline_memory():
    """Test: Offline decode from memory (full bytes)."""
    print("\n=== Test: Offline Decode from Memory ===")
    
    require_audio_file(MP3_PATH)
    
    # Read entire file into memory
    with open(MP3_PATH, "rb") as f:
        file_bytes = f.read()
    
    print(f"  File size: {len(file_bytes)} bytes")
    
    decoder = avioflow.AudioDecoder()
    decoder.load_buffer(file_bytes)
    
    meta = decoder.get_metadata()
    print(f"  Codec: {meta.codec}, Duration: {meta.duration:.2f}s")
    
    samples = decoder.get_samples()
    total_samples = get_sample_count(samples)

    print(f"  Total samples decoded: {total_samples}")
    assert total_samples > 0, "No samples decoded"


def test_load_from_bytes():
    """Test: Load function with bytes input."""
    print("\n=== Test: avioflow.load() with bytes ===")
    
    require_audio_file(WAV_PATH)
    
    # Read file as bytes
    with open(WAV_PATH, 'rb') as f:
        audio_bytes = f.read()
    
    print(f"  File size: {len(audio_bytes)} bytes")
    
    # Test 1: Load from bytes
    meta1, samples1 = avioflow.load(audio_bytes)
    print(f"  From bytes - Codec: {meta1.codec}, Duration: {meta1.duration:.2f}s")
    print(f"  Samples shape: {samples1.shape}, dtype: {samples1.dtype}")
    
    # Test 2: Load from filepath (for comparison)
    meta2, samples2 = avioflow.load(str(WAV_PATH))
    print(f"  From path - Codec: {meta2.codec}, Duration: {meta2.duration:.2f}s")
    
    # Verify both methods produce identical results
    assert samples1.shape == samples2.shape, "Shape mismatch between bytes and path"
    assert np.allclose(samples1, samples2), "Sample data mismatch between bytes and path"
    assert meta1.duration == meta2.duration, "Duration mismatch"
    assert meta1.sample_rate == meta2.sample_rate, "Sample rate mismatch"
    
    print("  PASS: Bytes and filepath produce identical results")


def test_info_with_buffer_inputs():
    """Test: avioflow.info() accepts bytes-like inputs."""
    print("\n=== Test: avioflow.info() with bytes-like inputs ===")

    require_audio_file(MP3_PATH)

    expected = avioflow.info(str(MP3_PATH))
    test_buffers = get_test_buffers(MP3_PATH)

    for name, source in test_buffers.items():
        meta = avioflow.info(source)
        print(f"  {name}: codec={meta.codec}, sample_rate={meta.sample_rate}, channels={meta.num_channels}")
        assert meta.codec == expected.codec, f"{name}: codec mismatch"
        assert meta.sample_rate == expected.sample_rate, f"{name}: sample rate mismatch"
        assert meta.num_channels == expected.num_channels, f"{name}: channel count mismatch"
        assert abs(meta.duration - expected.duration) < 1e-3, f"{name}: duration mismatch"


def test_decoder_load_buffer_with_inputs():
    """Test: AudioDecoder.load_buffer() accepts bytes-like inputs."""
    print("\n=== Test: AudioDecoder.load_buffer() with bytes-like inputs ===")

    require_audio_file(WAV_PATH)

    expected_meta, expected_samples = avioflow.load(str(WAV_PATH))

    for name, source in get_test_buffers(WAV_PATH).items():
        decoder = avioflow.AudioDecoder()
        meta = decoder.load_buffer(source)
        samples = decoder.get_samples()
        print(f"  {name}: shape={samples.shape}, codec={meta.codec}")
        assert samples.shape == expected_samples.shape, f"{name}: sample shape mismatch"
        assert np.allclose(samples, expected_samples), f"{name}: sample content mismatch"
        assert meta.sample_rate == expected_meta.sample_rate, f"{name}: sample rate mismatch"


def test_load_with_buffer_inputs():
    """Test: avioflow.load() accepts bytes-like inputs."""
    print("\n=== Test: avioflow.load() with bytes-like inputs ===")

    require_audio_file(WAV_PATH)

    expected_meta, expected_samples = avioflow.load(str(WAV_PATH))

    for name, source in get_test_buffers(WAV_PATH).items():
        meta, samples = avioflow.load(source)
        print(f"  {name}: shape={samples.shape}, codec={meta.codec}")
        assert samples.shape == expected_samples.shape, f"{name}: sample shape mismatch"
        assert np.allclose(samples, expected_samples), f"{name}: sample content mismatch"
        assert meta.sample_rate == expected_meta.sample_rate, f"{name}: sample rate mismatch"


def test_invalid_info_input_type():
    """Test: avioflow.info() rejects unsupported input types."""
    print("\n=== Test: avioflow.info() invalid input type ===")

    with pytest.raises(TypeError) as exc_info:
        avioflow.info(io.StringIO("not audio bytes"))
    message = str(exc_info.value)
    assert "bytes" in message and "BytesIO" in message


def test_load_with_resampling():
    """Test: Load function with bytes and resampling."""
    print("\n=== Test: avioflow.load() with bytes + resampling ===")
    
    require_audio_file(WAV_PATH)
    
    # Read file as bytes
    with open(WAV_PATH, 'rb') as f:
        audio_bytes = f.read()
    
    # Load with resampling to 16kHz, preserving all source channels
    meta, samples = avioflow.load(audio_bytes, output_sample_rate=16000)
    
    print(f"  Codec: {meta.codec}, Original SR: {meta.sample_rate}Hz")
    print(f"  Resampled shape: {samples.shape}")
    print(f"  Expected channels: {meta.num_channels}, Actual: {samples.shape[0]}")
    
    # Verify output format
    assert samples.shape[0] == meta.num_channels, f"Expected {meta.num_channels} channels, got {samples.shape[0]}"
    assert samples.dtype == np.float32, f"Expected float32, got {samples.dtype}"
    
    # Calculate expected sample count based on duration and target sample rate
    expected_samples = int(meta.duration * 16000)
    actual_samples = samples.shape[1]
    sample_diff = abs(expected_samples - actual_samples)
    
    print(f"  Expected ~{expected_samples} samples, got {actual_samples} (diff: {sample_diff})")
    assert sample_diff < 1000, f"Sample count mismatch too large: {sample_diff}"
    
    print("  PASS: Resampling works correctly with bytes input")


def test_load_with_passthrough_output_options():
    """Test: -1 preserves source sample rate and channel count."""
    print("\n=== Test: avioflow.load() with passthrough output options ===")

    require_audio_file(WAV_PATH)

    with open(WAV_PATH, "rb") as f:
        audio_bytes = f.read()

    meta_default, samples_default = avioflow.load(audio_bytes)
    meta_passthrough, samples_passthrough = avioflow.load(
        audio_bytes,
        output_sample_rate=-1,
        output_num_channels=-1,
    )

    print(f"  Default shape: {samples_default.shape}")
    print(f"  Passthrough shape: {samples_passthrough.shape}")

    assert meta_passthrough.sample_rate == meta_default.sample_rate, "-1 should preserve source sample rate"
    assert meta_passthrough.num_channels == meta_default.num_channels, "-1 should preserve source channel count"
    assert samples_passthrough.shape == samples_default.shape, "-1 should preserve output shape"
    assert np.allclose(samples_passthrough, samples_default), "-1 should match default decoding"


def test_info_metadata():
    """Test: avioflow.info() for fast metadata reading."""
    print("\n=== Test: avioflow.info() Metadata ===")

    require_audio_file(MP3_PATH)

    # Test info()
    meta = avioflow.info(str(MP3_PATH))
    print(f"  Codec: {meta.codec}")
    print(f"  Duration: {meta.duration:.2f}s")
    print(f"  Sample Rate: {meta.sample_rate}Hz")
    print(f"  Channels: {meta.num_channels}")

    # Validation
    assert meta.sample_rate > 0, "Invalid sample rate"
    assert meta.num_channels > 0, "Invalid channel count"
    assert meta.duration > 0, "Invalid duration"

    print("  PASS: info() successfully retrieved metadata")


def test_get_samples_offline():
    """Test: get_samples() in offline mode."""
    print("\n=== Test: get_samples() Offline ===")

    require_audio_file(MP3_PATH)

    decoder = avioflow.AudioDecoder()
    decoder.load_file(str(MP3_PATH))

    # Test new method name
    samples = decoder.get_samples()

    total_samples = get_sample_count(samples)
    print(f"  Total samples decoded: {total_samples}")

    assert total_samples > 0, "No samples decoded using get_samples()"
    print("  PASS: get_samples() works correctly for file input")


def test_time_range_across_whole_file():
    """Ranges late in the file are a regression guard.

    The trim bounds are absolute sample offsets, but a seek restarts the decoder's
    own sample counter, so comparing the two directly made every range past
    roughly half the duration come back short or empty.
    """
    print("\n=== Test: Time ranges across the whole file ===")

    require_audio_file(MP3_PATH)

    decoder = avioflow.AudioDecoder()
    decoder.load_file(str(MP3_PATH))
    rate = decoder.get_metadata().sample_rate

    for start in (0.0, 20.0, 50.0, 70.0, 90.0):
        samples = decoder.get_samples(start, start + 5.0)
        count = get_sample_count(samples)
        seconds = count / rate if rate else 0.0
        print(f"  [{start}, {start + 5.0}) -> {count} samples, {seconds:.3f}s")
        assert count > 0, f"range starting at {start}s returned nothing"
        assert abs(seconds - 5.0) < 0.01, f"range at {start}s gave {seconds:.3f}s, want 5.0s"


def test_open_ended_time_range():
    """An unset stop_seconds must decode to the end from any start point."""
    print("\n=== Test: Open-ended time ranges ===")

    require_audio_file(MP3_PATH)

    decoder = avioflow.AudioDecoder()
    decoder.load_file(str(MP3_PATH))
    rate = decoder.get_metadata().sample_rate

    # TownTheme.mp3 is ~97.45s.
    for start, expected in ((0.0, 97.45), (30.0, 67.45), (90.0, 7.45)):
        samples = decoder.get_samples(start)
        seconds = get_sample_count(samples) / rate if rate else 0.0
        print(f"  from {start}s -> {seconds:.3f}s (expected ~{expected}s)")
        assert seconds > 0, f"open-ended range from {start}s returned nothing"
        assert abs(seconds - expected) < 0.1, f"from {start}s: got {seconds:.3f}s, want ~{expected}s"
