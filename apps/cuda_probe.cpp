#include "common/device_info.hpp"

#include <iostream>

int main()
{
    const auto info = algobench::cuda_runtime_info();

    std::cout << "CUDA runtime probe\n";
    std::cout << "  compiled_with_cuda: " << (info.compiled_with_cuda ? "yes" : "no") << "\n";
    std::cout << "  available:          " << (info.available ? "yes" : "no") << "\n";
    std::cout << "  device_count:       " << info.device_count << "\n";
    std::cout << "  current_device:     " << info.current_device << "\n";
    std::cout << "  driver_version:     " << info.driver_version << "\n";
    std::cout << "  runtime_version:    " << info.runtime_version << "\n";
    std::cout << "  error_name:         " << info.error_name << "\n";
    std::cout << "  error_string:       " << info.error_string << "\n";
    if (!info.device_name.empty())
    {
        std::cout << "  device_name:        " << info.device_name << "\n";
    }

    return info.available ? 0 : 2;
}
