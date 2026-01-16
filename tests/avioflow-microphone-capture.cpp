#include "avioflow-cxx-api.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>

int main() {
    try {
        std::cout << "--- Audio Capture Device Test ---" << std::endl;
        
        // 1. List available devices
        std::cout << "Enumerating devices..." << std::endl;
        auto devices = avioflow::DeviceManager::list_audio_devices();
        
        std::vector<avioflow::DeviceInfo> input_devices;
        for (const auto& dev : devices) {
            if (!dev.is_output) {
                input_devices.push_back(dev);
            }
        }
        
        if (input_devices.empty()) {
            std::cout << "No input (microphone) devices found." << std::endl;
            return 0;
        }
        
        std::cout << "\nAvailable Input Devices:" << std::endl;
        std::cout << std::left << std::setw(5) << "ID" 
                  << std::setw(40) << "Description" 
                  << "Name" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        for (size_t i = 0; i < input_devices.size(); ++i) {
            std::cout << std::left << std::setw(5) << i 
                      << std::setw(40) << input_devices[i].description 
                      << input_devices[i].name << std::endl;
        }
        
        // 2. Select device
        int choice = -1;
        std::cout << "\nEnter device ID to capture (or -1 to exit): ";
        if (!(std::cin >> choice)) {
            std::cout << "Invalid input." << std::endl;
            return 1;
        }
        
        if (choice < 0 || static_cast<size_t>(choice) >= input_devices.size()) {
            std::cout << "Exiting." << std::endl;
            return 0;
        }
        
        const auto& selected = input_devices[choice];
        std::string source = selected.name; // DeviceManager already prefixes names with 'audio=' or 'video='
        
        std::cout << "\nSelected: " << selected.description << std::endl;
        std::cout << "Opening source: " << source << std::endl;
        
        // 3. Initialize decoder
        avioflow::AudioDecoder decoder;
        try {
            decoder.open(source);
        } catch (const std::exception& e) {
            std::cerr << "Failed to open device: " << e.what() << std::endl;
            return 1;
        }
        
        const auto& meta = decoder.get_metadata();
        std::cout << "\nCapture Started Successfully!" << std::endl;
        std::cout << "  Sample Rate: " << meta.sample_rate << " Hz" << std::endl;
        std::cout << "  Channels: " << meta.num_channels << std::endl;
        std::cout << "  Format: " << meta.sample_format << std::endl;
        
        std::cout << "\nCapturing for 5 seconds. Speak into the microphone now..." << std::endl;
        
        auto start = std::chrono::steady_clock::now();
        size_t total_samples = 0;
        int frame_count = 0;
        
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
            auto samples = decoder.decode_next();
            if (!samples.data.empty()) {
                total_samples += samples.data[0].size();
                frame_count++;
                if (frame_count % 10 == 0) {
                    std::cout << "\rCaptured " << total_samples << " samples (" 
                              << frame_count << " frames)..." << std::flush;
                }
            } else {
                // Device might need time to buffer
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        
        std::cout << "\n\nCapture complete!" << std::endl;
        std::cout << "Total samples per channel: " << total_samples << std::endl;
        
        if (total_samples > 0) {
            std::cout << "SUCCESS: Microphone is working and capturing data." << std::endl;
        } else {
            std::cout << "WARNING: No audio samples were captured. Check device permissions." << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
