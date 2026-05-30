#include "assignment/assignment_preprocessing.hpp"
#include "test_support.hpp"

int main()
{
    algobench::BenchmarkConfig config;
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;
    config.params["tasks"] = "16";
    config.params["resources"] = "20";
    config.params["top_k"] = "4";
    config.params["sweep_label"] = "unit_assignment";

    const auto shape = algobench::assignment::assignment_shape_for_config(config);
    TEST_CHECK_EQ(shape.task_count, 16);
    TEST_CHECK_EQ(shape.resource_count, 20);
    TEST_CHECK_EQ(shape.top_k, 4);
    TEST_CHECK_EQ(algobench::assignment::assignment_label_for_config(config), std::string("unit_assignment"));

    const auto problem = algobench::assignment::make_assignment_problem(shape);
    TEST_CHECK_EQ(problem.tasks.size(), static_cast<std::size_t>(16));
    TEST_CHECK_EQ(problem.resources.size(), static_cast<std::size_t>(20));

    const auto topk = algobench::assignment::compute_assignment_topk_cpu_reference(problem);
    TEST_CHECK_EQ(topk.size(), static_cast<std::size_t>(16 * 4));
    const auto aggregate = algobench::assignment::aggregate_assignment_solution(problem, topk);
    TEST_CHECK_EQ(aggregate.feasible_pair_count + aggregate.infeasible_pair_count, static_cast<std::int64_t>(16 * 20));
    TEST_CHECK(aggregate.selected_candidate_count <= static_cast<std::int64_t>(16 * 4));

    const auto validation = algobench::assignment::validate_assignment_topk(problem, topk, topk);
    TEST_CHECK(validation.correct);
    TEST_CHECK_EQ(validation.selected_resource_mismatches, static_cast<std::int64_t>(0));

    const auto results = algobench::assignment::run_assignment_preprocessing_cpu(config);
    TEST_CHECK_EQ(results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(results.front().benchmark, std::string("assignment_preprocessing"));
    TEST_CHECK_EQ(results.front().variant, std::string("cpu"));
    TEST_CHECK(results.front().correct);
    TEST_CHECK_EQ(results.front().input_size.at("tasks"), static_cast<std::int64_t>(16));
    TEST_CHECK(results.front().metadata.count("candidate_reduction_ratio") == 1);

    return algobench::test::finish();
}
