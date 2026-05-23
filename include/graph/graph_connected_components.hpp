#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"
#include "graph/graph_foundation.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace algobench::graph_cc
{

/**
 * Graph dimensions for the connected-components benchmark.
 *
 * The benchmark intentionally uses graph families with multiple disconnected
 * components. The CPU baseline uses Union-Find. The GPU implementation uses
 * iterative label propagation, so component diameter and graph anatomy matter.
 */
struct GraphCcShape
{
    std::string graph_kind = "random_clusters";

    std::int32_t component_count = 0;
    std::int32_t component_size = 0;

    // Used by grid_components.
    std::int32_t grid_width = 0;
    std::int32_t grid_height = 0;

    // Used by random_clusters.
    std::int32_t random_out_degree = 0;
    std::uint32_t random_seed = 0xC0FFEEu;

    // GPU convergence guard. Zero means choose a shape-dependent default.
    std::int32_t max_iterations = 0;
};

/** Summary/validation metadata for a component-label vector. */
struct ConnectedComponentsSummary
{
    bool correct = true;
    std::int64_t mismatch_count = 0;
    std::int32_t component_count = 0;
    std::int64_t largest_component_size = 0;
    std::int64_t isolated_node_count = 0;
    std::int64_t checksum = 0;
    std::int64_t reference_checksum = 0;
};

GraphCcShape cc_shape_for_config(const BenchmarkConfig& config);
graph::CsrGraph make_cc_graph_for_shape(const GraphCcShape& shape);
std::string describe_cc_shape(const GraphCcShape& shape);
std::string cc_result_preset_label_for_config(const BenchmarkConfig& config);

/** CPU Union-Find reference implementation. Returns canonical min-node labels. */
std::vector<std::int32_t> connected_components_cpu_reference(const graph::CsrGraph& graph);

/** Normalize arbitrary component labels to canonical min-node labels. */
std::vector<std::int32_t> normalize_component_labels(const std::vector<std::int32_t>& labels);

/** Validate labels against the CPU Union-Find reference. */
ConnectedComponentsSummary validate_component_labels(const graph::CsrGraph& graph,
                                                     const std::vector<std::int32_t>& labels);

std::vector<BenchmarkResult> run_graph_connected_components_cpu(const BenchmarkConfig& config);

#if GPUALGOBENCH_ENABLE_CUDA
std::vector<BenchmarkResult> run_graph_connected_components_gpu(const BenchmarkConfig& config);
#endif

} // namespace algobench::graph_cc
