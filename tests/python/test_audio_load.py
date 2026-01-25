#! /usr/bin/env python3
import sys

import avioflow

"""
Usage:
cd tests/python
python test_audio_load.py
"""

avioflow.set_log_level("info")


def main():
    audio_path = "../../public/wavs/TownTheme.mp3"
    try:
        # 1. Initialize Decoder
        decoder = avioflow.AudioDecoder()

        # 2. Open File
        decoder.open(audio_path)

        # 3. Get Metadata
        meta = decoder.get_metadata()
        print(f"\nMetadata Recognized:")
        print(f"  Container:    {meta.container}")
        print(f"  Codec:        {meta.codec}")
        print(f"  Sample Rate:  {meta.sample_rate} Hz")
        print(f"  Channels:     {meta.num_channels}")
        print(f"  Duration:     {meta.duration:.3f} s")
        print(f"  Num Samples:  {meta.num_samples}")

        # 4. Decode all samples
        print(f"\nDecoding all samples...")
        samples = decoder.get_all_samples()

        print(f"Decoding Success!")
        print(f"  Channels: {len(samples.data)}")
        if len(samples.data) > 0:
            print(f"  Samples per channel: {len(samples.data[0])}")

        print("\nSUCCESS: Python audio loading is working correctly.")

    except Exception as e:
        print(f"An error occurred during decoding: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
