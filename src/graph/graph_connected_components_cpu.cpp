#include "graph/graph_connected_components.hpp"

#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

#include <string>
#include <vector>

namespace algobench::graph_cc
{
namespace
{

void fill_metadata(BenchmarkResult& result,
                   const GraphCcShape& shape,
                   const graph::CsrGraph& graph,
                   const graph::GraphStats& stats,
                   const ConnectedComponentsSummary& summary,
                   std::int32_t iterations = 0,
                   bool converged = true)
{
    result.metadata["graph_kind"] = shape.graph_kind;
    result.metadata["shape"] = describe_cc_shape(shape);
    result.metadata["edge_count"] = std::to_string(graph.edge_count());
    result.metadata["min_out_degree"] = std::to_string(stats.min_out_degree);
    result.metadata["max_out_degree"] = std::to_string(stats.max_out_degree);
    result.metadata["mean_out_degree"] = std::to_string(stats.mean_out_degree);
    result.metadata["zero_out_degree_count"] = std::to_string(stats.zero_out_degree_count);
    result.metadata["component_count"] = std::to_string(summary.component_count);
    result.metadata["largest_component_size"] = std::to_string(summary.largest_component_size);
    result.metadata["isolated_node_count"] = std::to_string(summary.isolated_node_count);
    result.metadata["mismatch_count"] = std::to_string(summary.mismatch_count);
    result.metadata["checksum"] = std::to_string(summary.checksum);
    result.metadata["reference_checksum"] = std::to_string(summary.reference_checksum);
    result.metadata["iterations"] = std::to_string(iterations);
    result.metadata["converged"] = converged ? "true" : "false";
    result.metadata["validation"] = "component labels normalized and compared against CPU Union-Find reference";
}

} // namespace

std::vector<BenchmarkResult> run_graph_connected_components_cpu(const BenchmarkConfig& config)
{
    const auto shape = cc_shape_for_config(config);
    const auto graph = make_cc_graph_for_shape(shape);
    const auto stats = graph::summarize_graph(graph);
    std::vector<std::int32_t> labels;

    for (int i = 0; i < config.warmup; ++i)
    {
        labels = connected_components_cpu_reference(graph);
    }

    CpuTimer timer;
    timer.start();

    for (int r = 0; r < config.repeat; ++r)
    {
        labels = connected_components_cpu_reference(graph);
    }

    const double elapsed_ms = timer.stop_ms();
    const auto summary = validate_component_labels(graph, labels);

    BenchmarkResult result;
    result.benchmark = "graph_connected_components";
    result.variant = "cpu";
    result.preset = cc_result_preset_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["nodes"] = graph.node_count;
    result.input_size["edges"] = graph.edge_count();
    result.total_ms = elapsed_ms;
    result.correct = summary.correct;
    result.device = cpu_device_name();
    result.notes = "Union-Find connected components over CSR graph";
    fill_metadata(result, shape, graph, stats, summary);

    return {result};
}

} // namespace algobench::graph_cc
