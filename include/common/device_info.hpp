#pragma once

#include <string>

namespace algobench
{

struct CudaRuntimeInfo
{
    bool compiled_with_cuda = false;
    bool available = false;
    int device_count = 0;
    int current_device = 0;
    int driver_version = 0;
    int runtime_version = 0;
    std::string error_name;
    std::string error_string;
    std::string device_name;
};

std::string cpu_device_name();
CudaRuntimeInfo cuda_runtime_info();
std::string cuda_runtime_status();
std::string cuda_device_name();
bool cuda_runtime_available();

} // namespace algobench
