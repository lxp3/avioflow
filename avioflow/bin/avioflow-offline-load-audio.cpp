
#include "avioflow-cxx-api.h"
#include <fstream>
#include <iostream>
#include <vector>

void test_file_decode(const std::string &path)
{
    try
    {
        avioflow::AudioDecoder decoder;
        decoder.load_file(path);

        const auto &meta = decoder.get_metadata();
        std::cout << "Successfully opened file: " << path << "\n";
        std::cout << "Format: " << meta.sample_format << "\n";
        std::cout << "Channels: " << meta.num_channels << "\n";
        std::cout << "Sample Rate: " << meta.sample_rate << " Hz\n";
        std::cout << "Duration: " << meta.duration << " s\n";

        // Decode all samples at once (offline mode)
        auto samples = decoder.get_samples();
        size_t total_samples = samples.empty() ? 0 : samples[0].size();
        
        std::cout << "Decoded " << total_samples << " samples per channel across "
                  << meta.num_channels << " channels.\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error decoding file: " << e.what() << "\n";
    }
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: avioflow_audio_load <audio_file_path>\n";
        return 0;
    }

    std::string path = argv[1];
    std::cout << "--- Testing File Decode ---\n";
    test_file_decode(path);

    return 0;
}
