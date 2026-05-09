#include "common/device_info.hpp"

#include <sstream>
#include <thread>

#if GPUALGOBENCH_ENABLE_CUDA
#include <cuda_runtime.h>
#endif

namespace algobench
{

std::string cpu_device_name()
{
    std::ostringstream os;
    os << "CPU threads=" << std::thread::hardware_concurrency();
    return os.str();
}

bool cuda_runtime_available()
{
#if GPUALGOBENCH_ENABLE_CUDA
    int count = 0;
    const cudaError_t err = cudaGetDeviceCount(&count);
    return err == cudaSuccess && count > 0;
#else
    return false;
#endif
}

std::string cuda_device_name()
{
#if GPUALGOBENCH_ENABLE_CUDA
    int count = 0;
    const cudaError_t err = cudaGetDeviceCount(&count);
    if (err != cudaSuccess || count <= 0)
    {
        return "CUDA unavailable";
    }

    int device = 0;
    cudaGetDevice(&device);

    cudaDeviceProp prop{};
    if (cudaGetDeviceProperties(&prop, device) != cudaSuccess)
    {
        return "CUDA device properties unavailable";
    }

    std::ostringstream os;
    os << prop.name << " sm_" << prop.major << prop.minor
       << " global_mem_mb=" << (prop.totalGlobalMem / (1024 * 1024));
    return os.str();
#else
    return "CUDA disabled at build time";
#endif
}

} // namespace algobench
