#include "graph/graph_bfs.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

namespace algobench::graph_bfs
{
namespace
{

std::string get_param_or(const BenchmarkConfig& config, const std::string& key, const std::string& fallback)
{
    const auto it = config.params.find(key);
    if (it == config.params.end())
    {
        return fallback;
    }
    return it->second;
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
    const auto value = static_cast<unsigned long>(std::stoul(it->second));
    return static_cast<std::uint32_t>(value);
}

} // namespace

GraphBfsShape shape_for_config(const BenchmarkConfig& config)
{
    GraphBfsShape shape;
    shape.graph_kind = get_param_or(config, "graph", "layered");

    if (config.preset == "tiny")
    {
        shape.chain_nodes = 32;
        shape.grid_width = 8;
        shape.grid_height = 8;
        shape.layer_count = 6;
        shape.nodes_per_layer = 16;
        shape.fanout = 4;
        shape.random_nodes = 128;
        shape.random_out_degree = 4;
    }
    else if (config.preset == "small")
    {
        shape.chain_nodes = 8'192;
        shape.grid_width = 128;
        shape.grid_height = 128;
        shape.layer_count = 32;
        shape.nodes_per_layer = 512;
        shape.fanout = 8;
        shape.random_nodes = 32'768;
        shape.random_out_degree = 8;
    }
    else if (config.preset == "medium")
    {
        shape.chain_nodes = 65'536;
        shape.grid_width = 512;
        shape.grid_height = 512;
        shape.layer_count = 64;
        shape.nodes_per_layer = 4096;
        shape.fanout = 8;
        shape.random_nodes = 262'144;
        shape.random_out_degree = 8;
    }
    else if (config.preset == "large")
    {
        shape.chain_nodes = 262'144;
        shape.grid_width = 1024;
        shape.grid_height = 1024;
        shape.layer_count = 96;
        shape.nodes_per_layer = 8192;
        shape.fanout = 8;
        shape.random_nodes = 1'048'576;
        shape.random_out_degree = 8;
    }
    else
    {
        throw std::runtime_error("unknown preset for graph_bfs: " + config.preset);
    }

    shape.source = get_int_param_or(config, "source", 0);
    shape.chain_nodes = get_int_param_or(config, "chain_nodes", shape.chain_nodes);
    shape.grid_width = get_int_param_or(config, "grid_width", shape.grid_width);
    shape.grid_height = get_int_param_or(config, "grid_height", shape.grid_height);
    shape.layer_count = get_int_param_or(config, "layers", shape.layer_count);
    shape.nodes_per_layer = get_int_param_or(config, "nodes_per_layer", shape.nodes_per_layer);
    shape.fanout = get_int_param_or(config, "fanout", shape.fanout);
    shape.random_nodes = get_int_param_or(config, "random_nodes", shape.random_nodes);
    shape.random_out_degree = get_int_param_or(config, "random_out_degree", shape.random_out_degree);
    shape.random_seed = get_uint_param_or(config, "seed", shape.random_seed);

    return shape;
}

graph::CsrGraph make_graph_for_shape(const GraphBfsShape& shape)
{
    if (shape.graph_kind == "chain")
    {
        return graph::make_chain_graph(shape.chain_nodes, true);
    }
    if (shape.graph_kind == "grid")
    {
        return graph::make_grid_graph(shape.grid_width, shape.grid_height, true);
    }
    if (shape.graph_kind == "layered")
    {
        return graph::make_layered_graph(shape.layer_count, shape.nodes_per_layer, shape.fanout);
    }
    if (shape.graph_kind == "random" || shape.graph_kind == "random_sparse")
    {
        return graph::make_random_sparse_graph(shape.random_nodes, shape.random_out_degree, shape.random_seed);
    }

    throw std::runtime_error("unknown graph_bfs graph kind: " + shape.graph_kind);
}

std::string describe_shape(const GraphBfsShape& shape)
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
    if (shape.graph_kind == "random" || shape.graph_kind == "random_sparse")
    {
        return "random_nodes=" + std::to_string(shape.random_nodes) + ", out_degree=" +
               std::to_string(shape.random_out_degree) + ", seed=" + std::to_string(shape.random_seed);
    }
    return "unknown";
}

std::string result_preset_label_for_config(const BenchmarkConfig& config)
{
    const auto it = config.params.find("sweep_label");
    if (it != config.params.end() && !it->second.empty())
    {
        return it->second;
    }
    return config.preset;
}

std::vector<std::int32_t> bfs_cpu_reference(const graph::CsrGraph& graph, std::int32_t source)
{
    const auto report = graph::validate_csr_graph(graph);
    if (!report.valid)
    {
        throw std::runtime_error("cannot run BFS over invalid CSR graph: " + report.message);
    }
    if (source < 0 || source >= graph.node_count)
    {
        throw std::runtime_error("BFS source is outside graph node range");
    }

    std::vector<std::int32_t> distances(static_cast<std::size_t>(graph.node_count), kUnreachedDistance);
    std::deque<std::int32_t> queue;

    distances[static_cast<std::size_t>(source)] = 0;
    queue.push_back(source);

    while (!queue.empty())
    {
        const std::int32_t node = queue.front();
        queue.pop_front();

        const std::int32_t next_distance = distances[static_cast<std::size_t>(node)] + 1;
        const auto begin = graph.row_offsets[static_cast<std::size_t>(node)];
        const auto end = graph.row_offsets[static_cast<std::size_t>(node + 1)];

        for (std::int64_t edge = begin; edge < end; ++edge)
        {
            const std::int32_t neighbor = graph.column_indices[static_cast<std::size_t>(edge)];
            auto& neighbor_distance = distances[static_cast<std::size_t>(neighbor)];
            if (neighbor_distance == kUnreachedDistance)
            {
                neighbor_distance = next_distance;
                queue.push_back(neighbor);
            }
        }
    }

    return distances;
}

BfsValidationSummary validate_bfs_distances(const graph::CsrGraph& graph,
                                            std::int32_t source,
                                            const std::vector<std::int32_t>& distances)
{
    if (distances.size() != static_cast<std::size_t>(graph.node_count))
    {
        throw std::runtime_error("BFS validation distance vector has wrong size");
    }

    const auto reference = bfs_cpu_reference(graph, source);
    BfsValidationSummary summary;

    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const auto actual = distances[static_cast<std::size_t>(node)];
        const auto expected = reference[static_cast<std::size_t>(node)];
        summary.checksum += static_cast<std::int64_t>(actual) * static_cast<std::int64_t>(node + 1);
        summary.reference_checksum += static_cast<std::int64_t>(expected) * static_cast<std::int64_t>(node + 1);
        if (actual != expected)
        {
            ++summary.mismatch_count;
            summary.correct = false;
        }
        if (expected != kUnreachedDistance)
        {
            ++summary.reached_count;
            summary.max_distance = std::max(summary.max_distance, expected);
        }
    }

    return summary;
}

} // namespace algobench::graph_bfs
