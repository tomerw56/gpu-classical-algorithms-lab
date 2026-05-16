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

double smoke_value_for_index(std::int64_t i)
{
    const double x = static_cast<double>((i % 1024) + 1) * 0.001;
    return x * x + 2.0 * x + 1.0;
}

double transform_into(std::vector<double>& values)
{
    double checksum = 0.0;

    for (std::int64_t i = 0; i < static_cast<std::int64_t>(values.size()); ++i)
    {
        const double y = smoke_value_for_index(i);
        values[static_cast<std::size_t>(i)] = y;
        checksum += y;
    }

    return checksum;
}

double reference_checksum(std::int64_t n)
{
    double checksum = 0.0;

    for (std::int64_t i = 0; i < n; ++i)
    {
        checksum += smoke_value_for_index(i);
    }

    return checksum;
}

} // namespace

std::vector<BenchmarkResult> run_foundation_smoke_cpu(const BenchmarkConfig& config)
{
    const std::int64_t n = values_for_preset(config.preset);
    std::vector<double> values(static_cast<std::size_t>(n));

    for (int i = 0; i < config.warmup; ++i)
    {
        (void)transform_into(values);
    }

    CpuTimer timer;
    timer.start();

    // Important correctness policy:
    // repeat controls how many measured executions are timed, but the checksum
    // represents the final single output vector. Do not accumulate checksums
    // across repeats; otherwise CPU and GPU metadata describe different work.
    double checksum = 0.0;
    for (int r = 0; r < config.repeat; ++r)
    {
        checksum = transform_into(values);
    }

    const double elapsed_ms = timer.stop_ms();
    const double reference = reference_checksum(n);
    const double tolerance = std::max(1e-6, std::abs(reference) * 1e-10);

    BenchmarkResult result;
    result.benchmark = "foundation_smoke";
    result.variant = "cpu";
    result.preset = config.preset;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["values"] = n;
    result.total_ms = elapsed_ms;
    result.correct = std::isfinite(checksum) && std::abs(checksum - reference) <= tolerance;
    result.device = cpu_device_name();
    result.notes = "infrastructure smoke benchmark";
    result.metadata["checksum"] = std::to_string(checksum);
    result.metadata["reference"] = std::to_string(reference);
    result.metadata["checksum_policy"] = "final single output vector; not accumulated across repeats";

    return {result};
}

} // namespace algobench::foundation
