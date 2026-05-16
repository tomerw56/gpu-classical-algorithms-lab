#include "graph/graph_bfs.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <string>
#include <vector>

int main()
{
    using namespace algobench;
    using namespace algobench::graph_bfs;

    {
        const auto graph = algobench::graph::make_chain_graph(5, true);
        const auto distances = bfs_cpu_reference(graph, 0);
        const std::vector<std::int32_t> expected{0, 1, 2, 3, 4};
        TEST_CHECK_EQ(distances.size(), expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            TEST_CHECK_EQ(distances[i], expected[i]);
        }
    }

    {
        const auto graph = algobench::graph::make_grid_graph(3, 2, true);
        const auto distances = bfs_cpu_reference(graph, 0);
        TEST_CHECK_EQ(distances.size(), static_cast<std::size_t>(6));
        TEST_CHECK_EQ(distances[0], 0);
        TEST_CHECK_EQ(distances[1], 1);
        TEST_CHECK_EQ(distances[3], 1);
        TEST_CHECK_EQ(distances[5], 3);
    }

    {
        const auto graph = algobench::graph::build_csr_graph(4, {{0, 1}});
        const auto distances = bfs_cpu_reference(graph, 0);
        TEST_CHECK_EQ(distances[0], 0);
        TEST_CHECK_EQ(distances[1], 1);
        TEST_CHECK_EQ(distances[2], kUnreachedDistance);
        TEST_CHECK_EQ(distances[3], kUnreachedDistance);

        const auto summary = validate_bfs_distances(graph, 0, distances);
        TEST_CHECK(summary.correct);
        TEST_CHECK_EQ(summary.reached_count, static_cast<std::int64_t>(2));
        TEST_CHECK_EQ(summary.mismatch_count, static_cast<std::int64_t>(0));
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.repeat = 1;
        config.warmup = 0;
        config.include_gpu = false;
        config.params["graph"] = "chain";
        config.params["chain_nodes"] = "32";

        const auto results = run_graph_bfs_cpu(config);
        TEST_CHECK_EQ(results.size(), static_cast<std::size_t>(1));
        TEST_CHECK_EQ(results.front().benchmark, std::string("graph_bfs"));
        TEST_CHECK_EQ(results.front().variant, std::string("cpu"));
        TEST_CHECK(results.front().correct);
        TEST_CHECK_EQ(results.front().input_size.at("nodes"), static_cast<std::int64_t>(32));
        TEST_CHECK_EQ(results.front().metadata.at("graph_kind"), std::string("chain"));
        TEST_CHECK_EQ(results.front().metadata.at("mismatch_count"), std::string("0"));
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.repeat = 1;
        config.warmup = 0;
        config.include_gpu = false;
        config.params["graph"] = "layered";
        config.params["layers"] = "4";
        config.params["nodes_per_layer"] = "8";
        config.params["fanout"] = "3";
        config.params["sweep_label"] = "layered_demo_point";

        const auto results = run_graph_bfs_cpu(config);
        TEST_CHECK_EQ(results.size(), static_cast<std::size_t>(1));
        TEST_CHECK_EQ(results.front().preset, std::string("layered_demo_point"));
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.repeat = 1;
        config.warmup = 0;
        config.include_gpu = false;
        config.params["graph"] = "layered";
        config.params["layers"] = "4";
        config.params["nodes_per_layer"] = "8";
        config.params["fanout"] = "3";

        const auto shape = shape_for_config(config);
        TEST_CHECK_EQ(shape.graph_kind, std::string("layered"));
        const auto graph = make_graph_for_shape(shape);
        TEST_CHECK_EQ(graph.node_count, static_cast<std::int32_t>(32));
        TEST_CHECK(graph.edge_count() > 0);
    }

    TEST_CHECK_THROWS((void)bfs_cpu_reference(algobench::graph::make_chain_graph(3, true), 10));

    return algobench::test::finish();
}
