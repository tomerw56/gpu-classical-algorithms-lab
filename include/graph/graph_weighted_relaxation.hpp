#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"
#include "graph/graph_foundation.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace algobench::graph_weighted
{

inline constexpr std::int32_t kWeightedInfDistance = 1'000'000'000;

/**
 * CSR graph plus a positive integer weight per directed adjacency entry.
 *
 * The weight vector is aligned with `graph.column_indices`: `weights[e]` is the
 * cost of edge `e`, where `e` belongs to the CSR row whose range contains it.
 */
struct WeightedCsrGraph
{
    graph::CsrGraph graph;
    std::vector<std::int32_t> weights;
};

/** Benchmark graph dimensions for weighted shortest-path experiments. */
struct WeightedRelaxationShape
{
    std::string graph_kind = "layered";
    std::int32_t source = 0;

    // Chain graph.
    std::int32_t chain_nodes = 0;

    // Grid graph.
    std::int32_t grid_width = 0;
    std::int32_t grid_height = 0;

    // Layered graph.
    std::int32_t layer_count = 0;
    std::int32_t nodes_per_layer = 0;
    std::int32_t fanout = 0;

    // Random sparse graph.
    std::int32_t random_nodes = 0;
    std::int32_t random_out_degree = 0;
    std::uint32_t random_seed = 0xA11CEu;

    // GPU convergence guard. Zero means choose a graph-family default.
    std::int32_t max_iterations = 0;
};

/** Validation summary for a shortest-distance vector. */
struct WeightedDistanceSummary
{
    bool correct = true;
    std::int64_t mismatch_count = 0;
    std::int64_t reached_count = 0;
    std::int32_t max_finite_distance = 0;
    std::int64_t checksum = 0;
    std::int64_t reference_checksum = 0;
};

WeightedRelaxationShape weighted_shape_for_config(const BenchmarkConfig& config);
WeightedCsrGraph make_weighted_graph_for_shape(const WeightedRelaxationShape& shape);
std::string describe_weighted_shape(const WeightedRelaxationShape& shape);
std::string weighted_result_preset_label_for_config(const BenchmarkConfig& config);
std::int32_t weighted_default_max_iterations(const WeightedRelaxationShape& shape, const WeightedCsrGraph& graph);

/** CPU Dijkstra reference over positive integer weights. */
std::vector<std::int32_t> dijkstra_cpu_reference(const WeightedCsrGraph& graph, std::int32_t source);

/** Validate a computed distance vector against the CPU Dijkstra reference. */
WeightedDistanceSummary validate_weighted_distances(const WeightedCsrGraph& graph,
                                                    std::int32_t source,
                                                    const std::vector<std::int32_t>& distances);

std::vector<BenchmarkResult> run_graph_weighted_relaxation_cpu(const BenchmarkConfig& config);

#if GPUALGOBENCH_ENABLE_CUDA
std::vector<BenchmarkResult> run_graph_weighted_relaxation_gpu(const BenchmarkConfig& config);
#endif

} // namespace algobench::graph_weighted
