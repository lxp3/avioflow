#include "avioflow-cxx-api.h"
#include <iomanip>
#include <iostream>


int main() {
  try {
    std::cout << "--- Audio Device Enumeration ---" << std::endl;
    auto devices = avioflow::DeviceManager::list_audio_devices();

    if (devices.empty()) {
      std::cout << "No audio devices found." << std::endl;
      return 0;
    }

    std::cout << std::left << std::setw(10) << "Type" << std::left
              << std::setw(50) << "Description"
              << "Name/ID" << std::endl;
    std::cout << std::string(100, '-') << std::endl;

    for (const auto &dev : devices) {
      std::cout << std::left << std::setw(10)
                << (dev.is_output ? "Output" : "Input") << std::left
                << std::setw(50) << dev.description << dev.name << std::endl;
    }

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
