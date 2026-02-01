#include "avioflow-cxx-api.h"
#include <iostream>
#include <string>
#include <vector>


void print_list(const std::string &title,
                const std::vector<std::string> &list) {
  std::cout << "=== " << title << " (" << list.size() << ") ===" << std::endl;
  for (const auto &item : list) {
    std::cout << "  " << item << std::endl;
  }
  std::cout << std::endl;
}

int main() {
  try {
    auto decoders = avioflow::get_supported_decoders();
    auto encoders = avioflow::get_supported_encoders();
    auto formats = avioflow::get_supported_input_formats();

    print_list("Supported Audio Decoders", decoders);
    print_list("Supported Audio Encoders", encoders);
    print_list("Supported Input Formats", formats);

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
