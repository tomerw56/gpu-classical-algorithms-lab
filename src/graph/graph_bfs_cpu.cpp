#include "graph/graph_bfs.hpp"

#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace algobench::graph_bfs
{
namespace
{

void fill_metadata(BenchmarkResult& result,
                   const GraphBfsShape& shape,
                   const graph::CsrGraph& graph,
                   const graph::GraphStats& stats,
                   const BfsValidationSummary& validation)
{
    result.metadata["graph_kind"] = shape.graph_kind;
    result.metadata["shape"] = describe_shape(shape);
    result.metadata["source"] = std::to_string(shape.source);
    result.metadata["edge_count"] = std::to_string(graph.edge_count());
    result.metadata["min_out_degree"] = std::to_string(stats.min_out_degree);
    result.metadata["max_out_degree"] = std::to_string(stats.max_out_degree);
    result.metadata["mean_out_degree"] = std::to_string(stats.mean_out_degree);
    result.metadata["zero_out_degree_count"] = std::to_string(stats.zero_out_degree_count);
    result.metadata["reached_count"] = std::to_string(validation.reached_count);
    result.metadata["max_distance"] = std::to_string(validation.max_distance);
    result.metadata["frontier_level_count"] = std::to_string(validation.frontier_level_count);
    result.metadata["max_frontier_size"] = std::to_string(validation.max_frontier_size);
    result.metadata["mean_frontier_size"] = std::to_string(validation.mean_frontier_size);
    result.metadata["reached_edge_visits"] = std::to_string(validation.reached_edge_visits);
    result.metadata["max_frontier_edge_visits"] = std::to_string(validation.max_frontier_edge_visits);
    result.metadata["mean_frontier_edge_visits"] = std::to_string(validation.mean_frontier_edge_visits);
    result.metadata["mean_reached_out_degree"] = std::to_string(validation.mean_reached_out_degree);
    result.metadata["frontier_width_to_depth"] = std::to_string(validation.frontier_width_to_depth);
    result.metadata["mismatch_count"] = std::to_string(validation.mismatch_count);
    result.metadata["checksum"] = std::to_string(validation.checksum);
    result.metadata["reference_checksum"] = std::to_string(validation.reference_checksum);
    result.metadata["validation"] = "element-wise BFS distance comparison against CPU queue reference";
}

} // namespace

std::vector<BenchmarkResult> run_graph_bfs_cpu(const BenchmarkConfig& config)
{
    const auto shape = shape_for_config(config);
    const auto graph = make_graph_for_shape(shape);
    const auto stats = graph::summarize_graph(graph);
    std::vector<std::int32_t> distances;

    for (int i = 0; i < config.warmup; ++i)
    {
        distances = bfs_cpu_reference(graph, shape.source);
    }

    CpuTimer timer;
    timer.start();

    for (int r = 0; r < config.repeat; ++r)
    {
        distances = bfs_cpu_reference(graph, shape.source);
    }

    const double elapsed_ms = timer.stop_ms();
    const auto validation = validate_bfs_distances(graph, shape.source, distances);

    BenchmarkResult result;
    result.benchmark = "graph_bfs";
    result.variant = "cpu";
    result.preset = result_preset_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["nodes"] = graph.node_count;
    result.input_size["edges"] = graph.edge_count();
    result.total_ms = elapsed_ms;
    result.correct = validation.correct;
    result.device = cpu_device_name();
    result.notes = "queue-based BFS over CSR graph";
    fill_metadata(result, shape, graph, stats, validation);

    return {result};
}

} // namespace algobench::graph_bfs
