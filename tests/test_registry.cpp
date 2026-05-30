#include "common/benchmark_registry.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <string>

int main()
{
    auto registry = algobench::make_default_registry();

    const auto infos = registry.list();
    TEST_CHECK_EQ(infos.size(), static_cast<std::size_t>(9));

    const auto foundation_it = std::find_if(infos.begin(), infos.end(), [](const algobench::BenchmarkInfo& info) {
        return info.name == "foundation_smoke";
    });
    TEST_CHECK(foundation_it != infos.end());
    if (foundation_it != infos.end())
    {
        TEST_CHECK(!foundation_it->description.empty());
        TEST_CHECK_EQ(foundation_it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(foundation_it->presets.front(), std::string("tiny"));
    }

    const auto polynomial_it = std::find_if(infos.begin(), infos.end(), [](const algobench::BenchmarkInfo& info) {
        return info.name == "polynomial_batch";
    });
    TEST_CHECK(polynomial_it != infos.end());
    if (polynomial_it != infos.end())
    {
        TEST_CHECK(!polynomial_it->description.empty());
        TEST_CHECK_EQ(polynomial_it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(polynomial_it->presets.front(), std::string("tiny"));
    }

    const auto cost_matrix_it = std::find_if(infos.begin(), infos.end(), [](const algobench::BenchmarkInfo& info) {
        return info.name == "cost_matrix";
    });
    TEST_CHECK(cost_matrix_it != infos.end());
    if (cost_matrix_it != infos.end())
    {
        TEST_CHECK(!cost_matrix_it->description.empty());
        TEST_CHECK_EQ(cost_matrix_it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(cost_matrix_it->presets.front(), std::string("tiny"));
    }

    const auto spatial_events_it = std::find_if(infos.begin(), infos.end(), [](const algobench::BenchmarkInfo& info) {
        return info.name == "spatial_events";
    });
    TEST_CHECK(spatial_events_it != infos.end());
    if (spatial_events_it != infos.end())
    {
        TEST_CHECK(!spatial_events_it->description.empty());
        TEST_CHECK_EQ(spatial_events_it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(spatial_events_it->presets.front(), std::string("tiny"));
    }

    const auto graph_bfs_it = std::find_if(infos.begin(), infos.end(), [](const algobench::BenchmarkInfo& info) {
        return info.name == "graph_bfs";
    });
    TEST_CHECK(graph_bfs_it != infos.end());
    if (graph_bfs_it != infos.end())
    {
        TEST_CHECK(!graph_bfs_it->description.empty());
        TEST_CHECK_EQ(graph_bfs_it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(graph_bfs_it->presets.front(), std::string("tiny"));
    }



    const auto graph_cc_it = std::find_if(infos.begin(), infos.end(), [](const algobench::BenchmarkInfo& info) {
        return info.name == "graph_connected_components";
    });
    TEST_CHECK(graph_cc_it != infos.end());
    if (graph_cc_it != infos.end())
    {
        TEST_CHECK(!graph_cc_it->description.empty());
        TEST_CHECK_EQ(graph_cc_it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(graph_cc_it->presets.front(), std::string("tiny"));
    }

    const auto graph_weighted_it = std::find_if(infos.begin(), infos.end(), [](const algobench::BenchmarkInfo& info) {
        return info.name == "graph_weighted_relaxation";
    });
    TEST_CHECK(graph_weighted_it != infos.end());
    if (graph_weighted_it != infos.end())
    {
        TEST_CHECK(!graph_weighted_it->description.empty());
        TEST_CHECK_EQ(graph_weighted_it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(graph_weighted_it->presets.front(), std::string("tiny"));
    }




    const auto combination_it = std::find_if(infos.begin(), infos.end(), [](const algobench::BenchmarkInfo& info) {
        return info.name == "combination_finder";
    });
    TEST_CHECK(combination_it != infos.end());
    if (combination_it != infos.end())
    {
        TEST_CHECK(!combination_it->description.empty());
        TEST_CHECK_EQ(combination_it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(combination_it->presets.front(), std::string("tiny"));
    }

    const auto constraint_it = std::find_if(infos.begin(), infos.end(), [](const algobench::BenchmarkInfo& info) {
        return info.name == "constraint_network";
    });
    TEST_CHECK(constraint_it != infos.end());
    if (constraint_it != infos.end())
    {
        TEST_CHECK(!constraint_it->description.empty());
        TEST_CHECK_EQ(constraint_it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(constraint_it->presets.front(), std::string("tiny"));
    }

    algobench::BenchmarkConfig config;
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;
    config.include_gpu = false;

    const auto foundation_results = registry.run("foundation_smoke", config);
    TEST_CHECK_EQ(foundation_results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(foundation_results.front().benchmark, std::string("foundation_smoke"));
    TEST_CHECK_EQ(foundation_results.front().variant, std::string("cpu"));
    TEST_CHECK(foundation_results.front().correct);

    const auto polynomial_results = registry.run("polynomial_batch", config);
    TEST_CHECK_EQ(polynomial_results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(polynomial_results.front().benchmark, std::string("polynomial_batch"));
    TEST_CHECK_EQ(polynomial_results.front().variant, std::string("cpu"));
    TEST_CHECK(polynomial_results.front().correct);

    const auto cost_matrix_results = registry.run("cost_matrix", config);
    TEST_CHECK_EQ(cost_matrix_results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(cost_matrix_results.front().benchmark, std::string("cost_matrix"));
    TEST_CHECK_EQ(cost_matrix_results.front().variant, std::string("cpu"));
    TEST_CHECK(cost_matrix_results.front().correct);

    const auto spatial_events_results = registry.run("spatial_events", config);
    TEST_CHECK_EQ(spatial_events_results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(spatial_events_results.front().benchmark, std::string("spatial_events"));
    TEST_CHECK_EQ(spatial_events_results.front().variant, std::string("cpu"));
    TEST_CHECK(spatial_events_results.front().correct);

    const auto graph_bfs_results = registry.run("graph_bfs", config);
    TEST_CHECK_EQ(graph_bfs_results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(graph_bfs_results.front().benchmark, std::string("graph_bfs"));
    TEST_CHECK_EQ(graph_bfs_results.front().variant, std::string("cpu"));
    TEST_CHECK(graph_bfs_results.front().correct);



    const auto graph_cc_results = registry.run("graph_connected_components", config);
    TEST_CHECK_EQ(graph_cc_results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(graph_cc_results.front().benchmark, std::string("graph_connected_components"));
    TEST_CHECK_EQ(graph_cc_results.front().variant, std::string("cpu"));
    TEST_CHECK(graph_cc_results.front().correct);

    const auto graph_weighted_results = registry.run("graph_weighted_relaxation", config);
    TEST_CHECK_EQ(graph_weighted_results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(graph_weighted_results.front().benchmark, std::string("graph_weighted_relaxation"));
    TEST_CHECK_EQ(graph_weighted_results.front().variant, std::string("cpu"));
    TEST_CHECK(graph_weighted_results.front().correct);




    const auto combination_results = registry.run("combination_finder", config);
    TEST_CHECK_EQ(combination_results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(combination_results.front().benchmark, std::string("combination_finder"));
    TEST_CHECK_EQ(combination_results.front().variant, std::string("cpu"));
    TEST_CHECK(combination_results.front().correct);

    const auto constraint_results = registry.run("constraint_network", config);
    TEST_CHECK_EQ(constraint_results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(constraint_results.front().benchmark, std::string("constraint_network"));
    TEST_CHECK_EQ(constraint_results.front().variant, std::string("cpu"));
    TEST_CHECK(constraint_results.front().correct);

    const auto all_results = registry.run("all", config);
    TEST_CHECK_EQ(all_results.size(), static_cast<std::size_t>(9));
    TEST_CHECK_EQ(all_results[0].benchmark, std::string("foundation_smoke"));
    TEST_CHECK_EQ(all_results[1].benchmark, std::string("polynomial_batch"));
    TEST_CHECK_EQ(all_results[2].benchmark, std::string("cost_matrix"));
    TEST_CHECK_EQ(all_results[3].benchmark, std::string("spatial_events"));
    TEST_CHECK_EQ(all_results[4].benchmark, std::string("graph_bfs"));
    TEST_CHECK_EQ(all_results[5].benchmark, std::string("graph_connected_components"));
    TEST_CHECK_EQ(all_results[6].benchmark, std::string("graph_weighted_relaxation"));
    TEST_CHECK_EQ(all_results[7].benchmark, std::string("constraint_network"));
    TEST_CHECK_EQ(all_results[8].benchmark, std::string("combination_finder"));

    TEST_CHECK_THROWS((void)registry.run("does_not_exist", config));

    return algobench::test::finish();
}
