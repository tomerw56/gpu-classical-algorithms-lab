#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace algobench::polynomial
{

inline constexpr std::size_t kPolynomialCoefficientCount = 16;
inline constexpr std::int64_t kPolynomialXCycle = 4096;
inline constexpr double kPolynomialXStep = 100.0;
inline constexpr double kPolynomialCoefficientScale = 1.0e-5;

using Coefficients = std::array<double, kPolynomialCoefficientCount>;

Coefficients default_coefficients();
std::int64_t values_for_preset(const std::string& preset);
double x_for_index(std::int64_t index);
double evaluate_horner(double x, const Coefficients& coefficients);

std::vector<BenchmarkResult> run_polynomial_batch_cpu(const BenchmarkConfig& config);

#if GPUALGOBENCH_ENABLE_CUDA
std::vector<BenchmarkResult> run_polynomial_batch_gpu(const BenchmarkConfig& config);
#endif

} // namespace algobench::polynomial
