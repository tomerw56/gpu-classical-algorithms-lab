#include "common/benchmark_registry.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <string>

int main()
{
    auto registry = algobench::make_default_registry();

    const auto infos = registry.list();
    TEST_CHECK_EQ(infos.size(), static_cast<std::size_t>(4));

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

    const auto all_results = registry.run("all", config);
    TEST_CHECK_EQ(all_results.size(), static_cast<std::size_t>(4));
    TEST_CHECK_EQ(all_results[0].benchmark, std::string("foundation_smoke"));
    TEST_CHECK_EQ(all_results[1].benchmark, std::string("polynomial_batch"));
    TEST_CHECK_EQ(all_results[2].benchmark, std::string("cost_matrix"));
    TEST_CHECK_EQ(all_results[3].benchmark, std::string("spatial_events"));

    TEST_CHECK_THROWS((void)registry.run("does_not_exist", config));

    return algobench::test::finish();
}
