#include "common/benchmark_registry.hpp"
#include "foundation/foundation_smoke.hpp"
#include "polynomial/polynomial_batch.hpp"
#include "cost_matrix/cost_matrix.hpp"
#include "spatial_events/spatial_events.hpp"
#include "graph/graph_bfs.hpp"
#include "graph/graph_connected_components.hpp"
#include "graph/graph_weighted_relaxation.hpp"
#include "constraints/constraint_network.hpp"

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

    registry.add(
        BenchmarkInfo{
            "polynomial_batch",
            "Degree-15 polynomial evaluation over many stride-100 x values using Horner's method.",
            {"tiny", "small", "medium", "large"}},
        [](const BenchmarkConfig& config) {
            std::vector<BenchmarkResult> results;

            auto cpu_results = polynomial::run_polynomial_batch_cpu(config);
            results.insert(results.end(), cpu_results.begin(), cpu_results.end());

#if GPUALGOBENCH_ENABLE_CUDA
            if (config.include_gpu)
            {
                auto gpu_results = polynomial::run_polynomial_batch_gpu(config);
                results.insert(results.end(), gpu_results.begin(), gpu_results.end());
            }
#else
            (void)config;
#endif

            return results;
        });

    registry.add(
        BenchmarkInfo{
            "cost_matrix",
            "Branch-heavy task/resource cost matrix: feasibility filtering and complex scoring.",
            {"tiny", "small", "medium", "large"}},
        [](const BenchmarkConfig& config) {
            std::vector<BenchmarkResult> results;

            auto cpu_results = cost_matrix::run_cost_matrix_cpu(config);
            results.insert(results.end(), cpu_results.begin(), cpu_results.end());

#if GPUALGOBENCH_ENABLE_CUDA
            if (config.include_gpu)
            {
                auto gpu_results = cost_matrix::run_cost_matrix_gpu(config);
                results.insert(results.end(), gpu_results.begin(), gpu_results.end());
            }
#else
            (void)config;
#endif

            return results;
        });

    registry.add(
        BenchmarkInfo{
            "spatial_events",
            "Track-segment versus circular-zone event detection: none, enter, exit, stay-inside, cross-through.",
            {"tiny", "small", "medium", "large"}},
        [](const BenchmarkConfig& config) {
            std::vector<BenchmarkResult> results;

            auto cpu_results = spatial_events::run_spatial_events_cpu(config);
            results.insert(results.end(), cpu_results.begin(), cpu_results.end());

#if GPUALGOBENCH_ENABLE_CUDA
            if (config.include_gpu)
            {
                auto gpu_results = spatial_events::run_spatial_events_gpu(config);
                results.insert(results.end(), gpu_results.begin(), gpu_results.end());
            }
#else
            (void)config;
#endif

            return results;
        });


    registry.add(
        BenchmarkInfo{
            "graph_bfs",
            "CSR breadth-first search / reachability: CPU queue BFS versus GPU frontier BFS.",
            {"tiny", "small", "medium", "large"}},
        [](const BenchmarkConfig& config) {
            std::vector<BenchmarkResult> results;

            auto cpu_results = graph_bfs::run_graph_bfs_cpu(config);
            results.insert(results.end(), cpu_results.begin(), cpu_results.end());

#if GPUALGOBENCH_ENABLE_CUDA
            if (config.include_gpu)
            {
                auto gpu_results = graph_bfs::run_graph_bfs_gpu(config);
                results.insert(results.end(), gpu_results.begin(), gpu_results.end());
            }
#else
            (void)config;
#endif

            return results;
        });


    registry.add(
        BenchmarkInfo{
            "graph_connected_components",
            "CSR connected components: CPU Union-Find versus naive and non-naive GPU variants.",
            {"tiny", "small", "medium", "large"}},
        [](const BenchmarkConfig& config) {
            std::vector<BenchmarkResult> results;

            auto cpu_results = graph_cc::run_graph_connected_components_cpu(config);
            results.insert(results.end(), cpu_results.begin(), cpu_results.end());

#if GPUALGOBENCH_ENABLE_CUDA
            if (config.include_gpu)
            {
                auto gpu_results = graph_cc::run_graph_connected_components_gpu(config);
                results.insert(results.end(), gpu_results.begin(), gpu_results.end());
            }
#else
            (void)config;
#endif

            return results;
        });


    registry.add(
        BenchmarkInfo{
            "graph_weighted_relaxation",
            "Weighted shortest paths: CPU Dijkstra versus GPU iterative edge relaxation.",
            {"tiny", "small", "medium", "large"}},
        [](const BenchmarkConfig& config) {
            std::vector<BenchmarkResult> results;

            auto cpu_results = graph_weighted::run_graph_weighted_relaxation_cpu(config);
            results.insert(results.end(), cpu_results.begin(), cpu_results.end());

#if GPUALGOBENCH_ENABLE_CUDA
            if (config.include_gpu)
            {
                auto gpu_results = graph_weighted::run_graph_weighted_relaxation_gpu(config);
                results.insert(results.end(), gpu_results.begin(), gpu_results.end());
            }
#else
            (void)config;
#endif

            return results;
        });


    registry.add(
        BenchmarkInfo{
            "constraint_network",
            "Batch candidate-assignment validation against skill/time/capacity/spatial/risk constraints.",
            {"tiny", "small", "medium", "large"}},
        [](const BenchmarkConfig& config) {
            std::vector<BenchmarkResult> results;

            auto cpu_results = constraints::run_constraint_network_cpu(config);
            results.insert(results.end(), cpu_results.begin(), cpu_results.end());

#if GPUALGOBENCH_ENABLE_CUDA
            if (config.include_gpu)
            {
                auto gpu_results = constraints::run_constraint_network_gpu(config);
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
