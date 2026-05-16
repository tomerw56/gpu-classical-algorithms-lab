#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace algobench::graph
{

/**
 * One directed adjacency entry used when building a CSR graph.
 *
 * The graph foundation stores adjacency entries as directed edges. An
 * undirected logical edge is represented by two entries: `u -> v` and
 * `v -> u`.
 */
struct DirectedEdge
{
    std::int32_t source = 0;
    std::int32_t target = 0;
};

/**
 * Compressed Sparse Row graph representation.
 *
 * For node `u`, outgoing neighbors occupy:
 *
 *     column_indices[row_offsets[u] ... row_offsets[u + 1])
 *
 * `row_offsets` therefore has `node_count + 1` values and the last offset is
 * exactly the number of stored directed adjacency entries.
 */
struct CsrGraph
{
    std::int32_t node_count = 0;
    std::vector<std::int64_t> row_offsets;
    std::vector<std::int32_t> column_indices;

    /** Return the number of stored directed adjacency entries. */
    std::int64_t edge_count() const;

    /** Return the outgoing degree of one node, or throw for an invalid node id. */
    std::int64_t out_degree(std::int32_t node) const;
};

/**
 * Validation result for CSR invariants.
 *
 * `message` is kept human-readable so tests and future exporters can explain
 * malformed graph state instead of returning only `false`.
 */
struct GraphValidationReport
{
    bool valid = true;
    std::string message = "ok";
};

/** Simple graph summary used by tests, docs, and future benchmark metadata. */
struct GraphStats
{
    std::int32_t node_count = 0;
    std::int64_t edge_count = 0;
    std::int64_t min_out_degree = 0;
    std::int64_t max_out_degree = 0;
    double mean_out_degree = 0.0;
    std::int64_t zero_out_degree_count = 0;
};

/**
 * Build a deterministic CSR graph from directed edges.
 *
 * The builder validates source/target ids, sorts adjacency entries by
 * `(source, target)`, and removes duplicate directed edges by default. Keeping
 * this builder centralized avoids each later graph benchmark inventing its own
 * half-compatible CSR format.
 */
CsrGraph build_csr_graph(std::int32_t node_count,
                         std::vector<DirectedEdge> edges,
                         bool remove_duplicate_edges = true);

/** Validate structural CSR invariants without modifying the graph. */
GraphValidationReport validate_csr_graph(const CsrGraph& graph);

/** Summarize degree statistics. Throws if the CSR graph is structurally invalid. */
GraphStats summarize_graph(const CsrGraph& graph);

/**
 * Build a path graph `0 - 1 - 2 - ...`.
 *
 * With `bidirectional=true`, every logical path edge is emitted in both
 * directions. With `bidirectional=false`, only `i -> i + 1` entries are stored.
 */
CsrGraph make_chain_graph(std::int32_t node_count, bool bidirectional = true);

/**
 * Build a rectangular 4-neighbor grid graph.
 *
 * Node ids use row-major layout:
 *
 *     node_id = y * width + x
 *
 * With `bidirectional=true`, left/right and up/down motion is stored in both
 * directions. With `false`, only rightward and downward edges are emitted.
 */
CsrGraph make_grid_graph(std::int32_t width, std::int32_t height, bool bidirectional = true);

/**
 * Build a directed layered graph designed to create wide BFS frontiers.
 *
 * Each node in layer `L` connects to `fanout` deterministic nodes in layer
 * `L + 1`. The last layer has no outgoing edges. This graph shape will be useful
 * when Phase 3.2 compares CPU queue BFS with GPU frontier BFS.
 */
CsrGraph make_layered_graph(std::int32_t layer_count,
                            std::int32_t nodes_per_layer,
                            std::int32_t fanout);

/**
 * Build a deterministic directed sparse graph with a fixed out-degree.
 *
 * Self-loops are avoided. Every node receives exactly `out_degree` distinct
 * outgoing neighbors, so the total adjacency-entry count is
 * `node_count * out_degree` when the arguments are valid.
 */
CsrGraph make_random_sparse_graph(std::int32_t node_count,
                                  std::int32_t out_degree,
                                  std::uint32_t seed = 0xC001CAFEu);

} // namespace algobench::graph
