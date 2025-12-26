
#include "single-stream-decoder.h"
#include <iostream>
#include <fstream>
#include <vector>

void test_file_decode(const std::string& path) {
    try {
        avioflow::SingleStreamDecoder decoder;
        decoder.open(path);
        
        const auto& meta = decoder.get_metadata();
        std::cout << "Successfully opened file: " << path << "\n";
        std::cout << "Format: " << meta.format << "\n";
        std::cout << "Channels: " << meta.num_channels << "\n";
        std::cout << "Sample Rate: " << meta.sample_rate << " Hz\n";
        std::cout << "Duration: " << meta.duration << " s\n";

        auto samples = decoder.get_all_samples();
        std::cout << "Decoded " << samples[0].size() << " samples per channel.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error decoding file: " << e.what() << "\n";
    }
}

void test_memory_decode(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return;

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (file.read((char*)buffer.data(), size)) {
        try {
            avioflow::SingleStreamDecoder decoder;
            decoder.open(buffer.data(), buffer.size());
            
            const auto& meta = decoder.get_metadata();
            std::cout << "Successfully opened from memory.\n";
            std::cout << "Channels: " << meta.num_channels << "\n";
            
            auto samples = decoder.decode_next();
            if (!samples.empty()) {
                std::cout << "Successfully decoded first frame from memory. Samples: " << samples[0].size() << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Error decoding from memory: " << e.what() << "\n";
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: avioflow_test <audio_file_path>\n";
        return 0;
    }

    std::string path = argv[1];
    std::cout << "--- Testing File Decode ---\n";
    test_file_decode(path);

    std::cout << "\n--- Testing Memory Decode ---\n";
    test_memory_decode(path);

    return 0;
}
