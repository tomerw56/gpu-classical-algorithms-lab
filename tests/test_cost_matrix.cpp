#include "cost_matrix/cost_matrix.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace
{

double metadata_double(const algobench::BenchmarkResult& result, const std::string& key)
{
    const auto it = result.metadata.find(key);
    TEST_CHECK(it != result.metadata.end());
    if (it == result.metadata.end())
    {
        return 0.0;
    }
    return std::strtod(it->second.c_str(), nullptr);
}

std::int64_t metadata_int64(const algobench::BenchmarkResult& result, const std::string& key)
{
    const auto it = result.metadata.find(key);
    TEST_CHECK(it != result.metadata.end());
    if (it == result.metadata.end())
    {
        return 0;
    }
    return static_cast<std::int64_t>(std::strtoll(it->second.c_str(), nullptr, 10));
}

} // namespace

int main()
{
    using namespace algobench::cost_matrix;

    const auto tiny = shape_for_preset("tiny");
    const auto small = shape_for_preset("small");
    const auto medium = shape_for_preset("medium");
    const auto large = shape_for_preset("large");

    TEST_CHECK_EQ(tiny.task_count, 64);
    TEST_CHECK_EQ(tiny.resource_count, 64);
    TEST_CHECK_EQ(tiny.cell_count(), 4096);
    TEST_CHECK_EQ(small.task_count, 512);
    TEST_CHECK_EQ(small.resource_count, 512);
    TEST_CHECK_EQ(medium.task_count, 2048);
    TEST_CHECK_EQ(large.resource_count, 4096);
    TEST_CHECK_THROWS((void)shape_for_preset("invalid"));

    const auto tasks = make_tasks(8);
    const auto resources = make_resources(8);
    TEST_CHECK_EQ(tasks.size(), static_cast<std::size_t>(8));
    TEST_CHECK_EQ(resources.size(), static_cast<std::size_t>(8));
    TEST_CHECK(tasks[0].required_skill != 0u);
    TEST_CHECK(resources[0].skill_mask != 0u);
    TEST_CHECK(tasks[0].urgent == 1);

    const auto first_pair = evaluate_pair(tasks[0], resources[0]);
    TEST_CHECK(first_pair.feasible == 0 || first_pair.feasible == 1);
    TEST_CHECK(first_pair.cost >= 0.0f);

    Task forced_task = tasks[0];
    Resource forced_resource = resources[0];
    forced_task.required_skill = 1u << 7u;
    forced_resource.skill_mask = 1u << 1u;
    const auto forced_infeasible = evaluate_pair(forced_task, forced_resource);
    TEST_CHECK_EQ(forced_infeasible.feasible, static_cast<std::uint8_t>(0));
    TEST_CHECK_EQ(forced_infeasible.cost, kInfeasibleCost);

    forced_resource.skill_mask = forced_task.required_skill;
    forced_resource.x = forced_task.x;
    forced_resource.y = forced_task.y;
    forced_resource.available_at = 0.0f;
    forced_resource.speed = 1.0f;
    forced_resource.load = 0.1f;
    forced_task.deadline = 1000.0f;
    const auto forced_feasible = evaluate_pair(forced_task, forced_resource);
    TEST_CHECK_EQ(forced_feasible.feasible, static_cast<std::uint8_t>(1));
    TEST_CHECK(forced_feasible.cost < kInfeasibleCost);

    algobench::BenchmarkConfig config;
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;

    const auto once = run_cost_matrix_cpu(config);
    TEST_CHECK_EQ(once.size(), static_cast<std::size_t>(1));
    const auto& cpu_once = once.front();
    TEST_CHECK_EQ(cpu_once.benchmark, std::string("cost_matrix"));
    TEST_CHECK_EQ(cpu_once.variant, std::string("cpu"));
    TEST_CHECK_EQ(cpu_once.preset, std::string("tiny"));
    TEST_CHECK_EQ(cpu_once.input_size.at("tasks"), 64);
    TEST_CHECK_EQ(cpu_once.input_size.at("resources"), 64);
    TEST_CHECK_EQ(cpu_once.input_size.at("cells"), 4096);
    TEST_CHECK(cpu_once.correct);
    TEST_CHECK(cpu_once.total_ms >= 0.0);
    TEST_CHECK(cpu_once.metadata.count("checksum") == 1);
    TEST_CHECK(cpu_once.metadata.count("reference_checksum") == 1);
    TEST_CHECK(cpu_once.metadata.count("feasible_count") == 1);
    TEST_CHECK(cpu_once.metadata.count("feasibility_mismatches") == 1);

    const double checksum_once = metadata_double(cpu_once, "checksum");
    const double reference_once = metadata_double(cpu_once, "reference_checksum");
    TEST_CHECK_NEAR(checksum_once, reference_once,
                    std::max(1.0e-3, std::abs(reference_once) * 1.0e-6));
    TEST_CHECK_EQ(metadata_int64(cpu_once, "feasibility_mismatches"), 0);

    config.repeat = 3;
    const auto repeated = run_cost_matrix_cpu(config);
    TEST_CHECK_EQ(repeated.size(), static_cast<std::size_t>(1));
    const double checksum_repeated = metadata_double(repeated.front(), "checksum");
    const double reference_repeated = metadata_double(repeated.front(), "reference_checksum");

    // Repeat controls timing only. It must not multiply the final matrix checksum.
    TEST_CHECK_NEAR(checksum_repeated, checksum_once,
                    std::max(1.0e-3, std::abs(checksum_once) * 1.0e-6));
    TEST_CHECK_NEAR(reference_repeated, reference_once,
                    std::max(1.0e-3, std::abs(reference_once) * 1.0e-6));

    return algobench::test::finish();
}
