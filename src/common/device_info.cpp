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

CudaRuntimeInfo cuda_runtime_info()
{
    CudaRuntimeInfo info;

#if GPUALGOBENCH_ENABLE_CUDA
    info.compiled_with_cuda = true;

    cudaError_t err = cudaDriverGetVersion(&info.driver_version);
    if (err != cudaSuccess)
    {
        info.error_name = cudaGetErrorName(err);
        info.error_string = cudaGetErrorString(err);
        return info;
    }

    err = cudaRuntimeGetVersion(&info.runtime_version);
    if (err != cudaSuccess)
    {
        info.error_name = cudaGetErrorName(err);
        info.error_string = cudaGetErrorString(err);
        return info;
    }

    err = cudaGetDeviceCount(&info.device_count);
    if (err != cudaSuccess)
    {
        info.error_name = cudaGetErrorName(err);
        info.error_string = cudaGetErrorString(err);
        return info;
    }

    if (info.device_count <= 0)
    {
        info.error_name = "cudaErrorNoDevice";
        info.error_string = "no CUDA-capable device was found";
        return info;
    }

    err = cudaGetDevice(&info.current_device);
    if (err != cudaSuccess)
    {
        info.error_name = cudaGetErrorName(err);
        info.error_string = cudaGetErrorString(err);
        return info;
    }

    cudaDeviceProp prop{};
    err = cudaGetDeviceProperties(&prop, info.current_device);
    if (err != cudaSuccess)
    {
        info.error_name = cudaGetErrorName(err);
        info.error_string = cudaGetErrorString(err);
        return info;
    }

    std::ostringstream os;
    os << prop.name << " sm_" << prop.major << prop.minor
       << " global_mem_mb=" << (prop.totalGlobalMem / (1024 * 1024));
    info.device_name = os.str();
    info.available = true;
    info.error_name = "cudaSuccess";
    info.error_string = "success";
#else
    info.compiled_with_cuda = false;
    info.error_name = "CUDA disabled at build time";
    info.error_string = "GPUALGOBENCH_ENABLE_CUDA=0";
#endif

    return info;
}

std::string cuda_runtime_status()
{
    const CudaRuntimeInfo info = cuda_runtime_info();
    std::ostringstream os;
    os << "compiled_with_cuda=" << (info.compiled_with_cuda ? "yes" : "no")
       << ", available=" << (info.available ? "yes" : "no")
       << ", device_count=" << info.device_count
       << ", driver_version=" << info.driver_version
       << ", runtime_version=" << info.runtime_version
       << ", error=" << info.error_name << " (" << info.error_string << ")";

    if (!info.device_name.empty())
    {
        os << ", device=" << info.device_name;
    }

    return os.str();
}

bool cuda_runtime_available()
{
    return cuda_runtime_info().available;
}

std::string cuda_device_name()
{
    const CudaRuntimeInfo info = cuda_runtime_info();
    if (info.available)
    {
        return info.device_name;
    }

    std::ostringstream os;
    os << "CUDA unavailable: " << info.error_name;
    if (!info.error_string.empty())
    {
        os << " - " << info.error_string;
    }
    return os.str();
}

} // namespace algobench
