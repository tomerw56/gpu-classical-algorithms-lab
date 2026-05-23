#include "graph/graph_weighted_relaxation.hpp"

#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

#include <string>
#include <vector>

namespace algobench::graph_weighted
{
namespace
{

void fill_metadata(BenchmarkResult& result,
                   const WeightedRelaxationShape& shape,
                   const WeightedCsrGraph& weighted,
                   const graph::GraphStats& stats,
                   const WeightedDistanceSummary& summary,
                   std::int32_t iterations = 0,
                   bool converged = true)
{
    result.metadata["graph_kind"] = shape.graph_kind;
    result.metadata["shape"] = describe_weighted_shape(shape);
    result.metadata["source"] = std::to_string(shape.source);
    result.metadata["edge_count"] = std::to_string(weighted.graph.edge_count());
    result.metadata["min_out_degree"] = std::to_string(stats.min_out_degree);
    result.metadata["max_out_degree"] = std::to_string(stats.max_out_degree);
    result.metadata["mean_out_degree"] = std::to_string(stats.mean_out_degree);
    result.metadata["zero_out_degree_count"] = std::to_string(stats.zero_out_degree_count);
    result.metadata["reached_count"] = std::to_string(summary.reached_count);
    result.metadata["max_finite_distance"] = std::to_string(summary.max_finite_distance);
    result.metadata["mismatch_count"] = std::to_string(summary.mismatch_count);
    result.metadata["checksum"] = std::to_string(summary.checksum);
    result.metadata["reference_checksum"] = std::to_string(summary.reference_checksum);
    result.metadata["iterations"] = std::to_string(iterations);
    result.metadata["converged"] = converged ? "true" : "false";
    result.metadata["algorithm"] = "CPU Dijkstra priority queue reference";
    result.metadata["validation"] = "exact weighted-distance comparison against CPU Dijkstra reference";
}

} // namespace

std::vector<BenchmarkResult> run_graph_weighted_relaxation_cpu(const BenchmarkConfig& config)
{
    const auto shape = weighted_shape_for_config(config);
    const auto weighted = make_weighted_graph_for_shape(shape);
    const auto stats = graph::summarize_graph(weighted.graph);
    std::vector<std::int32_t> distances;

    for (int i = 0; i < config.warmup; ++i)
    {
        distances = dijkstra_cpu_reference(weighted, shape.source);
    }

    CpuTimer timer;
    timer.start();
    for (int r = 0; r < config.repeat; ++r)
    {
        distances = dijkstra_cpu_reference(weighted, shape.source);
    }
    const double elapsed_ms = timer.stop_ms();

    const auto summary = validate_weighted_distances(weighted, shape.source, distances);

    BenchmarkResult result;
    result.benchmark = "graph_weighted_relaxation";
    result.variant = "cpu";
    result.preset = weighted_result_preset_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["nodes"] = weighted.graph.node_count;
    result.input_size["edges"] = weighted.graph.edge_count();
    result.total_ms = elapsed_ms;
    result.correct = summary.correct;
    result.device = cpu_device_name();
    result.notes = "Dijkstra over positive integer weighted CSR graph";
    fill_metadata(result, shape, weighted, stats, summary);
    return {result};
}

} // namespace algobench::graph_weighted
