#include "graph/graph_connected_components.hpp"

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace algobench::graph_cc
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

void add_bidirectional_edge(std::vector<graph::DirectedEdge>& edges, std::int32_t a, std::int32_t b)
{
    if (a == b)
    {
        return;
    }
    edges.push_back({a, b});
    edges.push_back({b, a});
}

std::uint32_t next_lcg(std::uint32_t value)
{
    return value * 1664525u + 1013904223u;
}

std::int32_t checked_node_count(std::int32_t component_count, std::int32_t component_size)
{
    const auto product = static_cast<std::int64_t>(component_count) * static_cast<std::int64_t>(component_size);
    if (product > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
    {
        throw std::overflow_error("connected-components graph node count exceeds int32 range");
    }
    return static_cast<std::int32_t>(product);
}

std::vector<graph::DirectedEdge> make_chain_components_edges(std::int32_t component_count, std::int32_t component_size)
{
    std::vector<graph::DirectedEdge> edges;
    if (component_size > 1)
    {
        edges.reserve(static_cast<std::size_t>(component_count) * static_cast<std::size_t>(component_size - 1) * 2u);
    }

    for (std::int32_t component = 0; component < component_count; ++component)
    {
        const auto base = component * component_size;
        for (std::int32_t local = 0; local + 1 < component_size; ++local)
        {
            add_bidirectional_edge(edges, base + local, base + local + 1);
        }
    }
    return edges;
}

std::vector<graph::DirectedEdge> make_grid_components_edges(std::int32_t component_count,
                                                            std::int32_t width,
                                                            std::int32_t height)
{
    const auto component_size = width * height;
    std::vector<graph::DirectedEdge> edges;
    const auto logical_edges_per_grid = static_cast<std::int64_t>(std::max(0, width - 1)) * height +
                                        static_cast<std::int64_t>(std::max(0, height - 1)) * width;
    edges.reserve(static_cast<std::size_t>(logical_edges_per_grid * 2 * component_count));

    for (std::int32_t component = 0; component < component_count; ++component)
    {
        const auto base = component * component_size;
        for (std::int32_t y = 0; y < height; ++y)
        {
            for (std::int32_t x = 0; x < width; ++x)
            {
                const auto node = base + y * width + x;
                if (x + 1 < width)
                {
                    add_bidirectional_edge(edges, node, node + 1);
                }
                if (y + 1 < height)
                {
                    add_bidirectional_edge(edges, node, node + width);
                }
            }
        }
    }
    return edges;
}

std::vector<graph::DirectedEdge> make_random_cluster_edges(std::int32_t component_count,
                                                           std::int32_t component_size,
                                                           std::int32_t out_degree,
                                                           std::uint32_t seed)
{
    if (component_size == 0)
    {
        return {};
    }
    if (component_size == 1)
    {
        if (out_degree != 0)
        {
            throw std::invalid_argument("random_out_degree must be zero when component_size is one");
        }
        return {};
    }
    if (out_degree > component_size - 1)
    {
        throw std::invalid_argument("random_out_degree cannot exceed component_size - 1");
    }

    std::vector<graph::DirectedEdge> edges;
    edges.reserve(static_cast<std::size_t>(component_count) * static_cast<std::size_t>(component_size) *
                  static_cast<std::size_t>(std::max(1, out_degree)) * 2u);

    for (std::int32_t component = 0; component < component_count; ++component)
    {
        const auto base = component * component_size;

        // A backbone guarantees each cluster is one connected component even
        // when random_out_degree is small.
        for (std::int32_t local = 0; local + 1 < component_size; ++local)
        {
            add_bidirectional_edge(edges, base + local, base + local + 1);
        }

        if (out_degree == 0)
        {
            continue;
        }

        for (std::int32_t local = 0; local < component_size; ++local)
        {
            std::unordered_set<std::int32_t> targets;
            targets.reserve(static_cast<std::size_t>(out_degree) * 2u + 1u);
            std::uint32_t state = seed ^ (0x9E3779B9u * static_cast<std::uint32_t>(component + 1)) ^
                                  (0x85EBCA6Bu * static_cast<std::uint32_t>(local + 1));
            while (static_cast<std::int32_t>(targets.size()) < out_degree)
            {
                state = next_lcg(state);
                const auto candidate = static_cast<std::int32_t>(state % static_cast<std::uint32_t>(component_size));
                if (candidate == local)
                {
                    continue;
                }
                targets.insert(candidate);
            }
            for (const auto target : targets)
            {
                add_bidirectional_edge(edges, base + local, base + target);
            }
        }
    }
    return edges;
}

std::vector<graph::DirectedEdge> make_mixed_components_edges(std::int32_t component_count, std::int32_t component_size)
{
    std::vector<graph::DirectedEdge> edges;
    edges.reserve(static_cast<std::size_t>(component_count) * static_cast<std::size_t>(component_size) * 4u);

    for (std::int32_t component = 0; component < component_count; ++component)
    {
        const auto base = component * component_size;
        const auto size = std::max<std::int32_t>(1, component_size - (component % 5));
        if (size <= 1)
        {
            continue;
        }

        if (component % 3 == 0)
        {
            for (std::int32_t local = 0; local + 1 < size; ++local)
            {
                add_bidirectional_edge(edges, base + local, base + local + 1);
            }
        }
        else if (component % 3 == 1)
        {
            for (std::int32_t local = 1; local < size; ++local)
            {
                add_bidirectional_edge(edges, base, base + local);
            }
        }
        else
        {
            for (std::int32_t local = 0; local < size; ++local)
            {
                add_bidirectional_edge(edges, base + local, base + ((local + 1) % size));
                if (local + 2 < size)
                {
                    add_bidirectional_edge(edges, base + local, base + local + 2);
                }
            }
        }
    }
    return edges;
}

class UnionFind
{
public:
    explicit UnionFind(std::int32_t count) : parent_(static_cast<std::size_t>(count)), size_(static_cast<std::size_t>(count), 1)
    {
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    std::int32_t find(std::int32_t value)
    {
        auto root = value;
        while (parent_[static_cast<std::size_t>(root)] != root)
        {
            root = parent_[static_cast<std::size_t>(root)];
        }
        while (parent_[static_cast<std::size_t>(value)] != value)
        {
            const auto next = parent_[static_cast<std::size_t>(value)];
            parent_[static_cast<std::size_t>(value)] = root;
            value = next;
        }
        return root;
    }

    void unite(std::int32_t a, std::int32_t b)
    {
        auto root_a = find(a);
        auto root_b = find(b);
        if (root_a == root_b)
        {
            return;
        }

        // Keep the smaller node id as the canonical representative. This makes
        // result labels deterministic and easy to validate/plot.
        if (root_b < root_a)
        {
            std::swap(root_a, root_b);
        }
        parent_[static_cast<std::size_t>(root_b)] = root_a;
        size_[static_cast<std::size_t>(root_a)] += size_[static_cast<std::size_t>(root_b)];
    }

private:
    std::vector<std::int32_t> parent_;
    std::vector<std::int32_t> size_;
};

std::int64_t checksum_labels(const std::vector<std::int32_t>& labels)
{
    std::int64_t checksum = 0;
    for (std::size_t i = 0; i < labels.size(); ++i)
    {
        checksum += static_cast<std::int64_t>(i + 1u) * static_cast<std::int64_t>(labels[i] + 1);
    }
    return checksum;
}

} // namespace

GraphCcShape cc_shape_for_config(const BenchmarkConfig& config)
{
    GraphCcShape shape;
    shape.graph_kind = get_param_or(config, "graph", "random_clusters");

    if (config.preset == "tiny")
    {
        shape.component_count = 4;
        shape.component_size = 32;
        shape.grid_width = 8;
        shape.grid_height = 8;
        shape.random_out_degree = 3;
    }
    else if (config.preset == "small")
    {
        shape.component_count = 64;
        shape.component_size = 512;
        shape.grid_width = 32;
        shape.grid_height = 32;
        shape.random_out_degree = 4;
    }
    else if (config.preset == "medium")
    {
        shape.component_count = 128;
        shape.component_size = 2048;
        shape.grid_width = 64;
        shape.grid_height = 64;
        shape.random_out_degree = 4;
    }
    else if (config.preset == "large")
    {
        shape.component_count = 256;
        shape.component_size = 4096;
        shape.grid_width = 96;
        shape.grid_height = 96;
        shape.random_out_degree = 4;
    }
    else
    {
        throw std::runtime_error("unknown preset for graph_connected_components: " + config.preset);
    }

    shape.component_count = get_int_param_or(config, "components", shape.component_count);
    shape.component_size = get_int_param_or(config, "component_size", shape.component_size);
    shape.grid_width = get_int_param_or(config, "grid_width", shape.grid_width);
    shape.grid_height = get_int_param_or(config, "grid_height", shape.grid_height);
    shape.random_out_degree = get_int_param_or(config, "random_out_degree", shape.random_out_degree);
    shape.random_seed = get_uint_param_or(config, "seed", shape.random_seed);
    shape.max_iterations = get_int_param_or(config, "max_iterations", shape.max_iterations);

    return shape;
}

graph::CsrGraph make_cc_graph_for_shape(const GraphCcShape& shape)
{
    if (shape.component_count < 0 || shape.component_size < 0 || shape.grid_width < 0 || shape.grid_height < 0)
    {
        throw std::invalid_argument("connected-components shape dimensions must be non-negative");
    }

    if (shape.graph_kind == "chains" || shape.graph_kind == "chain_components")
    {
        const auto node_count = checked_node_count(shape.component_count, shape.component_size);
        return graph::build_csr_graph(node_count, make_chain_components_edges(shape.component_count, shape.component_size));
    }
    if (shape.graph_kind == "grid_components" || shape.graph_kind == "grids")
    {
        const auto component_size = shape.grid_width * shape.grid_height;
        const auto node_count = checked_node_count(shape.component_count, component_size);
        return graph::build_csr_graph(node_count, make_grid_components_edges(shape.component_count, shape.grid_width, shape.grid_height));
    }
    if (shape.graph_kind == "random_clusters" || shape.graph_kind == "random")
    {
        const auto node_count = checked_node_count(shape.component_count, shape.component_size);
        return graph::build_csr_graph(node_count,
                                      make_random_cluster_edges(shape.component_count,
                                                                shape.component_size,
                                                                shape.random_out_degree,
                                                                shape.random_seed));
    }
    if (shape.graph_kind == "mixed")
    {
        const auto node_count = checked_node_count(shape.component_count, shape.component_size);
        return graph::build_csr_graph(node_count, make_mixed_components_edges(shape.component_count, shape.component_size));
    }

    throw std::runtime_error("unknown graph_connected_components graph kind: " + shape.graph_kind);
}

std::string describe_cc_shape(const GraphCcShape& shape)
{
    if (shape.graph_kind == "grid_components" || shape.graph_kind == "grids")
    {
        return "components=" + std::to_string(shape.component_count) + ", grid=" +
               std::to_string(shape.grid_width) + "x" + std::to_string(shape.grid_height);
    }
    if (shape.graph_kind == "random_clusters" || shape.graph_kind == "random")
    {
        return "components=" + std::to_string(shape.component_count) + ", component_size=" +
               std::to_string(shape.component_size) + ", random_out_degree=" +
               std::to_string(shape.random_out_degree) + ", seed=" + std::to_string(shape.random_seed);
    }
    return "components=" + std::to_string(shape.component_count) + ", component_size=" +
           std::to_string(shape.component_size);
}

std::string cc_result_preset_label_for_config(const BenchmarkConfig& config)
{
    const auto it = config.params.find("sweep_label");
    if (it != config.params.end() && !it->second.empty())
    {
        return it->second;
    }
    return config.preset;
}

std::vector<std::int32_t> connected_components_cpu_reference(const graph::CsrGraph& graph)
{
    const auto report = graph::validate_csr_graph(graph);
    if (!report.valid)
    {
        throw std::runtime_error("cannot run connected components over invalid CSR graph: " + report.message);
    }

    UnionFind uf(graph.node_count);
    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const auto begin = graph.row_offsets[static_cast<std::size_t>(node)];
        const auto end = graph.row_offsets[static_cast<std::size_t>(node + 1)];
        for (auto edge = begin; edge < end; ++edge)
        {
            uf.unite(node, graph.column_indices[static_cast<std::size_t>(edge)]);
        }
    }

    std::vector<std::int32_t> labels(static_cast<std::size_t>(graph.node_count));
    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        labels[static_cast<std::size_t>(node)] = uf.find(node);
    }
    return labels;
}

std::vector<std::int32_t> normalize_component_labels(const std::vector<std::int32_t>& labels)
{
    std::unordered_map<std::int32_t, std::int32_t> min_node_by_label;
    for (std::int32_t node = 0; node < static_cast<std::int32_t>(labels.size()); ++node)
    {
        const auto label = labels[static_cast<std::size_t>(node)];
        const auto it = min_node_by_label.find(label);
        if (it == min_node_by_label.end() || node < it->second)
        {
            min_node_by_label[label] = node;
        }
    }

    std::vector<std::int32_t> normalized(labels.size());
    for (std::int32_t node = 0; node < static_cast<std::int32_t>(labels.size()); ++node)
    {
        normalized[static_cast<std::size_t>(node)] = min_node_by_label[labels[static_cast<std::size_t>(node)]];
    }
    return normalized;
}

ConnectedComponentsSummary validate_component_labels(const graph::CsrGraph& graph,
                                                     const std::vector<std::int32_t>& labels)
{
    if (labels.size() != static_cast<std::size_t>(graph.node_count))
    {
        throw std::runtime_error("component-label vector size does not match graph node count");
    }

    const auto reference = connected_components_cpu_reference(graph);
    const auto normalized = normalize_component_labels(labels);

    ConnectedComponentsSummary summary;
    summary.checksum = checksum_labels(normalized);
    summary.reference_checksum = checksum_labels(reference);

    std::unordered_map<std::int32_t, std::int64_t> sizes;
    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const auto index = static_cast<std::size_t>(node);
        if (normalized[index] != reference[index])
        {
            ++summary.mismatch_count;
        }
        ++sizes[reference[index]];
    }

    summary.correct = summary.mismatch_count == 0;
    summary.component_count = static_cast<std::int32_t>(sizes.size());
    for (const auto& entry : sizes)
    {
        summary.largest_component_size = std::max(summary.largest_component_size, entry.second);
        if (entry.second == 1)
        {
            ++summary.isolated_node_count;
        }
    }

    return summary;
}

} // namespace algobench::graph_cc
