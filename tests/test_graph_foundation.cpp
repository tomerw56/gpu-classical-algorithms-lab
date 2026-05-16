#include "graph/graph_foundation.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <vector>

int main()
{
    using namespace algobench::graph;

    // ---------------------------------------------------------------------
    // CSR builder and validation.
    // ---------------------------------------------------------------------
    {
        std::vector<DirectedEdge> edges{
            {0, 2},
            {0, 1},
            {0, 1}, // duplicate; the builder removes it by default
            {1, 2},
        };

        const auto graph = build_csr_graph(3, edges);
        const auto report = validate_csr_graph(graph);
        TEST_CHECK(report.valid);
        TEST_CHECK_EQ(graph.node_count, 3);
        TEST_CHECK_EQ(graph.edge_count(), 3);
        TEST_CHECK_EQ(graph.row_offsets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(graph.row_offsets[0], 0);
        TEST_CHECK_EQ(graph.row_offsets[1], 2);
        TEST_CHECK_EQ(graph.row_offsets[2], 3);
        TEST_CHECK_EQ(graph.row_offsets[3], 3);
        TEST_CHECK_EQ(graph.column_indices.size(), static_cast<std::size_t>(3));
        TEST_CHECK_EQ(graph.column_indices[0], 1);
        TEST_CHECK_EQ(graph.column_indices[1], 2);
        TEST_CHECK_EQ(graph.column_indices[2], 2);
        TEST_CHECK_EQ(graph.out_degree(0), 2);
        TEST_CHECK_EQ(graph.out_degree(1), 1);
        TEST_CHECK_EQ(graph.out_degree(2), 0);
        TEST_CHECK_THROWS((void)graph.out_degree(3));
    }

    // Invalid builder inputs must fail early.
    TEST_CHECK_THROWS((void)build_csr_graph(-1, {}));
    TEST_CHECK_THROWS((void)build_csr_graph(2, std::vector<DirectedEdge>{{0, 2}}));
    TEST_CHECK_THROWS((void)build_csr_graph(2, std::vector<DirectedEdge>{{-1, 1}}));

    // Validation should describe malformed CSR shapes without throwing.
    {
        CsrGraph invalid;
        invalid.node_count = 2;
        invalid.row_offsets = {0, 1};
        invalid.column_indices = {1};
        const auto report = validate_csr_graph(invalid);
        TEST_CHECK(!report.valid);
    }

    // ---------------------------------------------------------------------
    // Chain graph: a deliberately narrow frontier shape for later BFS work.
    // ---------------------------------------------------------------------
    {
        const auto graph = make_chain_graph(5, true);
        const auto stats = summarize_graph(graph);
        TEST_CHECK(validate_csr_graph(graph).valid);
        TEST_CHECK_EQ(graph.node_count, 5);
        TEST_CHECK_EQ(graph.edge_count(), 8);
        TEST_CHECK_EQ(graph.out_degree(0), 1);
        TEST_CHECK_EQ(graph.out_degree(1), 2);
        TEST_CHECK_EQ(graph.out_degree(4), 1);
        TEST_CHECK_EQ(stats.min_out_degree, 1);
        TEST_CHECK_EQ(stats.max_out_degree, 2);
        TEST_CHECK_EQ(stats.zero_out_degree_count, 0);
    }

    {
        const auto graph = make_chain_graph(5, false);
        TEST_CHECK_EQ(graph.edge_count(), 4);
        TEST_CHECK_EQ(graph.out_degree(0), 1);
        TEST_CHECK_EQ(graph.out_degree(3), 1);
        TEST_CHECK_EQ(graph.out_degree(4), 0);
    }

    // ---------------------------------------------------------------------
    // 3x2 grid: a spatially structured graph with a predictable edge count.
    // Logical undirected edges = 7, stored directed entries = 14.
    // ---------------------------------------------------------------------
    {
        const auto graph = make_grid_graph(3, 2, true);
        const auto stats = summarize_graph(graph);
        TEST_CHECK(validate_csr_graph(graph).valid);
        TEST_CHECK_EQ(graph.node_count, 6);
        TEST_CHECK_EQ(graph.edge_count(), 14);
        TEST_CHECK_EQ(stats.min_out_degree, 2);
        TEST_CHECK_EQ(stats.max_out_degree, 3);
        TEST_CHECK_EQ(stats.zero_out_degree_count, 0);
    }

    TEST_CHECK_THROWS((void)make_grid_graph(-1, 2, true));
    TEST_CHECK_THROWS((void)make_grid_graph(2, -1, true));

    // ---------------------------------------------------------------------
    // Layered graph: wide deterministic frontiers for the future GPU BFS demo.
    // 3 layers, 4 nodes/layer, fanout 2 => 2 transitions * 4 * 2 = 16 entries.
    // ---------------------------------------------------------------------
    {
        const auto graph = make_layered_graph(3, 4, 2);
        const auto stats = summarize_graph(graph);
        TEST_CHECK(validate_csr_graph(graph).valid);
        TEST_CHECK_EQ(graph.node_count, 12);
        TEST_CHECK_EQ(graph.edge_count(), 16);
        TEST_CHECK_EQ(graph.out_degree(0), 2);
        TEST_CHECK_EQ(graph.out_degree(7), 2);
        TEST_CHECK_EQ(graph.out_degree(8), 0);
        TEST_CHECK_EQ(stats.zero_out_degree_count, 4);
    }

    TEST_CHECK_THROWS((void)make_layered_graph(3, 4, 5));
    TEST_CHECK_THROWS((void)make_layered_graph(3, 0, 1));

    // ---------------------------------------------------------------------
    // Random sparse graph: deterministic generation with exact out-degree.
    // ---------------------------------------------------------------------
    {
        const auto graph = make_random_sparse_graph(10, 3, 12345u);
        const auto stats = summarize_graph(graph);
        TEST_CHECK(validate_csr_graph(graph).valid);
        TEST_CHECK_EQ(graph.node_count, 10);
        TEST_CHECK_EQ(graph.edge_count(), 30);
        TEST_CHECK_EQ(stats.min_out_degree, 3);
        TEST_CHECK_EQ(stats.max_out_degree, 3);
        TEST_CHECK_EQ(stats.zero_out_degree_count, 0);

        for (std::int32_t node = 0; node < graph.node_count; ++node)
        {
            TEST_CHECK_EQ(graph.out_degree(node), 3);
            const auto begin = graph.row_offsets[static_cast<std::size_t>(node)];
            const auto end = graph.row_offsets[static_cast<std::size_t>(node + 1)];
            for (auto edge = begin; edge < end; ++edge)
            {
                TEST_CHECK(graph.column_indices[static_cast<std::size_t>(edge)] != node);
            }
        }
    }

    TEST_CHECK_THROWS((void)make_random_sparse_graph(0, 1, 7u));
    TEST_CHECK_THROWS((void)make_random_sparse_graph(4, 4, 7u));

    return algobench::test::finish();
}
