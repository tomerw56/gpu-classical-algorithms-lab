#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"

#include <vector>

namespace algobench::foundation
{

std::vector<BenchmarkResult> run_foundation_smoke_cpu(const BenchmarkConfig& config);

#if GPUALGOBENCH_ENABLE_CUDA
std::vector<BenchmarkResult> run_foundation_smoke_gpu(const BenchmarkConfig& config);
#endif

std::int64_t values_for_preset(const std::string& preset);

} // namespace algobench::foundation
