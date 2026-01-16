#include "avioflow-cxx-api.h"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    try {
        std::cout << "--- Testing WASAPI Loopback Capture ---" << std::endl;
        std::cout << "Note: This test requires Windows and some audio playing on the system." << std::endl;

        avioflow::AudioDecoder decoder;
        try {
            decoder.open("wasapi_loopback");
        } catch (const std::exception& e) {
            std::cerr << "Could not open WASAPI loopback (maybe not on Windows or disabled): " << e.what() << std::endl;
            return 0; // Exit gracefully if not supported/available
        }

        const auto& meta = decoder.get_metadata();
        std::cout << "Captured Stream Info:" << std::endl;
        std::cout << "  Sample Rate: " << meta.sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << meta.num_channels << std::endl;
        std::cout << "  Format: " << meta.sample_format << std::endl;

        std::cout << "Capturing for 3 seconds..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        size_t total_samples = 0;
        int frame_count = 0;

        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(3)) {
            auto samples = decoder.decode_next();
            if (!samples.data.empty()) {
                total_samples += samples.data[0].size();
                frame_count++;
                if (frame_count % 10 == 0) {
                    std::cout << "\rCaptured " << total_samples << " samples in " << frame_count << " frames..." << std::flush;
                }
            } else {
                // No data yet, sleep a bit to avoid busy loop
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        std::cout << std::endl << "Capture finished." << std::endl;
        std::cout << "Total samples per channel: " << total_samples << std::endl;

        if (total_samples > 0) {
            std::cout << "SUCCESS: Captured audio data from system output." << std::endl;
        } else {
            std::cout << "WARNING: No audio data captured. Was audio playing?" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
