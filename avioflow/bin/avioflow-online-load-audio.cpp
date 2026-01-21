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

class ChunkedReader {
public:
    ChunkedReader(const std::string& path, size_t chunk_size)
        : file_(path, std::ios::binary), chunk_size_(chunk_size) {}

    int read(uint8_t* buf, int buf_size) {
        if (!file_.is_open() || file_.eof()) {
            return 0; // EOF
        }

        // Simulating network delay or block reading
        int to_read = std::min(static_cast<int>(chunk_size_), buf_size);
        file_.read(reinterpret_cast<char*>(buf), to_read);
        return static_cast<int>(file_.gcount());
    }

private:
    std::ifstream file_;
    size_t chunk_size_;
};

void test_online_decode(const std::string& path) {
    try {
        // Get format from file extension
        std::string format = get_format_from_path(path);
        std::cout << "Detected format: " << format << "\n";
        
        // Read in 4KB chunks
        ChunkedReader reader(path, 4096);
        
        // Setup streaming options with explicit format
        avioflow::AudioStreamOptions options;
        options.input_format = format;
        
        avioflow::AudioDecoder decoder;
        decoder.open_stream([&reader](uint8_t* buf, int size) {
            return reader.read(buf, size);
        }, options);

        const auto& meta = decoder.get_metadata();
        std::cout << "Successfully opened stream: " << path << "\n";
        std::cout << "Container: " << meta.container << "\n";
        std::cout << "Codec: " << meta.codec << "\n";
        std::cout << "Sample Format: " << meta.sample_format << "\n";
        std::cout << "Channels: " << meta.num_channels << "\n";
        std::cout << "Sample Rate: " << meta.sample_rate << " Hz\n";
        std::cout << "Bit Rate: " << meta.bit_rate / 1000 << " kbps\n";
        std::cout << "Initial Num Samples: " << meta.num_samples << "\n";
        std::cout << "Initial Duration: " << meta.duration << " s\n";

        // Decode all frames and count samples
        size_t total_samples = 0;
        int frame_count = 0;
        while (!decoder.is_finished()) {
            auto samples = decoder.decode_next();
            if (samples.data.empty())
                break;
            total_samples += samples.data[0].size();
            frame_count++;
        }
        std::cout << "Decoded " << total_samples << " samples per channel in "
                  << frame_count << " frames (Chunked Read).\n";

        // Display finalized metadata
        const auto& final_meta = decoder.get_metadata();
        std::cout << "--- Finalized Metadata ---\n";
        std::cout << "Final Num Samples: " << final_meta.num_samples << "\n";
        std::cout << "Final Duration: " << final_meta.duration << " s\n";
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
    
    std::cout << "--- Testing Online (Chunked) Decode ---\n";
    test_online_decode(path);

    return 0;
}
