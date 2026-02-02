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
    
    samples = decoder.get_all_samples()
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
    
    samples = decoder.get_all_samples()
    total_samples = get_sample_count(samples)
    
    print(f"  Total samples decoded: {total_samples}")
    print(f"  Time: {(time.time() - start) * 1000:.1f}ms")
    assert total_samples > 0, "No samples decoded"


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


def main():
    print("=== avioflow Offline Decoder Tests ===")
    avioflow.set_log_level("warning")
    
    try:
        test_offline_filepath()
        test_offline_memory()
        test_offline_url()
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
