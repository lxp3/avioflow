
#include "single-stream-decoder.h"
#include <fstream>
#include <iostream>
#include <vector>

void test_file_decode(const std::string &path)
{
    try
    {
        avioflow::SingleStreamDecoder decoder;
        decoder.open(path);

        const auto &meta = decoder.get_metadata();
        std::cout << "Successfully opened file: " << path << "\n";
        std::cout << "Format: " << meta.sample_format << "\n";
        std::cout << "Channels: " << meta.num_channels << "\n";
        std::cout << "Sample Rate: " << meta.sample_rate << " Hz\n";
        std::cout << "Duration: " << meta.duration << " s\n";

        // Decode all frames and count samples
        size_t total_samples = 0;
        int frame_count = 0;
        while (decoder.has_more())
        {
            auto *frame = decoder.decode_next();
            if (frame == nullptr)
                break;
            total_samples += frame->nb_samples;
            frame_count++;
        }
        std::cout << "Decoded " << total_samples << " samples per channel in "
                  << frame_count << " frames.\n";
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
