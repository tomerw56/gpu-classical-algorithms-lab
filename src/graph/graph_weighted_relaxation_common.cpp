#include "graph/graph_weighted_relaxation.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace algobench::graph_weighted
{
namespace
{

std::string get_param_or(const BenchmarkConfig& config, const std::string& key, const std::string& fallback)
{
    const auto it = config.params.find(key);
    return it == config.params.end() ? fallback : it->second;
}

std::int32_t get_int_param_or(const BenchmarkConfig& config, const std::string& key, std::int32_t fallback)
{
    const auto it = config.params.find(key);
    if (it == config.params.end())
    {
        return fallback;
    }
    const int value = std::stoi(it->second);
    if (value < 0)
    {
        throw std::runtime_error("parameter must be non-negative: " + key);
    }
    return static_cast<std::int32_t>(value);
}

std::uint32_t get_uint_param_or(const BenchmarkConfig& config, const std::string& key, std::uint32_t fallback)
{
    const auto it = config.params.find(key);
    if (it == config.params.end())
    {
        return fallback;
    }
    return static_cast<std::uint32_t>(std::stoul(it->second));
}

std::int32_t deterministic_weight(std::int32_t source, std::int32_t target, std::int64_t edge_index)
{
    const auto raw = static_cast<std::uint32_t>(source * 1315423911u) ^
                     static_cast<std::uint32_t>(target * 2654435761u) ^
                     static_cast<std::uint32_t>(edge_index * 97u);
    return 1 + static_cast<std::int32_t>(raw % 31u);
}

std::vector<std::int32_t> make_weights_for_graph(const graph::CsrGraph& graph)
{
    std::vector<std::int32_t> weights(static_cast<std::size_t>(graph.edge_count()));
    for (std::int32_t source = 0; source < graph.node_count; ++source)
    {
        const auto begin = graph.row_offsets[static_cast<std::size_t>(source)];
        const auto end = graph.row_offsets[static_cast<std::size_t>(source + 1)];
        for (auto edge = begin; edge < end; ++edge)
        {
            const auto target = graph.column_indices[static_cast<std::size_t>(edge)];
            weights[static_cast<std::size_t>(edge)] = deterministic_weight(source, target, edge);
        }
    }
    return weights;
}

} // namespace

WeightedRelaxationShape weighted_shape_for_config(const BenchmarkConfig& config)
{
    WeightedRelaxationShape shape;
    shape.graph_kind = get_param_or(config, "graph", "layered");

    if (config.preset == "tiny")
    {
        shape.chain_nodes = 256;
        shape.grid_width = 16;
        shape.grid_height = 16;
        shape.layer_count = 8;
        shape.nodes_per_layer = 64;
        shape.fanout = 4;
        shape.random_nodes = 1024;
        shape.random_out_degree = 8;
    }
    else if (config.preset == "medium")
    {
        shape.chain_nodes = 16'384;
        shape.grid_width = 256;
        shape.grid_height = 256;
        shape.layer_count = 64;
        shape.nodes_per_layer = 4096;
        shape.fanout = 8;
        shape.random_nodes = 262'144;
        shape.random_out_degree = 8;
    }
    else if (config.preset == "large")
    {
        shape.chain_nodes = 65'536;
        shape.grid_width = 512;
        shape.grid_height = 512;
        shape.layer_count = 96;
        shape.nodes_per_layer = 8192;
        shape.fanout = 8;
        shape.random_nodes = 1'048'576;
        shape.random_out_degree = 8;
    }
    else
    {
        // small/default
        shape.chain_nodes = 4096;
        shape.grid_width = 64;
        shape.grid_height = 64;
        shape.layer_count = 32;
        shape.nodes_per_layer = 512;
        shape.fanout = 6;
        shape.random_nodes = 16'384;
        shape.random_out_degree = 8;
    }

    shape.source = get_int_param_or(config, "source", shape.source);
    shape.chain_nodes = get_int_param_or(config, "chain_nodes", shape.chain_nodes);
    shape.grid_width = get_int_param_or(config, "grid_width", shape.grid_width);
    shape.grid_height = get_int_param_or(config, "grid_height", shape.grid_height);
    shape.layer_count = get_int_param_or(config, "layers", shape.layer_count);
    shape.nodes_per_layer = get_int_param_or(config, "nodes_per_layer", shape.nodes_per_layer);
    shape.fanout = get_int_param_or(config, "fanout", shape.fanout);
    shape.random_nodes = get_int_param_or(config, "random_nodes", shape.random_nodes);
    shape.random_out_degree = get_int_param_or(config, "random_out_degree", shape.random_out_degree);
    shape.random_seed = get_uint_param_or(config, "seed", shape.random_seed);
    shape.delta = get_int_param_or(config, "delta", shape.delta);
    if (shape.delta <= 0)
    {
        throw std::runtime_error("delta must be positive");
    }
    shape.max_iterations = get_int_param_or(config, "max_iterations", shape.max_iterations);

    return shape;
}

WeightedCsrGraph make_weighted_graph_for_shape(const WeightedRelaxationShape& shape)
{
    graph::CsrGraph base;
    if (shape.graph_kind == "chain")
    {
        base = graph::make_chain_graph(shape.chain_nodes, true);
    }
    else if (shape.graph_kind == "grid")
    {
        base = graph::make_grid_graph(shape.grid_width, shape.grid_height, true);
    }
    else if (shape.graph_kind == "layered")
    {
        base = graph::make_layered_graph(shape.layer_count, shape.nodes_per_layer, shape.fanout);
    }
    else if (shape.graph_kind == "random")
    {
        base = graph::make_random_sparse_graph(shape.random_nodes, shape.random_out_degree, shape.random_seed);
    }
    else
    {
        throw std::runtime_error("unknown weighted graph kind: " + shape.graph_kind);
    }

    if (base.node_count > 0 && (shape.source < 0 || shape.source >= base.node_count))
    {
        throw std::runtime_error("weighted graph source is outside node range");
    }

    WeightedCsrGraph weighted;
    weighted.graph = std::move(base);
    weighted.weights = make_weights_for_graph(weighted.graph);
    return weighted;
}

std::string describe_weighted_shape(const WeightedRelaxationShape& shape)
{
    if (shape.graph_kind == "chain")
    {
        return "chain_nodes=" + std::to_string(shape.chain_nodes);
    }
    if (shape.graph_kind == "grid")
    {
        return "grid=" + std::to_string(shape.grid_width) + "x" + std::to_string(shape.grid_height);
    }
    if (shape.graph_kind == "layered")
    {
        return "layers=" + std::to_string(shape.layer_count) + ", nodes_per_layer=" +
               std::to_string(shape.nodes_per_layer) + ", fanout=" + std::to_string(shape.fanout);
    }
    if (shape.graph_kind == "random")
    {
        return "random_nodes=" + std::to_string(shape.random_nodes) + ", out_degree=" +
               std::to_string(shape.random_out_degree) + ", seed=" + std::to_string(shape.random_seed);
    }
    return shape.graph_kind;
}

std::string weighted_result_preset_label_for_config(const BenchmarkConfig& config)
{
    const auto it = config.params.find("sweep_label");
    return it == config.params.end() ? config.preset : it->second;
}

std::int32_t weighted_default_max_iterations(const WeightedRelaxationShape& shape, const WeightedCsrGraph& graph)
{
    if (shape.max_iterations > 0)
    {
        return shape.max_iterations;
    }
    if (shape.graph_kind == "chain")
    {
        // A relaxation pass that performs the last useful update is still
        // followed by one no-change pass to prove convergence. A path with
        // N nodes has N-1 edges from the source to the far end, so use N
        // passes rather than N-1. Without this extra pass the distances can
        // already be correct while the convergence flag remains false.
        return std::max<std::int32_t>(1, graph.graph.node_count);
    }
    if (shape.graph_kind == "grid")
    {
        return std::max<std::int32_t>(1, shape.grid_width + shape.grid_height + 8);
    }
    if (shape.graph_kind == "layered")
    {
        return std::max<std::int32_t>(1, shape.layer_count + 4);
    }
    return std::max<std::int32_t>(32, std::min<std::int32_t>(2048, graph.graph.node_count));
}

std::vector<std::int32_t> dijkstra_cpu_reference(const WeightedCsrGraph& weighted, std::int32_t source)
{
    const auto& graph = weighted.graph;
    std::vector<std::int32_t> distances(static_cast<std::size_t>(graph.node_count), kWeightedInfDistance);
    if (graph.node_count == 0)
    {
        return distances;
    }
    distances[static_cast<std::size_t>(source)] = 0;

    using QueueEntry = std::pair<std::int32_t, std::int32_t>; // distance, node
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
    queue.push({0, source});

    while (!queue.empty())
    {
        const auto [distance, node] = queue.top();
        queue.pop();
        if (distance != distances[static_cast<std::size_t>(node)])
        {
            continue;
        }
        const auto begin = graph.row_offsets[static_cast<std::size_t>(node)];
        const auto end = graph.row_offsets[static_cast<std::size_t>(node + 1)];
        for (auto edge = begin; edge < end; ++edge)
        {
            const auto target = graph.column_indices[static_cast<std::size_t>(edge)];
            const auto candidate = distance + weighted.weights[static_cast<std::size_t>(edge)];
            if (candidate < distances[static_cast<std::size_t>(target)])
            {
                distances[static_cast<std::size_t>(target)] = candidate;
                queue.push({candidate, target});
            }
        }
    }

    return distances;
}

WeightedDistanceSummary validate_weighted_distances(const WeightedCsrGraph& graph,
                                                    std::int32_t source,
                                                    const std::vector<std::int32_t>& distances)
{
    const auto reference = dijkstra_cpu_reference(graph, source);
    if (distances.size() != reference.size())
    {
        throw std::runtime_error("weighted distance vector size mismatch");
    }

    WeightedDistanceSummary summary;
    for (std::size_t i = 0; i < reference.size(); ++i)
    {
        const auto actual = distances[i];
        const auto expected = reference[i];
        if (actual != expected)
        {
            ++summary.mismatch_count;
        }
        if (expected < kWeightedInfDistance)
        {
            ++summary.reached_count;
            summary.max_finite_distance = std::max(summary.max_finite_distance, expected);
        }
        summary.checksum += static_cast<std::int64_t>(actual < kWeightedInfDistance ? actual : -1) * static_cast<std::int64_t>(i + 1);
        summary.reference_checksum += static_cast<std::int64_t>(expected < kWeightedInfDistance ? expected : -1) * static_cast<std::int64_t>(i + 1);
    }
    summary.correct = summary.mismatch_count == 0;
    return summary;
}

} // namespace algobench::graph_weighted
