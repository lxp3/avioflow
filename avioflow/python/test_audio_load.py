import sys
import os

# Add the binary directory to sys.path for the .pyd module
# Assuming running from the avioflow/python directory or project root
script_dir = os.path.dirname(os.path.abspath(__file__))
module_path = os.path.abspath(os.path.join(script_dir, "../../build/bin/Release"))

if os.path.exists(module_path):
    sys.path.append(module_path)
    if os.name == 'nt':
        # On Windows, we need to add the DLL path (where the .pyd and ffmpeg DLLs are)
        os.add_dll_directory(module_path)
else:
    # Try root build path if above fails
    module_path = os.path.abspath("build/bin/Release")
    if os.path.exists(module_path):
        sys.path.append(module_path)
        if os.name == 'nt':
            os.add_dll_directory(module_path)

try:
    import avioflow as av
except ImportError as e:
    print(f"Error: Could not import avioflow. Ensure it is built in {module_path}")
    print(f"Details: {e}")
    sys.exit(1)

def main():
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <audio_file_path>")
        sys.exit(1)

    audio_path = sys.argv[1]
    if not os.path.exists(audio_path):
        # Try absolute path or search relative to root
        root_path = os.path.abspath(os.path.join(script_dir, "../../", audio_path))
        if os.path.exists(root_path):
            audio_path = root_path
        else:
            print(f"Error: File not found: {audio_path}")
            sys.exit(1)

    print(f"--- Audio Load Test (Python) ---")
    print(f"Target: {audio_path}")

    try:
        # 1. Initialize Decoder
        decoder = av.AudioDecoder()
        
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
