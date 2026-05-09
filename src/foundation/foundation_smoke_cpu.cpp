#include "foundation/foundation_smoke.hpp"

#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace algobench::foundation
{

std::int64_t values_for_preset(const std::string& preset)
{
    if (preset == "tiny")
    {
        return 1024;
    }
    if (preset == "small")
    {
        return 1'000'000;
    }
    if (preset == "medium")
    {
        return 5'000'000;
    }
    if (preset == "large")
    {
        return 20'000'000;
    }

    throw std::runtime_error("unknown preset for foundation_smoke: " + preset);
}

namespace
{

double run_transform(std::int64_t n)
{
    std::vector<double> values(static_cast<std::size_t>(n));
    double checksum = 0.0;

    for (std::int64_t i = 0; i < n; ++i)
    {
        const double x = static_cast<double>((i % 1024) + 1) * 0.001;
        const double y = x * x + 2.0 * x + 1.0;
        values[static_cast<std::size_t>(i)] = y;
        checksum += y;
    }

    // Prevent aggressive optimization from removing the loop.
    return checksum + values[static_cast<std::size_t>(n - 1)] * 1e-12;
}

} // namespace

std::vector<BenchmarkResult> run_foundation_smoke_cpu(const BenchmarkConfig& config)
{
    const std::int64_t n = values_for_preset(config.preset);

    for (int i = 0; i < config.warmup; ++i)
    {
        (void)run_transform(n);
    }

    CpuTimer timer;
    timer.start();

    double checksum = 0.0;
    for (int r = 0; r < config.repeat; ++r)
    {
        checksum += run_transform(n);
    }

    const double elapsed_ms = timer.stop_ms();

    BenchmarkResult result;
    result.benchmark = "foundation_smoke";
    result.variant = "cpu";
    result.preset = config.preset;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["values"] = n;
    result.total_ms = elapsed_ms;
    result.correct = std::isfinite(checksum) && checksum > 0.0;
    result.device = cpu_device_name();
    result.notes = "infrastructure smoke benchmark";
    result.metadata["checksum"] = std::to_string(checksum);

    return {result};
}

} // namespace algobench::foundation
