#include "avioflow-cxx-api.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <filesystem>

struct WavPcmInfo {
    int sample_rate = 0;
    int channels = 0;
    int bits_per_sample = 0;
    size_t data_offset = 0;
    size_t data_size = 0;
};

uint16_t read_le16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           static_cast<uint16_t>(data[1] << 8);
}

uint32_t read_le32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

WavPcmInfo parse_wav_pcm_info(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 44 ||
        std::memcmp(buffer.data(), "RIFF", 4) != 0 ||
        std::memcmp(buffer.data() + 8, "WAVE", 4) != 0) {
        throw std::runtime_error("Expected a WAV file");
    }

    WavPcmInfo info;
    size_t offset = 12;
    bool found_fmt = false;
    bool found_data = false;

    while (offset + 8 <= buffer.size()) {
        const uint8_t* chunk = buffer.data() + offset;
        const uint32_t chunk_size = read_le32(chunk + 4);
        const size_t payload_offset = offset + 8;
        if (payload_offset + chunk_size > buffer.size()) {
            throw std::runtime_error("Invalid WAV chunk size");
        }

        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (chunk_size < 16) {
                throw std::runtime_error("Invalid WAV fmt chunk");
            }
            const uint16_t audio_format = read_le16(buffer.data() + payload_offset);
            if (audio_format != 1) {
                throw std::runtime_error("Only PCM WAV input is supported by this example");
            }
            info.channels = read_le16(buffer.data() + payload_offset + 2);
            info.sample_rate = static_cast<int>(read_le32(buffer.data() + payload_offset + 4));
            info.bits_per_sample = read_le16(buffer.data() + payload_offset + 14);
            found_fmt = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            info.data_offset = payload_offset;
            info.data_size = chunk_size;
            found_data = true;
        }

        offset = payload_offset + chunk_size + (chunk_size % 2);
    }

    if (!found_fmt || !found_data || info.sample_rate <= 0 ||
        info.channels <= 0 || info.bits_per_sample != 16) {
        throw std::runtime_error("Expected 16-bit PCM WAV input");
    }
    return info;
}

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
        const int CHUNK_DURATION_MS = 100;
        
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

        const WavPcmInfo wav_info = parse_wav_pcm_info(buffer);
        const int bytes_per_sample = wav_info.bits_per_sample / 8;
        const size_t chunk_size =
            wav_info.sample_rate * wav_info.channels * bytes_per_sample * CHUNK_DURATION_MS / 1000;
        
        const uint8_t* pcm_data = buffer.data() + wav_info.data_offset;
        size_t pcm_size = wav_info.data_size;
        
        std::cout << "Streaming with PCM format (s16le, " << wav_info.sample_rate
                  << "Hz, " << wav_info.channels << " channel)\n";
        std::cout << "Chunk size: " << chunk_size << " bytes (" << CHUNK_DURATION_MS << "ms per chunk)\n";
        std::cout << "File size: " << size << " bytes\n";
        std::cout << "PCM data size: " << pcm_size << " bytes (after WAV header)\n";
        
        // Setup streaming options for raw PCM
        avioflow::AudioStreamOptions options;
        options.input_format = "s16le";           // Raw PCM format
        options.input_sample_rate = wav_info.sample_rate;  // Must specify for raw PCM
        options.input_channels = wav_info.channels;        // Must specify for raw PCM
        
        avioflow::AudioDecoder decoder(options);
        
        if (chunk_size == 0) {
            std::cerr << "Invalid chunk size\n";
            return;
        }
        
        size_t offset = 0;
        int chunk_count = 0;
        size_t total_samples = 0;
        
        std::cout << "\n--- Starting chunked streaming ---\n";
        
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
        decoder.finish();
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
