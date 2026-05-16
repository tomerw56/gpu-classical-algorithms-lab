#include "polynomial/polynomial_batch.hpp"

#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace algobench::polynomial
{
namespace
{

__constant__ double kDeviceCoefficients[kPolynomialCoefficientCount];

__global__ void polynomial_batch_kernel(double* output, std::int64_t n)
{
    const std::int64_t idx = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (idx >= n)
    {
        return;
    }

    const std::int64_t cycle_index = (idx % kPolynomialXCycle) + 1;
    const double x = static_cast<double>(cycle_index) * kPolynomialXStep;

    double value = kDeviceCoefficients[kPolynomialCoefficientCount - 1];
#pragma unroll
    for (int degree = static_cast<int>(kPolynomialCoefficientCount) - 2; degree >= 0; --degree)
    {
        value = value * x + kDeviceCoefficients[degree];
    }

    output[idx] = value;
}

struct ValidationSummary
{
    double checksum = 0.0;
    double reference_checksum = 0.0;
    double max_abs_error = 0.0;
    double max_rel_error = 0.0;
    bool correct = true;
};

ValidationSummary validate_output(const std::vector<double>& output, const Coefficients& coefficients)
{
    ValidationSummary summary;

    for (std::int64_t i = 0; i < static_cast<std::int64_t>(output.size()); ++i)
    {
        const double actual = output[static_cast<std::size_t>(i)];
        const double reference = evaluate_horner(x_for_index(i), coefficients);
        const double abs_error = std::abs(actual - reference);
        const double rel_denominator = std::max(1.0, std::abs(reference));
        const double rel_error = abs_error / rel_denominator;
        const double tolerance = std::max(1.0e-9, std::abs(reference) * 1.0e-11);

        summary.checksum += actual;
        summary.reference_checksum += reference;
        summary.max_abs_error = std::max(summary.max_abs_error, abs_error);
        summary.max_rel_error = std::max(summary.max_rel_error, rel_error);

        if (!std::isfinite(actual) || abs_error > tolerance)
        {
            summary.correct = false;
        }
    }

    return summary;
}

void fill_common_metadata(BenchmarkResult& result, const ValidationSummary& validation)
{
    result.metadata["coefficient_count"] = std::to_string(kPolynomialCoefficientCount);
    result.metadata["x_step"] = std::to_string(kPolynomialXStep);
    result.metadata["x_cycle"] = std::to_string(kPolynomialXCycle);
    result.metadata["checksum"] = std::to_string(validation.checksum);
    result.metadata["reference_checksum"] = std::to_string(validation.reference_checksum);
    result.metadata["max_abs_error"] = std::to_string(validation.max_abs_error);
    result.metadata["max_rel_error"] = std::to_string(validation.max_rel_error);
    result.metadata["validation"] = "element-wise Horner comparison over final output vector";
    result.metadata["x_policy"] = "stride-100 domain cycled every 4096 values to keep degree-15 arithmetic bounded";
}

} // namespace

std::vector<BenchmarkResult> run_polynomial_batch_gpu(const BenchmarkConfig& config)
{
    BenchmarkResult result;
    result.benchmark = "polynomial_batch";
    result.variant = "gpu";
    result.preset = config.preset;
    result.repeat = config.repeat;
    result.warmup = config.warmup;

    const std::int64_t n = values_for_preset(config.preset);
    const auto coefficients = default_coefficients();
    result.input_size["values"] = n;
    result.input_size["coefficients"] = static_cast<std::int64_t>(kPolynomialCoefficientCount);
    result.device = cuda_device_name();
    result.notes = "degree-15 Horner evaluation with one CUDA thread per x value";
    result.metadata["cuda_status"] = cuda_runtime_status();

    if (!cuda_runtime_available())
    {
        result.correct = false;
        result.metadata["status"] = "CUDA runtime unavailable";
        return {result};
    }

    double* d_output = nullptr;
    const std::size_t output_bytes = static_cast<std::size_t>(n) * sizeof(double);
    CUDA_CHECK(cudaMalloc(&d_output, output_bytes));

    cudaEvent_t h2d_start{}, h2d_stop{};
    CUDA_CHECK(cudaEventCreate(&h2d_start));
    CUDA_CHECK(cudaEventCreate(&h2d_stop));
    CUDA_CHECK(cudaEventRecord(h2d_start));
    CUDA_CHECK(cudaMemcpyToSymbol(kDeviceCoefficients,
                                  coefficients.data(),
                                  coefficients.size() * sizeof(double),
                                  0,
                                  cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(h2d_stop));
    CUDA_CHECK(cudaEventSynchronize(h2d_stop));

    float h2d_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&h2d_ms, h2d_start, h2d_stop));

    const int threads = 256;
    const int blocks = static_cast<int>((n + threads - 1) / threads);

    for (int i = 0; i < config.warmup; ++i)
    {
        polynomial_batch_kernel<<<blocks, threads>>>(d_output, n);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    cudaEvent_t kernel_start{}, kernel_stop{};
    CUDA_CHECK(cudaEventCreate(&kernel_start));
    CUDA_CHECK(cudaEventCreate(&kernel_stop));
    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        polynomial_batch_kernel<<<blocks, threads>>>(d_output, n);
    }
    CUDA_CHECK(cudaEventRecord(kernel_stop));
    CUDA_CHECK(cudaEventSynchronize(kernel_stop));
    CUDA_CHECK(cudaGetLastError());

    float kernel_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&kernel_ms, kernel_start, kernel_stop));

    std::vector<double> host_output(static_cast<std::size_t>(n));

    cudaEvent_t d2h_start{}, d2h_stop{};
    CUDA_CHECK(cudaEventCreate(&d2h_start));
    CUDA_CHECK(cudaEventCreate(&d2h_stop));
    CUDA_CHECK(cudaEventRecord(d2h_start));
    CUDA_CHECK(cudaMemcpy(host_output.data(), d_output, output_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(d2h_stop));
    CUDA_CHECK(cudaEventSynchronize(d2h_stop));

    float d2h_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&d2h_ms, d2h_start, d2h_stop));

    const auto validation = validate_output(host_output, coefficients);

    result.h2d_ms = static_cast<double>(h2d_ms);
    result.kernel_ms = static_cast<double>(kernel_ms);
    result.d2h_ms = static_cast<double>(d2h_ms);
    result.total_ms = result.h2d_ms + result.kernel_ms + result.d2h_ms;
    result.correct = validation.correct;
    fill_common_metadata(result, validation);

    CUDA_CHECK(cudaEventDestroy(h2d_start));
    CUDA_CHECK(cudaEventDestroy(h2d_stop));
    CUDA_CHECK(cudaEventDestroy(kernel_start));
    CUDA_CHECK(cudaEventDestroy(kernel_stop));
    CUDA_CHECK(cudaEventDestroy(d2h_start));
    CUDA_CHECK(cudaEventDestroy(d2h_stop));
    CUDA_CHECK(cudaFree(d_output));

    return {result};
}

} // namespace algobench::polynomial
