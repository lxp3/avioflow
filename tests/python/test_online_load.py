from avioflow import AudioDecoder
import numpy as np

def test_load():
    decoder = AudioDecoder(output_sample_rate=44100)
    # Test with a path (if you have one)
    # meta = decoder.load("test.mp3")
    # print(meta)
    # samples = decoder.get_all_samples()
    # print(f"Decoded {samples.shape} samples")

def test_streaming():
    decoder = AudioDecoder(input_sample_rate=44100, input_channels=2, input_format="s16le")
    # Simulate raw PCM data push
    data = bytes([0] * 1024)
    samples = decoder(data)
    if samples is not None:
        print(f"Decoded streaming buffer: {samples.shape}")

if __name__ == "__main__":
    print("Testing new AudioDecoder interface...")
    # These will fail until bindings are recompiled
    # test_load()
    # test_streaming()
