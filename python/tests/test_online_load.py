#!/usr/bin/env python3
"""Test script for online (streaming) audio decoding with avioflow."""
import io
import os
import sys
import time
import avioflow
import numpy as np

MP3_PATH = "public/wavs/TownTheme.mp3"
WAV_PATH = "public/wavs/zh.wav"


def get_sample_count(samples):
    """Get total sample count from numpy array."""
    if samples is None or samples.size == 0:
        return 0
    return samples.shape[1] if len(samples.shape) > 1 else samples.shape[0]


def decode_available_samples(decoder):
    total_decoded = 0
    while True:
        frame = decoder.read()
        if frame is None:
            break
        total_decoded += get_sample_count(frame)
    return total_decoded


def simulate_streaming(test_name: str, file_path: str, format: str, 
                       sample_rate: int = 0, channels: int = 0):
    """
    Simulate streaming by pushing data in 100ms chunks.
    
    Args:
        test_name: Name of the test for logging
        file_path: Path to the audio file
        format: Input format (e.g., "mp3", "wav", "s16le")
        sample_rate: Sample rate (required for raw PCM)
        channels: Number of channels (required for raw PCM)
    """
    print(f"\n=== Running {test_name} (100ms chunks) ===")
    
    if not os.path.exists(file_path):
        print(f"  Skip: File not found at {file_path}")
        return
    
    with open(file_path, "rb") as f:
        buffer = f.read()
    
    print(f"  File size: {len(buffer)} bytes")
    
    # Initialize decoder with streaming options
    decoder = avioflow.AudioDecoder(
        input_format=format,
        input_sample_rate=sample_rate if sample_rate > 0 else None,
        input_channels=channels if channels > 0 else None
    )
    
    # Calculate chunk size for 100ms
    if format in ("s16le", "wav"):
        sr = sample_rate if sample_rate > 0 else 16000
        ch = channels if channels > 0 else 1
        chunk_size = int(sr * ch * 2 * 0.1)  # 16-bit = 2 bytes
    else:
        chunk_size = 4096  # ~100ms for typical MP3 bitrate
    
    offset = 0
    total_decoded = 0
    push_count = 0
    start_time = time.time()
    
    # Push all data in chunks, decoding after each push
    while offset < len(buffer):
        to_push = min(chunk_size, len(buffer) - offset)
        decoder.push(buffer[offset:offset + to_push])
        offset += to_push
        push_count += 1
        
        # Try to decode available frames
        total_decoded += decode_available_samples(decoder)
    
    # Continue decoding until finished
    while not decoder.is_finished():
        decoded = decode_available_samples(decoder)
        if decoded == 0:
            break
        total_decoded += decoded
    
    elapsed = (time.time() - start_time) * 1000
    print(f"  Push count: {push_count}, Total samples: {total_decoded}")
    print(f"  Time: {elapsed:.1f}ms")
    
    assert total_decoded > 0, f"{test_name} failed: No samples decoded"


def test_online_mp3():
    """Test: Online MP3 streaming (TownTheme.mp3 is 44100Hz, 2ch)."""
    simulate_streaming("Online MP3 Test", MP3_PATH, "mp3", 44100, 2)


def test_online_wav():
    """Test: Online WAV streaming (zh.wav is 16000Hz, 1ch)."""
    simulate_streaming("Online WAV Test", WAV_PATH, "wav", 16000, 1)


def test_online_pcm_fallback():
    """
    Test: PCM fallback when WAV format is specified but input is raw PCM.
    This simulates the behavior of ASR services like Alibaba/Tencent.
    """
    print("\n=== Running Online PCM Fallback Test ===")
    
    if not os.path.exists(WAV_PATH):
        print(f"  Skip: File not found at {WAV_PATH}")
        return
    
    with open(WAV_PATH, "rb") as f:
        buffer = f.read()
    
    # Strip 44 bytes WAV header to simulate raw PCM
    if len(buffer) > 44:
        buffer = buffer[44:]
    
    print(f"  PCM data size: {len(buffer)} bytes (WAV header stripped)")
    
    # Use "wav" format, but input is actually raw PCM
    # Decoder should automatically fallback to s16le
    decoder = avioflow.AudioDecoder(
        input_format="wav",  # Intentional: should fallback to s16le
        input_sample_rate=16000,
        input_channels=1
    )
    
    # Push in 100ms chunks
    chunk_size = int(16000 * 1 * 2 * 0.1)
    offset = 0
    total_decoded = 0
    start_time = time.time()
    
    while offset < len(buffer):
        to_push = min(chunk_size, len(buffer) - offset)
        decoder.push(buffer[offset:offset + to_push])
        offset += to_push
        
        total_decoded += decode_available_samples(decoder)
    
    elapsed = (time.time() - start_time) * 1000
    print(f"  Total samples: {total_decoded}")
    print(f"  Time: {elapsed:.1f}ms")
    
    assert total_decoded > 0, "PCM fallback test failed: No samples decoded"


def test_push_with_buffer_inputs():
    """Test: push/__call__ accept bytes-like inputs."""
    print("\n=== Running Streaming Buffer Input Test ===")

    if not os.path.exists(MP3_PATH):
        print(f"  Skip: File not found at {MP3_PATH}")
        return

    with open(MP3_PATH, "rb") as f:
        raw = f.read()

    chunks = [
        ("bytearray", bytearray(raw[:12288])),
        ("memoryview", memoryview(raw[12288:24576])),
        ("bytesio", io.BytesIO(raw[24576:36864])),
    ]

    decoder = avioflow.AudioDecoder(input_format="mp3", input_sample_rate=44100, input_channels=2)

    for name, chunk in chunks:
        decoder.push(chunk)
        print(f"  pushed {name}: {len(bytes(chunk.getbuffer())) if hasattr(chunk, 'getbuffer') else len(chunk)} bytes")

    samples = decoder(raw[36864:40960])
    total_decoded = get_sample_count(samples) + decode_available_samples(decoder)
    print(f"  Total decoded samples after mixed inputs: {total_decoded}")
    assert total_decoded > 0, "Mixed buffer inputs produced no decoded samples"


def main():
    print("=== avioflow Online (Streaming) Decoder Tests ===")
    avioflow.set_log_level("warning")
    
    try:
        test_online_mp3()
        test_online_wav()
        test_online_pcm_fallback()
        test_push_with_buffer_inputs()
        print("\nAll online tests passed!")
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
