#include "graph/graph_foundation.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace algobench::graph
{

namespace
{

void require_non_negative(const char* name, std::int32_t value)
{
    if (value < 0)
    {
        throw std::invalid_argument(std::string(name) + " must be non-negative");
    }
}

std::int32_t checked_product(std::int32_t lhs, std::int32_t rhs, const char* name)
{
    const auto product = static_cast<std::int64_t>(lhs) * static_cast<std::int64_t>(rhs);
    if (product > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
    {
        throw std::overflow_error(std::string(name) + " exceeds int32 node-id range");
    }
    return static_cast<std::int32_t>(product);
}

std::int32_t grid_node(std::int32_t x, std::int32_t y, std::int32_t width)
{
    return y * width + x;
}

std::uint32_t next_lcg(std::uint32_t value)
{
    return value * 1664525u + 1013904223u;
}

} // namespace

std::int64_t CsrGraph::edge_count() const
{
    return static_cast<std::int64_t>(column_indices.size());
}

std::int64_t CsrGraph::out_degree(std::int32_t node) const
{
    if (node < 0 || node >= node_count)
    {
        throw std::out_of_range("CSR node id is out of range");
    }

    if (row_offsets.size() != static_cast<std::size_t>(node_count) + 1u)
    {
        throw std::runtime_error("CSR row_offsets size is inconsistent with node_count");
    }

    return row_offsets[static_cast<std::size_t>(node + 1)] -
           row_offsets[static_cast<std::size_t>(node)];
}

CsrGraph build_csr_graph(std::int32_t node_count,
                         std::vector<DirectedEdge> edges,
                         bool remove_duplicate_edges)
{
    require_non_negative("node_count", node_count);

    for (const auto& edge : edges)
    {
        if (edge.source < 0 || edge.source >= node_count)
        {
            throw std::out_of_range("directed edge source is out of range");
        }
        if (edge.target < 0 || edge.target >= node_count)
        {
            throw std::out_of_range("directed edge target is out of range");
        }
    }

    std::sort(edges.begin(), edges.end(), [](const DirectedEdge& lhs, const DirectedEdge& rhs) {
        if (lhs.source != rhs.source)
        {
            return lhs.source < rhs.source;
        }
        return lhs.target < rhs.target;
    });

    if (remove_duplicate_edges)
    {
        edges.erase(std::unique(edges.begin(), edges.end(), [](const DirectedEdge& lhs, const DirectedEdge& rhs) {
                        return lhs.source == rhs.source && lhs.target == rhs.target;
                    }),
                    edges.end());
    }

    CsrGraph graph;
    graph.node_count = node_count;
    graph.row_offsets.assign(static_cast<std::size_t>(node_count) + 1u, 0);
    graph.column_indices.resize(edges.size());

    for (const auto& edge : edges)
    {
        ++graph.row_offsets[static_cast<std::size_t>(edge.source + 1)];
    }

    for (std::size_t offset = 1; offset < graph.row_offsets.size(); ++offset)
    {
        graph.row_offsets[offset] += graph.row_offsets[offset - 1];
    }

    for (std::size_t index = 0; index < edges.size(); ++index)
    {
        graph.column_indices[index] = edges[index].target;
    }

    const auto report = validate_csr_graph(graph);
    if (!report.valid)
    {
        throw std::runtime_error("internal CSR build failure: " + report.message);
    }

    return graph;
}

GraphValidationReport validate_csr_graph(const CsrGraph& graph)
{
    GraphValidationReport report;

    if (graph.node_count < 0)
    {
        report.valid = false;
        report.message = "node_count must be non-negative";
        return report;
    }

    const auto expected_row_count = static_cast<std::size_t>(graph.node_count) + 1u;
    if (graph.row_offsets.size() != expected_row_count)
    {
        report.valid = false;
        report.message = "row_offsets size must equal node_count + 1";
        return report;
    }

    if (graph.row_offsets.empty())
    {
        report.valid = false;
        report.message = "row_offsets must contain at least the zero sentinel";
        return report;
    }

    if (graph.row_offsets.front() != 0)
    {
        report.valid = false;
        report.message = "row_offsets[0] must be zero";
        return report;
    }

    const auto edge_count = static_cast<std::int64_t>(graph.column_indices.size());
    for (std::size_t i = 0; i < graph.row_offsets.size(); ++i)
    {
        const auto offset = graph.row_offsets[i];
        if (offset < 0 || offset > edge_count)
        {
            report.valid = false;
            report.message = "row offset is outside the column_indices range";
            return report;
        }
        if (i > 0 && offset < graph.row_offsets[i - 1])
        {
            report.valid = false;
            report.message = "row_offsets must be non-decreasing";
            return report;
        }
    }

    if (graph.row_offsets.back() != edge_count)
    {
        report.valid = false;
        report.message = "last row offset must equal edge_count";
        return report;
    }

    for (const auto target : graph.column_indices)
    {
        if (target < 0 || target >= graph.node_count)
        {
            report.valid = false;
            report.message = "column index target is outside the node range";
            return report;
        }
    }

    return report;
}

GraphStats summarize_graph(const CsrGraph& graph)
{
    const auto report = validate_csr_graph(graph);
    if (!report.valid)
    {
        throw std::runtime_error("cannot summarize invalid CSR graph: " + report.message);
    }

    GraphStats stats;
    stats.node_count = graph.node_count;
    stats.edge_count = graph.edge_count();

    if (graph.node_count == 0)
    {
        return stats;
    }

    stats.min_out_degree = std::numeric_limits<std::int64_t>::max();
    std::int64_t degree_sum = 0;

    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const auto degree = graph.out_degree(node);
        degree_sum += degree;
        stats.min_out_degree = std::min(stats.min_out_degree, degree);
        stats.max_out_degree = std::max(stats.max_out_degree, degree);
        if (degree == 0)
        {
            ++stats.zero_out_degree_count;
        }
    }

    stats.mean_out_degree = static_cast<double>(degree_sum) / static_cast<double>(graph.node_count);
    return stats;
}

CsrGraph make_chain_graph(std::int32_t node_count, bool bidirectional)
{
    require_non_negative("node_count", node_count);

    std::vector<DirectedEdge> edges;
    if (node_count > 1)
    {
        const auto one_way_count = static_cast<std::size_t>(node_count - 1);
        edges.reserve(bidirectional ? one_way_count * 2u : one_way_count);
    }

    for (std::int32_t node = 0; node + 1 < node_count; ++node)
    {
        edges.push_back({node, static_cast<std::int32_t>(node + 1)});
        if (bidirectional)
        {
            edges.push_back({static_cast<std::int32_t>(node + 1), node});
        }
    }

    return build_csr_graph(node_count, std::move(edges));
}

CsrGraph make_grid_graph(std::int32_t width, std::int32_t height, bool bidirectional)
{
    require_non_negative("width", width);
    require_non_negative("height", height);

    const auto node_count = checked_product(width, height, "grid node count");
    std::vector<DirectedEdge> edges;

    if (width > 0 && height > 0)
    {
        const auto horizontal_edges = static_cast<std::int64_t>(std::max(0, width - 1)) * height;
        const auto vertical_edges = static_cast<std::int64_t>(std::max(0, height - 1)) * width;
        const auto logical_edges = horizontal_edges + vertical_edges;
        const auto adjacency_entries = bidirectional ? logical_edges * 2 : logical_edges;
        edges.reserve(static_cast<std::size_t>(adjacency_entries));
    }

    for (std::int32_t y = 0; y < height; ++y)
    {
        for (std::int32_t x = 0; x < width; ++x)
        {
            const auto source = grid_node(x, y, width);
            if (x + 1 < width)
            {
                const auto right = grid_node(static_cast<std::int32_t>(x + 1), y, width);
                edges.push_back({source, right});
                if (bidirectional)
                {
                    edges.push_back({right, source});
                }
            }
            if (y + 1 < height)
            {
                const auto down = grid_node(x, static_cast<std::int32_t>(y + 1), width);
                edges.push_back({source, down});
                if (bidirectional)
                {
                    edges.push_back({down, source});
                }
            }
        }
    }

    return build_csr_graph(node_count, std::move(edges));
}

CsrGraph make_layered_graph(std::int32_t layer_count,
                            std::int32_t nodes_per_layer,
                            std::int32_t fanout)
{
    require_non_negative("layer_count", layer_count);
    require_non_negative("nodes_per_layer", nodes_per_layer);
    require_non_negative("fanout", fanout);

    if (nodes_per_layer == 0 && fanout != 0)
    {
        throw std::invalid_argument("fanout must be zero when nodes_per_layer is zero");
    }
    if (nodes_per_layer > 0 && fanout > nodes_per_layer)
    {
        throw std::invalid_argument("fanout cannot exceed nodes_per_layer for unique targets");
    }

    const auto node_count = checked_product(layer_count, nodes_per_layer, "layered graph node count");
    std::vector<DirectedEdge> edges;

    if (layer_count > 1 && nodes_per_layer > 0 && fanout > 0)
    {
        const auto adjacency_entries = static_cast<std::int64_t>(layer_count - 1) *
                                       nodes_per_layer *
                                       fanout;
        edges.reserve(static_cast<std::size_t>(adjacency_entries));
    }

    for (std::int32_t layer = 0; layer + 1 < layer_count; ++layer)
    {
        const auto source_layer_offset = layer * nodes_per_layer;
        const auto target_layer_offset = static_cast<std::int32_t>(layer + 1) * nodes_per_layer;

        for (std::int32_t position = 0; position < nodes_per_layer; ++position)
        {
            const auto source = source_layer_offset + position;
            for (std::int32_t k = 0; k < fanout; ++k)
            {
                const auto target_position = static_cast<std::int32_t>((position + layer + k) % nodes_per_layer);
                const auto target = target_layer_offset + target_position;
                edges.push_back({source, target});
            }
        }
    }

    return build_csr_graph(node_count, std::move(edges));
}

CsrGraph make_random_sparse_graph(std::int32_t node_count,
                                  std::int32_t out_degree,
                                  std::uint32_t seed)
{
    require_non_negative("node_count", node_count);
    require_non_negative("out_degree", out_degree);

    if (node_count == 0)
    {
        if (out_degree != 0)
        {
            throw std::invalid_argument("out_degree must be zero for an empty graph");
        }
        return build_csr_graph(0, {});
    }

    const auto max_without_self_loop = std::max(0, node_count - 1);
    if (out_degree > max_without_self_loop)
    {
        throw std::invalid_argument("out_degree exceeds the number of distinct non-self targets");
    }

    std::vector<DirectedEdge> edges;
    edges.reserve(static_cast<std::size_t>(node_count) * static_cast<std::size_t>(out_degree));

    for (std::int32_t source = 0; source < node_count; ++source)
    {
        std::unordered_set<std::int32_t> targets;
        targets.reserve(static_cast<std::size_t>(out_degree) * 2u + 1u);

        std::uint32_t state = seed ^ (0x9E3779B9u * static_cast<std::uint32_t>(source + 1));
        while (static_cast<std::int32_t>(targets.size()) < out_degree)
        {
            state = next_lcg(state);
            const auto candidate = static_cast<std::int32_t>(state % static_cast<std::uint32_t>(node_count));
            if (candidate == source)
            {
                continue;
            }
            targets.insert(candidate);
        }

        for (const auto target : targets)
        {
            edges.push_back({source, target});
        }
    }

    return build_csr_graph(node_count, std::move(edges));
}

} // namespace algobench::graph
