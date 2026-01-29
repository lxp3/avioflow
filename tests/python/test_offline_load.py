#! /usr/bin/env python3
"""Test script for offline audio loading with avioflow."""
import sys
import os
import avioflow

print(f"Imported avioflow from: {avioflow.__file__}")

avioflow.set_log_level("info")

def main():
    # Use path from argument if provided, otherwise use default
    if len(sys.argv) > 1:
        audio_path = sys.argv[1]
    else:
        # Fallback for local testing
        audio_path = os.path.join(os.path.dirname(__file__), "../../public/TownTheme.mp3")
    
    print(f"Testing with audio file: {audio_path}")
    
    if not os.path.exists(audio_path):
        print(f"Error: Audio file not found at {audio_path}")
        sys.exit(1)

    try:
        # 1. Initialize Decoder
        decoder = avioflow.AudioDecoder()

        # 2. Load File (new API: load() instead of open())
        meta = decoder.load(audio_path)

        # 3. Print Metadata
        print(f"\nMetadata Recognized:")
        print(f"  Container:    {meta.container}")
        print(f"  Codec:        {meta.codec}")
        print(f"  Sample Rate:  {meta.sample_rate} Hz")
        print(f"  Channels:     {meta.num_channels}")
        print(f"  Duration:     {meta.duration:.3f} s")
        print(f"  Num Samples:  {meta.num_samples}")

        # 4. Decode all samples (returns numpy array with shape (channels, samples))
        print(f"\nDecoding all samples...")
        samples = decoder.get_all_samples()

        print(f"Decoding Success!")
        print(f"  Shape: {samples.shape}")  # (channels, samples)
        print(f"  Channels: {samples.shape[0]}")
        print(f"  Samples per channel: {samples.shape[1]}")
        print(f"  dtype: {samples.dtype}")

        # Verify sample count matches metadata
        if samples.shape[1] == meta.num_samples:
            print(f"  ✓ Sample count matches metadata")
        else:
            print(f"  ⚠ Sample count mismatch: {samples.shape[1]} vs {meta.num_samples}")

        print("\nSUCCESS: Python audio loading is working correctly.")

    except Exception as e:
        print(f"An error occurred during decoding: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
