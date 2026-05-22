#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"
#include "graph/graph_foundation.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace algobench::graph_bfs
{

/** Sentinel distance for nodes not reached by BFS. */
inline constexpr std::int32_t kUnreachedDistance = -1;

/**
 * Concrete graph dimensions used by the BFS benchmark.
 *
 * `graph_kind` controls which generator from graph_foundation is used. The
 * presets intentionally keep the same qualitative meaning across graph kinds:
 * tiny is readable/debuggable, while large starts to expose frontier behavior.
 */
struct GraphBfsShape
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
    std::uint32_t random_seed = 0xC001CAFEu;
};

/**
 * High-level validation/summary information for a BFS distance vector.
 *
 * The frontier fields are computed from the CPU reference distance vector.
 * They explain why the same BFS implementation can behave very differently
 * across graph anatomies:
 *
 * - chain graphs have huge depth but max_frontier_size ~= 1
 * - grids have many levels but only moderate wavefronts
 * - layered/random graphs can expose large frontiers and more useful work
 *   per synchronization point
 */
struct BfsValidationSummary
{
    bool correct = true;
    std::int64_t mismatch_count = 0;
    std::int64_t reached_count = 0;
    std::int32_t max_distance = 0;
    std::int64_t checksum = 0;
    std::int64_t reference_checksum = 0;

    // Frontier/profile metrics derived from the reference BFS tree.
    std::int32_t frontier_level_count = 0;
    std::int64_t max_frontier_size = 0;
    double mean_frontier_size = 0.0;

    // Sum of outgoing edges incident to reached nodes. This approximates the
    // amount of edge-scanning work performed by a top-down BFS traversal.
    std::int64_t reached_edge_visits = 0;
    std::int64_t max_frontier_edge_visits = 0;
    double mean_frontier_edge_visits = 0.0;

    // Useful compact ratios for plots and documentation.
    double mean_reached_out_degree = 0.0;
    double frontier_width_to_depth = 0.0;
};

/** Resolve benchmark preset + optional --set graph=... overrides into a graph shape. */
GraphBfsShape shape_for_config(const BenchmarkConfig& config);

/** Build the benchmark graph for a resolved shape. */
graph::CsrGraph make_graph_for_shape(const GraphBfsShape& shape);

/** Human-readable description for result metadata. */
std::string describe_shape(const GraphBfsShape& shape);

/**
 * Display label for benchmark result tables.
 *
 * Normal ad-hoc graph_bfs runs keep the ordinary preset name (`tiny`,
 * `small`, ...). Sweep scripts may pass `--set sweep_label=...` so the
 * console table and JSONL rows display a meaningful scale point such as
 * `layered_64x4096` instead of the implementation-detail base preset used
 * while explicit `--set` dimensions override the shape.
 */
std::string result_preset_label_for_config(const BenchmarkConfig& config);

/** CPU queue BFS reference implementation over CSR. */
std::vector<std::int32_t> bfs_cpu_reference(const graph::CsrGraph& graph, std::int32_t source);

/** Validate a computed BFS distance vector against the CPU reference. */
BfsValidationSummary validate_bfs_distances(const graph::CsrGraph& graph,
                                            std::int32_t source,
                                            const std::vector<std::int32_t>& distances);

std::vector<BenchmarkResult> run_graph_bfs_cpu(const BenchmarkConfig& config);

#if GPUALGOBENCH_ENABLE_CUDA
std::vector<BenchmarkResult> run_graph_bfs_gpu(const BenchmarkConfig& config);
#endif

} // namespace algobench::graph_bfs
