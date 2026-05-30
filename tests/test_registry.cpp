#include "common/benchmark_registry.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
void check_info(const std::vector<algobench::BenchmarkInfo>& infos, const std::string& name)
{
    const auto it = std::find_if(infos.begin(), infos.end(), [&](const algobench::BenchmarkInfo& info) {
        return info.name == name;
    });
    TEST_CHECK(it != infos.end());
    if (it != infos.end())
    {
        TEST_CHECK(!it->description.empty());
        TEST_CHECK_EQ(it->presets.size(), static_cast<std::size_t>(4));
        TEST_CHECK_EQ(it->presets.front(), std::string("tiny"));
    }
}

void check_cpu_run(algobench::BenchmarkRegistry& registry,
                   const std::string& name,
                   const algobench::BenchmarkConfig& config)
{
    const auto results = registry.run(name, config);
    TEST_CHECK_EQ(results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(results.front().benchmark, name);
    TEST_CHECK_EQ(results.front().variant, std::string("cpu"));
    TEST_CHECK(results.front().correct);
}
}

int main()
{
    auto registry = algobench::make_default_registry();

    const std::vector<std::string> expected = {
        "foundation_smoke",
        "polynomial_batch",
        "cost_matrix",
        "spatial_events",
        "graph_bfs",
        "graph_connected_components",
        "graph_weighted_relaxation",
        "constraint_network",
        "combination_finder",
        "assignment_preprocessing",
        "local_search_moves",
        "scenario_simulation",
    };

    const auto infos = registry.list();
    TEST_CHECK_EQ(infos.size(), expected.size());
    for (const auto& name : expected)
    {
        check_info(infos, name);
    }

    algobench::BenchmarkConfig config;
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;
    config.include_gpu = false;

    for (const auto& name : expected)
    {
        check_cpu_run(registry, name, config);
    }

    const auto all_results = registry.run("all", config);
    TEST_CHECK_EQ(all_results.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        TEST_CHECK_EQ(all_results[i].benchmark, expected[i]);
    }

    TEST_CHECK_THROWS((void)registry.run("does_not_exist", config));

    return algobench::test::finish();
}
