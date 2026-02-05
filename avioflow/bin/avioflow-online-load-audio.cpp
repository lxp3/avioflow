#include "avioflow-cxx-api.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <filesystem>

// Get format from file extension
std::string get_format_from_path(const std::string& path) {
    std::filesystem::path file_path(path);
    std::string ext = file_path.extension().string();
    
    // Remove leading dot
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Map extensions to FFmpeg format names
    if (ext == "aac" || ext == "m4a") return "aac";
    if (ext == "opus") return "opus";
    if (ext == "wav") return "wav";
    if (ext == "mp3") return "mp3";
    if (ext == "ogg") return "ogg";
    if (ext == "flac") return "flac";
    
    // Default to extension name
    return ext;
}

void test_online_decode(const std::string& path) {
    try {
        // Hardcode for zh.wav: 16kHz, Mono, s16le
        const int SAMPLE_RATE = 16000;
        const int CHANNELS = 1;
        const int BYTES_PER_SAMPLE = 2;  // s16le
        const int CHUNK_DURATION_MS = 100;
        const int WAV_HEADER_SIZE = 44;
        
        // Calculate 100ms chunk size: 16000 * 1 * 2 * 0.1 = 3200 bytes
        const size_t chunk_size = SAMPLE_RATE * CHANNELS * BYTES_PER_SAMPLE * CHUNK_DURATION_MS / 1000;
        
        std::cout << "Streaming with PCM format (s16le, " << SAMPLE_RATE << "Hz, " << CHANNELS << " channel)\n";
        std::cout << "Chunk size: " << chunk_size << " bytes (" << CHUNK_DURATION_MS << "ms per chunk)\n";
        
        // Read file into memory
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "Could not open file: " << path << "\n";
            return;
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        file.close();
        
        // Skip WAV header (44 bytes), only use raw PCM data
        if (size <= WAV_HEADER_SIZE) {
            std::cerr << "File too small to contain valid WAV header\n";
            return;
        }
        const uint8_t* pcm_data = buffer.data() + WAV_HEADER_SIZE;
        size_t pcm_size = size - WAV_HEADER_SIZE;
        
        std::cout << "File size: " << size << " bytes\n";
        std::cout << "PCM data size: " << pcm_size << " bytes (after skipping " << WAV_HEADER_SIZE << " byte header)\n";
        
        // Setup streaming options for raw PCM
        avioflow::AudioStreamOptions options;
        options.input_format = "s16le";           // Raw PCM format
        options.input_sample_rate = SAMPLE_RATE;  // Must specify for raw PCM
        options.input_channels = CHANNELS;        // Must specify for raw PCM
        
        avioflow::AudioDecoder decoder(options);
        
        // Push first chunk to initialize decoder
        if (pcm_size < chunk_size) {
            std::cerr << "PCM data too small for even one chunk\n";
            return;
        }
        decoder.push(pcm_data, chunk_size);
        
        // Now metadata should be available
        const auto& meta = decoder.get_metadata();
        std::cout << "\n--- Initial Metadata (after first push) ---\n";
        std::cout << "Sample Format: " << meta.sample_format << "\n";
        std::cout << "Channels: " << meta.num_channels << "\n";
        std::cout << "Sample Rate: " << meta.sample_rate << " Hz\n";
        
        // Continue pushing remaining data in 100ms chunks
        size_t offset = chunk_size;  // Start from second chunk
        int chunk_count = 1;  // First chunk already pushed
        size_t total_samples = 0;
        
        std::cout << "\n--- Starting chunked streaming ---\n";
        std::cout << "Chunk 1: Pushed " << chunk_size << " bytes (initialization)\n";
        
        while (offset < pcm_size) {
            size_t current_chunk_size = std::min(chunk_size, pcm_size - offset);
            chunk_count++;
            
            std::cout << "Chunk " << chunk_count << ": Pushing " << current_chunk_size << " bytes...";
            decoder.push(pcm_data + offset, current_chunk_size);
            offset += current_chunk_size;
            
            // Decode all available frames after this push
            int frames_in_chunk = 0;
            size_t samples_in_chunk = 0;
            while (true) {
                auto frame = decoder.read();
                if (!frame)
                    break;
                total_samples += frame.num_samples;
                samples_in_chunk += frame.num_samples;
                frames_in_chunk++;
            }
            
            std::cout << " Decoded " << frames_in_chunk << " frames, " 
                      << samples_in_chunk << " samples (Total: " << total_samples << ")\n";
        }
        
        // Flush decoder
        std::cout << "\nFlushing decoder...\n";
        int flush_frames = 0;
        size_t flush_samples = 0;
        while (!decoder.is_finished()) {
            auto frame = decoder.read();
            if (!frame)
                break;
            total_samples += frame.num_samples;
            flush_samples += frame.num_samples;
            flush_frames++;
        }
        if (flush_frames > 0) {
            std::cout << "Flushed " << flush_frames << " frames, " << flush_samples << " samples\n";
        }

        // Display finalized metadata
        const auto& final_meta = decoder.get_metadata();
        std::cout << "\n--- Finalized Metadata ---\n";
        std::cout << "Sample Format: " << final_meta.sample_format << "\n";
        std::cout << "Channels: " << final_meta.num_channels << "\n";
        std::cout << "Sample Rate: " << final_meta.sample_rate << " Hz\n";
        std::cout << "Total Samples: " << final_meta.num_samples << "\n";
        std::cout << "Duration: " << final_meta.duration << " s\n";
        
        std::cout << "\n>>> Total Samples Decoded: " << total_samples << "\n";
        std::cout << ">>> Total Chunks Pushed: " << chunk_count << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error decoding stream: " << e.what() << "\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: avioflow_online_load <audio_file_path>\n";
        std::cout << "Supported formats: aac, opus, wav, mp3, ogg, flac\n";
        std::cout << "Example: avioflow_online_load audio.aac\n";
        std::cout << "Note: Format is auto-detected from file extension\n";
        return 0;
    }

    std::string path = argv[1];
    
    std::cout << "--- Testing Online (Push-based) Decode ---\n";
    test_online_decode(path);

    return 0;
}
