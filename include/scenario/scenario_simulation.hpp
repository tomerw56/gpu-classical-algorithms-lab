#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__CUDACC__)
#define ALGOBENCH_HD __host__ __device__ inline
#else
#define ALGOBENCH_HD inline
#endif

namespace algobench::scenario
{

constexpr std::uint32_t kScenarioViolationFailure = 1u << 0;
constexpr std::uint32_t kScenarioViolationCapacity = 1u << 1;
constexpr std::uint32_t kScenarioViolationRisk = 1u << 2;
constexpr std::uint32_t kScenarioViolationDelay = 1u << 3;

struct ScenarioShape
{
    std::int32_t task_count = 0;
    std::int32_t resource_count = 0;
    std::int64_t scenario_count = 0;
};

struct ScenarioTask
{
    float x = 0.0f;
    float y = 0.0f;
    std::int32_t demand = 0;
    float base_cost = 0.0f;
    float priority = 0.0f;
    float risk = 0.0f;
    float latest_finish = 0.0f;
};

struct ScenarioResource
{
    float x = 0.0f;
    float y = 0.0f;
    std::int32_t capacity = 0;
    float speed = 0.0f;
    float reliability = 0.0f;
    float risk_tolerance = 0.0f;
    float fixed_cost = 0.0f;
};

struct ScenarioCase
{
    std::int32_t failed_resource = -1;
    std::int32_t demand_spike_task = -1;
    float delay_multiplier = 1.0f;
    float cost_multiplier = 1.0f;
    float risk_multiplier = 1.0f;
    float weather_penalty = 0.0f;
};

struct ScenarioEvaluation
{
    std::uint8_t feasible = 1;
    std::uint32_t violation_mask = 0;
    std::int32_t violation_count = 0;
    std::int32_t failed_task_count = 0;
    std::int32_t capacity_violation_count = 0;
    std::int32_t risk_violation_count = 0;
    std::int32_t delay_violation_count = 0;
    float score = 0.0f;
};

struct ScenarioProblem
{
    ScenarioShape shape;
    std::vector<ScenarioTask> tasks;
    std::vector<ScenarioResource> resources;
    std::vector<std::int32_t> assignment;
    std::vector<ScenarioCase> scenarios;
};

struct ScenarioAggregate
{
    std::int64_t feasible_scenarios = 0;
    std::int64_t infeasible_scenarios = 0;
    std::int64_t failure_scenarios = 0;
    std::int64_t capacity_scenarios = 0;
    std::int64_t risk_scenarios = 0;
    std::int64_t delay_scenarios = 0;
    std::int64_t total_violations = 0;
    double score_sum = 0.0;
    double mean_score = 0.0;
    double best_score = 0.0;
    double worst_score = 0.0;
    double score_checksum = 0.0;
    double robustness_score = 0.0;
};

struct ScenarioValidation
{
    bool correct = true;
    std::int64_t feasible_mismatches = 0;
    std::int64_t violation_mismatches = 0;
    std::int64_t count_mismatches = 0;
    double checksum = 0.0;
    double reference_checksum = 0.0;
    double max_abs_error = 0.0;
    double max_rel_error = 0.0;
};

ALGOBENCH_HD float scenario_squared_distance(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

ALGOBENCH_HD ScenarioEvaluation evaluate_scenario_case(const ScenarioTask* tasks,
                                                       const ScenarioResource* resources,
                                                       const std::int32_t* assignment,
                                                       std::int32_t task_count,
                                                       const ScenarioCase& scenario)
{
    ScenarioEvaluation out{};
    float score = scenario.weather_penalty * 120.0f;
    std::uint32_t mask = 0;

    for (std::int32_t t = 0; t < task_count; ++t)
    {
        const ScenarioTask task = tasks[t];
        const std::int32_t resource_index = assignment[t];
        const ScenarioResource resource = resources[resource_index];

        const bool failed = (scenario.failed_resource == resource_index) ||
                            (resource.reliability < 0.35f && ((t + resource_index + scenario.failed_resource + 97) % 17 == 0));
        const std::int32_t demand = task.demand + (scenario.demand_spike_task == t ? 3 : 0);
        const float d2 = scenario_squared_distance(task.x, task.y, resource.x, resource.y);
        const float travel_time = sqrtf(d2 + 1.0f) / (resource.speed + 1.0e-3f) * scenario.delay_multiplier;
        const float task_risk = task.risk * scenario.risk_multiplier;

        float task_score = (task.base_cost + resource.fixed_cost) * scenario.cost_multiplier;
        task_score += 0.45f * travel_time;
        task_score += 140.0f * task_risk;
        task_score -= 25.0f * task.priority;

        if (failed)
        {
            mask |= kScenarioViolationFailure;
            ++out.failed_task_count;
            task_score += 2000.0f + 100.0f * task.priority;
        }
        if (demand > resource.capacity)
        {
            mask |= kScenarioViolationCapacity;
            ++out.capacity_violation_count;
            task_score += 350.0f * static_cast<float>(demand - resource.capacity);
        }
        if (task_risk > resource.risk_tolerance)
        {
            mask |= kScenarioViolationRisk;
            ++out.risk_violation_count;
            task_score += 500.0f * (task_risk - resource.risk_tolerance + 0.05f);
        }
        if (travel_time > task.latest_finish)
        {
            mask |= kScenarioViolationDelay;
            ++out.delay_violation_count;
            task_score += 40.0f * (travel_time - task.latest_finish);
        }

        score += task_score;
    }

    out.violation_mask = mask;
    out.violation_count = out.failed_task_count + out.capacity_violation_count + out.risk_violation_count + out.delay_violation_count;
    out.feasible = (mask == 0u) ? 1u : 0u;
    out.score = score;
    return out;
}

ScenarioShape scenario_shape_for_config(const BenchmarkConfig& config);
std::string scenario_label_for_config(const BenchmarkConfig& config);
ScenarioProblem make_scenario_problem(const ScenarioShape& shape);
std::vector<ScenarioEvaluation> evaluate_scenarios_cpu_reference(const ScenarioProblem& problem);
ScenarioAggregate aggregate_scenario_evaluations(const std::vector<ScenarioEvaluation>& evaluations);
ScenarioValidation validate_scenario_evaluations(const std::vector<ScenarioEvaluation>& actual,
                                                 const std::vector<ScenarioEvaluation>& reference);
void fill_scenario_metadata(BenchmarkResult& result,
                            const ScenarioAggregate& aggregate,
                            const ScenarioValidation& validation);

std::vector<BenchmarkResult> run_scenario_simulation_cpu(const BenchmarkConfig& config);
std::vector<BenchmarkResult> run_scenario_simulation_gpu(const BenchmarkConfig& config);

} // namespace algobench::scenario

#undef ALGOBENCH_HD
