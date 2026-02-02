import os
import sys
import time
from avioflow import AudioDecoder

def test_streaming():
    """
    测试流式解码：使用原始 PCM 格式，按 100ms 每帧推送。
    """
    file_path = "public/wavs/zh.wav"
    if not os.path.exists(file_path):
        print(f"Error: File not found at {file_path}")
        return

    # Hardcode for zh.wav: 16kHz, Mono, s16le
    SAMPLE_RATE = 16000
    CHANNELS = 1
    BYTES_PER_SAMPLE = 2  # s16le
    CHUNK_DURATION_MS = 100
    WAV_HEADER_SIZE = 44
    
    # Calculate 100ms chunk size: 16000 * 1 * 2 * 0.1 = 3200 bytes
    chunk_size = SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE * CHUNK_DURATION_MS // 1000
    
    print(f">>> Starting real-time streaming test for: {file_path}")
    print(f"Streaming with PCM format (s16le, {SAMPLE_RATE}Hz, {CHANNELS} channel)")
    print(f"Chunk size: {chunk_size} bytes ({CHUNK_DURATION_MS}ms per chunk)")
    
    # 1. 初始化解码器 - 使用原始 PCM 格式
    decoder = AudioDecoder(
        input_format="s16le",           # Raw PCM format
        input_sample_rate=SAMPLE_RATE,  # Must specify for raw PCM
        input_channels=CHANNELS          # Must specify for raw PCM
    )

    file_size = os.path.getsize(file_path)
    pcm_size = file_size - WAV_HEADER_SIZE
    
    print(f"File size: {file_size} bytes")
    print(f"PCM data size: {pcm_size} bytes (after skipping {WAV_HEADER_SIZE} byte header)")

    total_samples_decoded = 0
    chunk_count = 0
    start_time = time.time()

    print(f"\n--- Starting chunked streaming ---")

    try:
        with open(file_path, "rb") as f:
            # Skip WAV header (44 bytes), only read raw PCM data
            f.read(WAV_HEADER_SIZE)
            
            # Push first chunk to initialize decoder
            first_chunk = f.read(chunk_size)
            if len(first_chunk) < chunk_size:
                print("Error: PCM data too small for even one chunk")
                return
            
            decoder.push(first_chunk, len(first_chunk))
            chunk_count = 1
            
            # Now metadata should be available
            meta = decoder.get_metadata()
            print(f"\n--- Initial Metadata (after first push) ---")
            print(f"Sample Format: {meta.sample_format}")
            print(f"Channels: {meta.num_channels}")
            print(f"Sample Rate: {meta.sample_rate} Hz")
            
            print(f"\n--- Starting chunked streaming ---")
            print(f"Chunk 1: Pushed {len(first_chunk)} bytes (initialization)")
            
            # Continue reading and pushing remaining chunks
            while True:
                frame_data = f.read(chunk_size)
                if not frame_data:
                    break
                
                chunk_count += 1
                print(f"Chunk {chunk_count}: Pushing {len(frame_data)} bytes...", end="")
                
                # Core: stream decode call
                # This internally calls push() and then decodes all available frames
                samples = decoder(frame_data)
                
                if samples is not None and samples.size > 0:
                    # samples shape: (channels, num_samples)
                    decoded_samples = samples.shape[1] if samples.ndim > 1 else samples.shape[0]
                    total_samples_decoded += decoded_samples
                    print(f" Decoded {decoded_samples} samples (Total: {total_samples_decoded})")
                else:
                    print(f" No samples decoded (buffering...)")

        # Flush decoder
        print(f"\nFlushing decoder...")
        flush_count = 0
        while not decoder.is_finished():
            final_samples = decoder.decode_next()
            if final_samples is None or final_samples.size == 0:
                break
            decoded_samples = final_samples.shape[1] if final_samples.ndim > 1 else final_samples.shape[0]
            total_samples_decoded += decoded_samples
            flush_count += 1
        
        if flush_count > 0:
            print(f"Flushed {flush_count} frames")

        end_time = time.time()
        print(f"\n>>> Streaming finished in {end_time - start_time:.3f}s")

        # Print finalized metadata
        meta = decoder.get_metadata()
        print("\n--- Finalized Metadata ---")
        print(f"Sample Format: {meta.sample_format}")
        print(f"Channels: {meta.num_channels}")
        print(f"Sample Rate: {meta.sample_rate} Hz")
        print(f"Total Samples: {meta.num_samples}")
        print(f"Duration: {meta.duration:.3f} s")
        
        print(f"\n>>> Total Samples Decoded: {total_samples_decoded}")
        print(f">>> Total Chunks Pushed: {chunk_count}")
        
        # Verify
        if total_samples_decoded == meta.num_samples:
            print("✓ Sample count matches metadata!")
        else:
            print(f"✗ Sample count mismatch: decoded {total_samples_decoded}, expected {meta.num_samples}")

    except Exception as e:
        print(f"\nError during streaming: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    test_streaming()
