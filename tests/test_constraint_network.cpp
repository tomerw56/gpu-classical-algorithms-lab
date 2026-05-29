#include "constraints/constraint_network.hpp"
#include "test_support.hpp"

#include <string>

using namespace algobench;
using namespace algobench::constraints;

int main()
{
    {
        Task task{};
        task.required_skill_mask = 0b0011u;
        task.required_capacity = 4;
        task.earliest_start = 100;
        task.latest_end = 220;
        task.x = 0.0f;
        task.y = 0.0f;
        task.zone_id = 2;
        task.risk = 0.4f;

        Resource resource{};
        resource.skill_mask = 0b0111u;
        resource.capacity = 5;
        resource.available_start = 80;
        resource.available_end = 300;
        resource.home_x = 10.0f;
        resource.home_y = 0.0f;
        resource.max_distance = 100.0f;
        resource.forbidden_zone_mask = 0u;
        resource.risk_tolerance = 0.8f;

        CandidateAssignment candidate{};
        candidate.start_time = 120;
        candidate.duration = 40;

        const auto evaluation = evaluate_candidate(task, resource, candidate);
        TEST_CHECK(evaluation.valid == 1u);
        TEST_CHECK_EQ(evaluation.violation_count, 0);
        TEST_CHECK_EQ(evaluation.violation_mask, 0u);
    }

    {
        Task task{};
        task.required_skill_mask = 0b1000u;
        task.required_capacity = 8;
        task.earliest_start = 500;
        task.latest_end = 550;
        task.x = 500.0f;
        task.y = 500.0f;
        task.zone_id = 3;
        task.risk = 0.95f;

        Resource resource{};
        resource.skill_mask = 0b0011u;
        resource.capacity = 2;
        resource.available_start = 0;
        resource.available_end = 400;
        resource.home_x = 0.0f;
        resource.home_y = 0.0f;
        resource.max_distance = 10.0f;
        resource.forbidden_zone_mask = 1u << 3u;
        resource.risk_tolerance = 0.3f;

        CandidateAssignment candidate{};
        candidate.start_time = 100;
        candidate.duration = 500;

        const auto evaluation = evaluate_candidate(task, resource, candidate);
        TEST_CHECK(evaluation.valid == 0u);
        TEST_CHECK((evaluation.violation_mask & kViolationSkill) != 0u);
        TEST_CHECK((evaluation.violation_mask & kViolationCapacity) != 0u);
        TEST_CHECK((evaluation.violation_mask & kViolationTaskWindow) != 0u);
        TEST_CHECK((evaluation.violation_mask & kViolationResourceWindow) != 0u);
        TEST_CHECK((evaluation.violation_mask & kViolationDistance) != 0u);
        TEST_CHECK((evaluation.violation_mask & kViolationForbiddenZone) != 0u);
        TEST_CHECK((evaluation.violation_mask & kViolationRisk) != 0u);
        TEST_CHECK_EQ(evaluation.violation_count, 7);
    }


    {
        BenchmarkConfig config;
        config.preset = "tiny";
        TEST_CHECK_EQ(constraint_label_for_config(config), std::string("tiny"));
        config.params["sweep_label"] = "cn_64k";
        TEST_CHECK_EQ(constraint_label_for_config(config), std::string("cn_64k"));
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.params["tasks"] = "16";
        config.params["resources"] = "8";
        config.params["candidates"] = "128";
        config.repeat = 1;
        config.warmup = 0;

        const auto shape = constraint_shape_for_config(config);
        TEST_CHECK_EQ(shape.task_count, 16);
        TEST_CHECK_EQ(shape.resource_count, 8);
        TEST_CHECK_EQ(shape.candidate_count, 128);

        const auto problem = make_constraint_problem(shape);
        TEST_CHECK_EQ(problem.tasks.size(), static_cast<std::size_t>(16));
        TEST_CHECK_EQ(problem.resources.size(), static_cast<std::size_t>(8));
        TEST_CHECK_EQ(problem.candidates.size(), static_cast<std::size_t>(128));

        const auto evaluations = evaluate_candidates_cpu_reference(problem);
        const auto validation = validate_constraint_evaluations(problem, evaluations);
        TEST_CHECK(validation.correct);
        TEST_CHECK_EQ(validation.validity_mismatches, 0);
        TEST_CHECK_EQ(validation.mask_mismatches, 0);
    }


    {
        BenchmarkConfig config;
        config.preset = "tiny";
        TEST_CHECK_EQ(constraint_label_for_config(config), std::string("tiny"));
        config.params["sweep_label"] = "cn_64k";
        TEST_CHECK_EQ(constraint_label_for_config(config), std::string("cn_64k"));
    }

    {
        BenchmarkConfig config;
        config.preset = "tiny";
        config.params["tasks"] = "16";
        config.params["resources"] = "8";
        config.params["candidates"] = "256";
        config.repeat = 1;
        config.warmup = 0;

        const auto results = run_constraint_network_cpu(config);
        TEST_CHECK_EQ(results.size(), 1u);
        TEST_CHECK_EQ(results[0].benchmark, std::string("constraint_network"));
        TEST_CHECK_EQ(results[0].variant, std::string("cpu"));
        TEST_CHECK(results[0].correct);
        TEST_CHECK(results[0].metadata.count("valid_count") == 1);
        TEST_CHECK(results[0].metadata.count("total_violations") == 1);
    }

    return algobench::test::finish();
}
