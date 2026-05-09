#include "common/benchmark_registry.hpp"
#include "foundation/foundation_smoke.hpp"

#include <algorithm>
#include <stdexcept>

namespace algobench
{

void BenchmarkRegistry::add(BenchmarkInfo info, BenchmarkFunction function)
{
    entries_.push_back(Entry{std::move(info), std::move(function)});
}

std::vector<BenchmarkInfo> BenchmarkRegistry::list() const
{
    std::vector<BenchmarkInfo> infos;
    infos.reserve(entries_.size());
    for (const auto& entry : entries_)
    {
        infos.push_back(entry.info);
    }
    return infos;
}

std::vector<BenchmarkResult> BenchmarkRegistry::run(const std::string& name, const BenchmarkConfig& config) const
{
    std::vector<BenchmarkResult> all_results;

    if (name == "all")
    {
        for (const auto& entry : entries_)
        {
            auto local_config = config;
            local_config.benchmark = entry.info.name;
            auto results = entry.function(local_config);
            all_results.insert(all_results.end(), results.begin(), results.end());
        }
        return all_results;
    }

    const auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.info.name == name;
    });

    if (it == entries_.end())
    {
        throw std::runtime_error("unknown benchmark: " + name);
    }

    auto local_config = config;
    local_config.benchmark = it->info.name;
    return it->function(local_config);
}

BenchmarkRegistry make_default_registry()
{
    BenchmarkRegistry registry;

    registry.add(
        BenchmarkInfo{
            "foundation_smoke",
            "Infrastructure smoke benchmark: deterministic vector transform and checksum.",
            {"tiny", "small", "medium", "large"}},
        [](const BenchmarkConfig& config) {
            std::vector<BenchmarkResult> results;

            auto cpu_results = foundation::run_foundation_smoke_cpu(config);
            results.insert(results.end(), cpu_results.begin(), cpu_results.end());

#if GPUALGOBENCH_ENABLE_CUDA
            if (config.include_gpu)
            {
                auto gpu_results = foundation::run_foundation_smoke_gpu(config);
                results.insert(results.end(), gpu_results.begin(), gpu_results.end());
            }
#else
            (void)config;
#endif

            return results;
        });

    return registry;
}

} // namespace algobench
