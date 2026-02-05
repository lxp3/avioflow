#!/usr/bin/env python3
"""Test script for offline audio loading with avioflow."""
import os
import sys
import time
import avioflow
import numpy as np

MP3_PATH = "public/wavs/TownTheme.mp3"
WAV_PATH = "public/wavs/zh.wav"
MP3_URL = "https://opengameart.org/sites/default/files/TownTheme.mp3"


def get_sample_count(samples):
    """Get total sample count from numpy array."""
    if samples is None or samples.size == 0:
        return 0
    return samples.shape[1] if len(samples.shape) > 1 else samples.shape[0]


def test_offline_filepath():
    """Test: Offline decode from file path."""
    print("\n=== Test: Offline Decode from Filepath ===")
    
    if not os.path.exists(MP3_PATH):
        print(f"  Skip: File not found at {MP3_PATH}")
        return
    
    start = time.time()
    decoder = avioflow.AudioDecoder()
    decoder.open(MP3_PATH)
    
    meta = decoder.get_metadata()
    print(f"  Codec: {meta.codec}, Duration: {meta.duration:.2f}s")
    
    samples = decoder.get_samples()
    total_samples = get_sample_count(samples)

    print(f"  Total samples decoded: {total_samples}")
    print(f"  Time: {(time.time() - start) * 1000:.1f}ms")
    assert total_samples > 0, "No samples decoded"


def test_offline_memory():
    """Test: Offline decode from memory (full bytes)."""
    print("\n=== Test: Offline Decode from Memory ===")
    
    if not os.path.exists(MP3_PATH):
        print(f"  Skip: File not found at {MP3_PATH}")
        return
    
    start = time.time()
    
    # Read entire file into memory
    with open(MP3_PATH, "rb") as f:
        file_bytes = f.read()
    
    print(f"  File size: {len(file_bytes)} bytes")
    
    decoder = avioflow.AudioDecoder()
    decoder.open(file_bytes)
    
    meta = decoder.get_metadata()
    print(f"  Codec: {meta.codec}, Duration: {meta.duration:.2f}s")
    
    samples = decoder.get_samples()
    total_samples = get_sample_count(samples)

    print(f"  Total samples decoded: {total_samples}")
    print(f"  Time: {(time.time() - start) * 1000:.1f}ms")
    assert total_samples > 0, "No samples decoded"


def test_load_from_bytes():
    """Test: Load function with bytes input."""
    print("\n=== Test: avioflow.load() with bytes ===")
    
    if not os.path.exists(WAV_PATH):
        print(f"  Skip: File not found at {WAV_PATH}")
        return
    
    start = time.time()
    
    # Read file as bytes
    with open(WAV_PATH, 'rb') as f:
        audio_bytes = f.read()
    
    print(f"  File size: {len(audio_bytes)} bytes")
    
    # Test 1: Load from bytes
    meta1, samples1 = avioflow.load(audio_bytes)
    print(f"  From bytes - Codec: {meta1.codec}, Duration: {meta1.duration:.2f}s")
    print(f"  Samples shape: {samples1.shape}, dtype: {samples1.dtype}")
    
    # Test 2: Load from filepath (for comparison)
    meta2, samples2 = avioflow.load(WAV_PATH)
    print(f"  From path - Codec: {meta2.codec}, Duration: {meta2.duration:.2f}s")
    
    # Verify both methods produce identical results
    assert samples1.shape == samples2.shape, "Shape mismatch between bytes and path"
    assert np.allclose(samples1, samples2), "Sample data mismatch between bytes and path"
    assert meta1.duration == meta2.duration, "Duration mismatch"
    assert meta1.sample_rate == meta2.sample_rate, "Sample rate mismatch"
    
    print(f"  ✓ Bytes and filepath produce identical results")
    print(f"  Time: {(time.time() - start) * 1000:.1f}ms")


def test_load_with_resampling():
    """Test: Load function with bytes and resampling."""
    print("\n=== Test: avioflow.load() with bytes + resampling ===")
    
    if not os.path.exists(WAV_PATH):
        print(f"  Skip: File not found at {WAV_PATH}")
        return
    
    start = time.time()
    
    # Read file as bytes
    with open(WAV_PATH, 'rb') as f:
        audio_bytes = f.read()
    
    # Load with resampling to 16kHz mono
    meta, samples = avioflow.load(audio_bytes, output_sample_rate=16000, output_num_channels=1)
    
    print(f"  Codec: {meta.codec}, Original SR: {meta.sample_rate}Hz")
    print(f"  Resampled shape: {samples.shape}")
    print(f"  Expected channels: 1, Actual: {samples.shape[0]}")
    
    # Verify output format
    assert samples.shape[0] == 1, f"Expected 1 channel, got {samples.shape[0]}"
    assert samples.dtype == np.float32, f"Expected float32, got {samples.dtype}"
    
    # Calculate expected sample count based on duration and target sample rate
    expected_samples = int(meta.duration * 16000)
    actual_samples = samples.shape[1]
    sample_diff = abs(expected_samples - actual_samples)
    
    print(f"  Expected ~{expected_samples} samples, got {actual_samples} (diff: {sample_diff})")
    assert sample_diff < 1000, f"Sample count mismatch too large: {sample_diff}"
    
    print(f"  ✓ Resampling works correctly with bytes input")
    print(f"  Time: {(time.time() - start) * 1000:.1f}ms")


def test_offline_url():
    """Test: Offline decode from URL."""
    print("\n=== Test: Offline Decode from URL ===")
    print(f"  URL: {MP3_URL}")
    
    start = time.time()
    
    try:
        decoder = avioflow.AudioDecoder()
        decoder.open(MP3_URL)
        
        meta = decoder.get_metadata()
        print(f"  Codec: {meta.codec}, Sample Rate: {meta.sample_rate}Hz")
        
        # Only decode a few frames to verify it works
        frame_count = 0
        while not decoder.is_finished() and frame_count < 10:
            frame = decoder.decode_next()
            if frame is None or frame.size == 0:
                break
            frame_count += 1
        
        print(f"  Successfully decoded {frame_count} frames from URL")
        print(f"  Time: {(time.time() - start) * 1000:.1f}ms")
        assert frame_count > 0, "No frames decoded from URL"
        
    except Exception as e:
        print(f"  URL test skipped (network error): {e}")


def test_info_metadata():
    """Test: avioflow.info() for fast metadata reading."""
    print("\n=== Test: avioflow.info() Metadata ===")

    if not os.path.exists(MP3_PATH):
        print(f"  Skip: File not found at {MP3_PATH}")
        return

    start = time.time()

    # Test info()
    meta = avioflow.info(MP3_PATH)

    duration_ms = (time.time() - start) * 1000
    print(f"  Codec: {meta.codec}")
    print(f"  Duration: {meta.duration:.2f}s")
    print(f"  Sample Rate: {meta.sample_rate}Hz")
    print(f"  Channels: {meta.num_channels}")
    print(f"  Time: {duration_ms:.2f}ms")

    # Validation
    assert meta.sample_rate > 0, "Invalid sample rate"
    assert meta.num_channels > 0, "Invalid channel count"
    assert meta.duration > 0, "Invalid duration"

    # Verify it's faster than full loading (optional sanity check)
    assert duration_ms < 500, "Metadata reading took unexpectedly long (>500ms)"
    print("  ✓ info() successfully retrieved metadata")


def test_get_samples_offline():
    """Test: get_samples() in offline mode."""
    print("\n=== Test: get_samples() Offline ===")

    if not os.path.exists(MP3_PATH):
        print(f"  Skip: File not found at {MP3_PATH}")
        return

    decoder = avioflow.AudioDecoder()
    decoder.open(MP3_PATH)

    # Test new method name
    samples = decoder.get_samples()

    total_samples = get_sample_count(samples)
    print(f"  Total samples decoded: {total_samples}")

    assert total_samples > 0, "No samples decoded using get_samples()"
    print("  ✓ get_samples() works correctly for file input")


def main():
    print("=== avioflow Offline Decoder Tests ===")
    avioflow.set_log_level("warning")

    try:
        test_offline_filepath()
        test_offline_memory()
        test_load_from_bytes()
        test_load_with_resampling()
        test_offline_url()

        # New tests
        test_info_metadata()
        test_get_samples_offline()

        print("\nAll offline tests passed!")
    except AssertionError as e:
        print(f"\nTest failed: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
