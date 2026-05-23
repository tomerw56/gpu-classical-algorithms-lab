#include "graph/graph_connected_components.hpp"

#include "test_support.hpp"

#include <cstdint>
#include <string>
#include <vector>

int main()
{
    using namespace algobench;
    using namespace algobench::graph_cc;

    {
        const auto graph = algobench::graph::build_csr_graph(
            6,
            {{0, 1}, {1, 0}, {1, 2}, {2, 1}, {3, 4}, {4, 3}});
        const auto labels = connected_components_cpu_reference(graph);
        const std::vector<std::int32_t> expected{0, 0, 0, 3, 3, 5};
        TEST_CHECK_EQ(labels.size(), expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            TEST_CHECK_EQ(labels[i], expected[i]);
        }

        const auto summary = validate_component_labels(graph, labels);
        TEST_CHECK(summary.correct);
        TEST_CHECK_EQ(summary.component_count, static_cast<std::int32_t>(3));
        TEST_CHECK_EQ(summary.largest_component_size, static_cast<std::int64_t>(3));
        TEST_CHECK_EQ(summary.isolated_node_count, static_cast<std::int64_t>(1));
        TEST_CHECK_EQ(summary.mismatch_count, static_cast<std::int64_t>(0));
    }

    {
        const std::vector<std::int32_t> arbitrary_labels{10, 10, 7, 7, 7, 99};
        const auto normalized = normalize_component_labels(arbitrary_labels);
        const std::vector<std::int32_t> expected{0, 0, 2, 2, 2, 5};
        TEST_CHECK_EQ(normalized.size(), expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
        {
            TEST_CHECK_EQ(normalized[i], expected[i]);
        }
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.repeat = 1;
        config.warmup = 0;
        config.include_gpu = false;
        config.params["graph"] = "chains";
        config.params["components"] = "4";
        config.params["component_size"] = "8";

        const auto shape = cc_shape_for_config(config);
        TEST_CHECK_EQ(shape.graph_kind, std::string("chains"));
        const auto graph = make_cc_graph_for_shape(shape);
        TEST_CHECK_EQ(graph.node_count, static_cast<std::int32_t>(32));
        const auto labels = connected_components_cpu_reference(graph);
        const auto summary = validate_component_labels(graph, labels);
        TEST_CHECK(summary.correct);
        TEST_CHECK_EQ(summary.component_count, static_cast<std::int32_t>(4));
        TEST_CHECK_EQ(summary.largest_component_size, static_cast<std::int64_t>(8));
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.repeat = 1;
        config.warmup = 0;
        config.include_gpu = false;
        config.params["graph"] = "grid_components";
        config.params["components"] = "3";
        config.params["grid_width"] = "4";
        config.params["grid_height"] = "5";

        const auto graph = make_cc_graph_for_shape(cc_shape_for_config(config));
        const auto summary = validate_component_labels(graph, connected_components_cpu_reference(graph));
        TEST_CHECK(summary.correct);
        TEST_CHECK_EQ(summary.component_count, static_cast<std::int32_t>(3));
        TEST_CHECK_EQ(summary.largest_component_size, static_cast<std::int64_t>(20));
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.repeat = 1;
        config.warmup = 0;
        config.include_gpu = false;
        config.params["graph"] = "random_clusters";
        config.params["components"] = "5";
        config.params["component_size"] = "16";
        config.params["random_out_degree"] = "2";
        config.params["sweep_label"] = "cc_demo_point";

        const auto results = run_graph_connected_components_cpu(config);
        TEST_CHECK_EQ(results.size(), static_cast<std::size_t>(1));
        TEST_CHECK_EQ(results.front().benchmark, std::string("graph_connected_components"));
        TEST_CHECK_EQ(results.front().variant, std::string("cpu"));
        TEST_CHECK_EQ(results.front().preset, std::string("cc_demo_point"));
        TEST_CHECK(results.front().correct);
        TEST_CHECK_EQ(results.front().metadata.at("component_count"), std::string("5"));
        TEST_CHECK_EQ(results.front().metadata.at("mismatch_count"), std::string("0"));
        TEST_CHECK(results.front().metadata.find("largest_component_size") != results.front().metadata.end());
    }

    TEST_CHECK_THROWS((void)make_cc_graph_for_shape(GraphCcShape{"does_not_exist"}));

    return algobench::test::finish();
}
