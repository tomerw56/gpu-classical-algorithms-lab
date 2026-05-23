#include "graph/graph_weighted_relaxation.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace algobench;
using namespace algobench::graph_weighted;

int main()
{
    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.params["graph"] = "chain";
        config.params["chain_nodes"] = "6";
        config.repeat = 1;
        config.warmup = 0;

        const auto shape = weighted_shape_for_config(config);
        const auto weighted = make_weighted_graph_for_shape(shape);
        TEST_CHECK_EQ(weighted.graph.node_count, 6);
        TEST_CHECK_EQ(weighted.graph.edge_count(), 10);
        TEST_CHECK_EQ(weighted.weights.size(), weighted.graph.column_indices.size());
        TEST_CHECK(std::all_of(weighted.weights.begin(), weighted.weights.end(), [](std::int32_t w) { return w > 0; }));
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.params["graph"] = "grid";
        config.params["grid_width"] = "3";
        config.params["grid_height"] = "2";

        const auto shape = weighted_shape_for_config(config);
        const auto weighted = make_weighted_graph_for_shape(shape);
        const auto distances = dijkstra_cpu_reference(weighted, shape.source);
        const auto summary = validate_weighted_distances(weighted, shape.source, distances);
        TEST_CHECK(summary.correct);
        TEST_CHECK_EQ(summary.mismatch_count, 0);
        TEST_CHECK_EQ(summary.reached_count, weighted.graph.node_count);
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.params["graph"] = "layered";
        config.params["layers"] = "4";
        config.params["nodes_per_layer"] = "8";
        config.params["fanout"] = "3";

        const auto shape = weighted_shape_for_config(config);
        const auto weighted = make_weighted_graph_for_shape(shape);
        const auto distances = dijkstra_cpu_reference(weighted, shape.source);
        const auto summary = validate_weighted_distances(weighted, shape.source, distances);
        TEST_CHECK(summary.correct);
        TEST_CHECK(summary.reached_count > 0);
        TEST_CHECK(summary.max_finite_distance >= 0);
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.params["graph"] = "random";
        config.params["random_nodes"] = "128";
        config.params["random_out_degree"] = "4";
        config.repeat = 1;
        config.warmup = 0;

        const auto results = run_graph_weighted_relaxation_cpu(config);
        TEST_CHECK_EQ(results.size(), 1u);
        TEST_CHECK_EQ(results[0].benchmark, std::string("graph_weighted_relaxation"));
        TEST_CHECK_EQ(results[0].variant, std::string("cpu"));
        TEST_CHECK(results[0].correct);
        TEST_CHECK(results[0].metadata.count("graph_kind") == 1);
        TEST_CHECK(results[0].metadata.count("max_finite_distance") == 1);
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.params["graph"] = "chain";
        config.params["chain_nodes"] = "6";

        const auto shape = weighted_shape_for_config(config);
        const auto weighted = make_weighted_graph_for_shape(shape);

        // Chain relaxation needs one extra no-change pass after the last useful
        // distance update to mark convergence. For N=6, the longest shortest
        // path has 5 edges, so the default guard should be at least 6.
        TEST_CHECK(weighted_default_max_iterations(shape, weighted) >= weighted.graph.node_count);
    }

    return algobench::test::finish();
}
