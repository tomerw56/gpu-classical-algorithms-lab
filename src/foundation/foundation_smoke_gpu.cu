#include "foundation/foundation_smoke.hpp"

#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace algobench::foundation
{
namespace
{

__global__ void transform_kernel(double* output, std::int64_t n)
{
    const std::int64_t idx = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (idx >= n)
    {
        return;
    }

    const double x = static_cast<double>((idx % 1024) + 1) * 0.001;
    output[idx] = x * x + 2.0 * x + 1.0;
}

double cpu_reference_checksum(std::int64_t n)
{
    double checksum = 0.0;
    for (std::int64_t i = 0; i < n; ++i)
    {
        const double x = static_cast<double>((i % 1024) + 1) * 0.001;
        checksum += x * x + 2.0 * x + 1.0;
    }
    return checksum;
}

} // namespace

std::vector<BenchmarkResult> run_foundation_smoke_gpu(const BenchmarkConfig& config)
{
    BenchmarkResult result;
    result.benchmark = "foundation_smoke";
    result.variant = "gpu";
    result.preset = config.preset;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.device = cuda_device_name();
    result.notes = "CUDA infrastructure smoke benchmark";

    if (!cuda_runtime_available())
    {
        result.correct = false;
        result.metadata["status"] = "CUDA runtime unavailable";
        return {result};
    }

    const std::int64_t n = values_for_preset(config.preset);
    result.input_size["values"] = n;

    double* d_output = nullptr;
    const std::size_t bytes = static_cast<std::size_t>(n) * sizeof(double);
    CUDA_CHECK(cudaMalloc(&d_output, bytes));

    const int threads = 256;
    const int blocks = static_cast<int>((n + threads - 1) / threads);

    for (int i = 0; i < config.warmup; ++i)
    {
        transform_kernel<<<blocks, threads>>>(d_output, n);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    cudaEvent_t start{}, stop{};
    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&stop));

    CUDA_CHECK(cudaEventRecord(start));
    for (int r = 0; r < config.repeat; ++r)
    {
        transform_kernel<<<blocks, threads>>>(d_output, n);
    }
    CUDA_CHECK(cudaEventRecord(stop));
    CUDA_CHECK(cudaEventSynchronize(stop));
    CUDA_CHECK(cudaGetLastError());

    float kernel_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&kernel_ms, start, stop));

    std::vector<double> host_output(static_cast<std::size_t>(n));

    cudaEvent_t d2h_start{}, d2h_stop{};
    CUDA_CHECK(cudaEventCreate(&d2h_start));
    CUDA_CHECK(cudaEventCreate(&d2h_stop));
    CUDA_CHECK(cudaEventRecord(d2h_start));
    CUDA_CHECK(cudaMemcpy(host_output.data(), d_output, bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(d2h_stop));
    CUDA_CHECK(cudaEventSynchronize(d2h_stop));

    float d2h_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&d2h_ms, d2h_start, d2h_stop));

    double checksum = 0.0;
    for (double value : host_output)
    {
        checksum += value;
    }

    const double reference = cpu_reference_checksum(n);
    const double tolerance = std::max(1e-6, std::abs(reference) * 1e-10);

    result.kernel_ms = static_cast<double>(kernel_ms);
    result.d2h_ms = static_cast<double>(d2h_ms);
    result.total_ms = result.kernel_ms + result.d2h_ms;
    result.correct = std::abs(checksum - reference) <= tolerance;
    result.metadata["checksum"] = std::to_string(checksum);
    result.metadata["reference"] = std::to_string(reference);

    CUDA_CHECK(cudaEventDestroy(start));
    CUDA_CHECK(cudaEventDestroy(stop));
    CUDA_CHECK(cudaEventDestroy(d2h_start));
    CUDA_CHECK(cudaEventDestroy(d2h_stop));
    CUDA_CHECK(cudaFree(d_output));

    return {result};
}

} // namespace algobench::foundation
