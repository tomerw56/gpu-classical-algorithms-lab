#include "scenario/scenario_simulation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace algobench::scenario
{
namespace
{
std::int32_t int_param(const BenchmarkConfig& config, const std::string& key, std::int32_t fallback)
{
    const auto it = config.params.find(key);
    return it == config.params.end() ? fallback : std::stoi(it->second);
}

std::int64_t int64_param(const BenchmarkConfig& config, const std::string& key, std::int64_t fallback)
{
    const auto it = config.params.find(key);
    return it == config.params.end() ? fallback : std::stoll(it->second);
}

std::string as_string(double value)
{
    return std::to_string(value);
}
}

ScenarioShape scenario_shape_for_config(const BenchmarkConfig& config)
{
    ScenarioShape shape{};
    if (config.preset == "tiny")
    {
        shape = ScenarioShape{32, 16, 4096};
    }
    else if (config.preset == "small")
    {
        shape = ScenarioShape{64, 32, 65536};
    }
    else if (config.preset == "medium")
    {
        shape = ScenarioShape{128, 64, 1048576};
    }
    else if (config.preset == "large")
    {
        shape = ScenarioShape{256, 128, 4194304};
    }
    else
    {
        shape = ScenarioShape{64, 32, 65536};
    }

    shape.task_count = int_param(config, "tasks", shape.task_count);
    shape.resource_count = int_param(config, "resources", shape.resource_count);
    shape.scenario_count = int64_param(config, "scenarios", shape.scenario_count);

    if (shape.task_count <= 0 || shape.resource_count <= 0 || shape.scenario_count <= 0)
    {
        throw std::runtime_error("scenario_simulation requires positive tasks, resources, and scenarios");
    }
    return shape;
}

std::string scenario_label_for_config(const BenchmarkConfig& config)
{
    const auto it = config.params.find("sweep_label");
    if (it != config.params.end() && !it->second.empty())
    {
        return it->second;
    }
    return config.preset;
}

ScenarioProblem make_scenario_problem(const ScenarioShape& shape)
{
    ScenarioProblem problem{};
    problem.shape = shape;
    problem.tasks.resize(static_cast<std::size_t>(shape.task_count));
    problem.resources.resize(static_cast<std::size_t>(shape.resource_count));
    problem.assignment.resize(static_cast<std::size_t>(shape.task_count));
    problem.scenarios.resize(static_cast<std::size_t>(shape.scenario_count));

    for (std::int32_t t = 0; t < shape.task_count; ++t)
    {
        auto& task = problem.tasks[static_cast<std::size_t>(t)];
        task.x = static_cast<float>((t * 37 + 11) % 2048) * 0.35f;
        task.y = static_cast<float>((t * 59 + 23) % 2048) * 0.35f;
        task.demand = 1 + (t % 4);
        task.base_cost = 80.0f + static_cast<float>((t * 17) % 180);
        task.priority = 1.0f + static_cast<float>((t * 13) % 100) / 30.0f;

        // Keep the base plan plausible. The point of this benchmark is not to
        // prove that the generated plan is always good; it is to evaluate the
        // same plan across many possible worlds. Therefore the base case should
        // mostly pass mild scenarios but fail meaningful stress scenarios.
        task.risk = 0.035f + static_cast<float>((t * 29 + 7) % 70) / 650.0f;
        task.latest_finish = 120.0f + static_cast<float>((t * 19) % 90);
    }

    for (std::int32_t r = 0; r < shape.resource_count; ++r)
    {
        auto& resource = problem.resources[static_cast<std::size_t>(r)];
        resource.x = static_cast<float>((r * 43 + 5) % 2048) * 0.35f;
        resource.y = static_cast<float>((r * 71 + 17) % 2048) * 0.35f;
        resource.capacity = 7 + (r % 8);
        resource.speed = 7.0f + static_cast<float>((r * 11) % 55) / 10.0f;
        resource.reliability = 0.84f + static_cast<float>((r * 23) % 15) / 100.0f;
        resource.risk_tolerance = 0.28f + static_cast<float>((r * 31) % 55) / 100.0f;
        resource.fixed_cost = 20.0f + static_cast<float>((r * 7) % 75);
    }

    // Build a reasonable base assignment instead of a purely modular one. This
    // is not an optimizer; it just chooses a nearby capable resource so that the
    // scenario sweep contains a useful mix of feasible and infeasible outcomes.
    for (std::int32_t t = 0; t < shape.task_count; ++t)
    {
        const auto& task = problem.tasks[static_cast<std::size_t>(t)];
        std::int32_t best_resource = 0;
        float best_score = std::numeric_limits<float>::infinity();
        for (std::int32_t r = 0; r < shape.resource_count; ++r)
        {
            const auto& resource = problem.resources[static_cast<std::size_t>(r)];
            const bool capacity_ok = resource.capacity >= task.demand + 3;
            const bool risk_ok = resource.risk_tolerance >= task.risk * 1.40f;
            const float d2 = scenario_squared_distance(task.x, task.y, resource.x, resource.y);
            float candidate_score = d2 + 15.0f * resource.fixed_cost;
            if (!capacity_ok) candidate_score += 1000000.0f;
            if (!risk_ok) candidate_score += 1000000.0f;
            if (candidate_score < best_score)
            {
                best_score = candidate_score;
                best_resource = r;
            }
        }
        problem.assignment[static_cast<std::size_t>(t)] = best_resource;

        const auto& resource = problem.resources[static_cast<std::size_t>(best_resource)];
        const float d2 = scenario_squared_distance(task.x, task.y, resource.x, resource.y);
        const float base_travel = std::sqrt(d2 + 1.0f) / (resource.speed + 1.0e-3f);
        problem.tasks[static_cast<std::size_t>(t)].latest_finish = std::max(task.latest_finish, 25.0f + 1.45f * base_travel);
    }

    for (std::int64_t s = 0; s < shape.scenario_count; ++s)
    {
        auto& scenario = problem.scenarios[static_cast<std::size_t>(s)];
        scenario.failed_resource = ((s * 17 + 5) % 23 == 0)
            ? static_cast<std::int32_t>((s * 13 + 3) % shape.resource_count)
            : -1;
        scenario.demand_spike_task = ((s * 19 + 7) % 29 == 0)
            ? static_cast<std::int32_t>((s * 11 + 1) % shape.task_count)
            : -1;
        scenario.delay_multiplier = 0.75f + static_cast<float>((s * 37 + 11) % 60) / 100.0f;
        if ((s * 47 + 3) % 17 == 0)
        {
            scenario.delay_multiplier += 0.65f;
        }
        scenario.cost_multiplier = 0.90f + static_cast<float>((s * 41 + 13) % 55) / 100.0f;
        scenario.risk_multiplier = 0.75f + static_cast<float>((s * 53 + 17) % 60) / 100.0f;
        if ((s * 31 + 5) % 19 == 0)
        {
            scenario.risk_multiplier += 0.55f;
        }
        scenario.weather_penalty = static_cast<float>((s * 61 + 19) % 100) / 100.0f;
    }

    return problem;
}

std::vector<ScenarioEvaluation> evaluate_scenarios_cpu_reference(const ScenarioProblem& problem)
{
    std::vector<ScenarioEvaluation> out(problem.scenarios.size());
    for (std::size_t i = 0; i < problem.scenarios.size(); ++i)
    {
        out[i] = evaluate_scenario_case(problem.tasks.data(),
                                        problem.resources.data(),
                                        problem.assignment.data(),
                                        problem.shape.task_count,
                                        problem.scenarios[i]);
    }
    return out;
}

ScenarioAggregate aggregate_scenario_evaluations(const std::vector<ScenarioEvaluation>& evaluations)
{
    ScenarioAggregate aggregate{};
    aggregate.best_score = std::numeric_limits<double>::infinity();
    aggregate.worst_score = -std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < evaluations.size(); ++i)
    {
        const auto& e = evaluations[i];
        if (e.feasible)
        {
            ++aggregate.feasible_scenarios;
        }
        else
        {
            ++aggregate.infeasible_scenarios;
        }
        if (e.violation_mask & kScenarioViolationFailure) ++aggregate.failure_scenarios;
        if (e.violation_mask & kScenarioViolationCapacity) ++aggregate.capacity_scenarios;
        if (e.violation_mask & kScenarioViolationRisk) ++aggregate.risk_scenarios;
        if (e.violation_mask & kScenarioViolationDelay) ++aggregate.delay_scenarios;
        aggregate.total_violations += e.violation_count;
        aggregate.score_sum += e.score;
        aggregate.best_score = std::min(aggregate.best_score, static_cast<double>(e.score));
        aggregate.worst_score = std::max(aggregate.worst_score, static_cast<double>(e.score));
        aggregate.score_checksum += static_cast<double>(e.score) * static_cast<double>((i % 4093) + 1);
    }

    if (!evaluations.empty())
    {
        aggregate.mean_score = aggregate.score_sum / static_cast<double>(evaluations.size());
        const double infeasible_ratio = static_cast<double>(aggregate.infeasible_scenarios) / static_cast<double>(evaluations.size());
        aggregate.robustness_score = aggregate.mean_score + 0.25 * aggregate.worst_score + 10000.0 * infeasible_ratio;
    }
    else
    {
        aggregate.best_score = 0.0;
        aggregate.worst_score = 0.0;
    }
    return aggregate;
}

ScenarioValidation validate_scenario_evaluations(const std::vector<ScenarioEvaluation>& actual,
                                                 const std::vector<ScenarioEvaluation>& reference)
{
    ScenarioValidation validation{};
    if (actual.size() != reference.size())
    {
        validation.correct = false;
        validation.feasible_mismatches = static_cast<std::int64_t>(std::max(actual.size(), reference.size()));
        return validation;
    }

    constexpr double abs_tol = 1.0e-2;
    constexpr double rel_tol = 1.0e-5;
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        const auto& a = actual[i];
        const auto& b = reference[i];
        if (a.feasible != b.feasible)
        {
            ++validation.feasible_mismatches;
        }
        if (a.violation_mask != b.violation_mask)
        {
            ++validation.violation_mismatches;
        }
        if (a.violation_count != b.violation_count ||
            a.failed_task_count != b.failed_task_count ||
            a.capacity_violation_count != b.capacity_violation_count ||
            a.risk_violation_count != b.risk_violation_count ||
            a.delay_violation_count != b.delay_violation_count)
        {
            ++validation.count_mismatches;
        }
        const double err = std::abs(static_cast<double>(a.score) - static_cast<double>(b.score));
        const double denom = std::max(1.0, std::abs(static_cast<double>(b.score)));
        const double rel = err / denom;
        validation.max_abs_error = std::max(validation.max_abs_error, err);
        validation.max_rel_error = std::max(validation.max_rel_error, rel);
        validation.checksum += static_cast<double>(a.score) * static_cast<double>((i % 4093) + 1);
        validation.reference_checksum += static_cast<double>(b.score) * static_cast<double>((i % 4093) + 1);
        if (err > abs_tol && rel > rel_tol)
        {
            validation.correct = false;
        }
    }

    if (validation.feasible_mismatches || validation.violation_mismatches || validation.count_mismatches)
    {
        validation.correct = false;
    }
    return validation;
}

void fill_scenario_metadata(BenchmarkResult& result,
                            const ScenarioAggregate& aggregate,
                            const ScenarioValidation& validation)
{
    const double total_scenarios = static_cast<double>(aggregate.feasible_scenarios + aggregate.infeasible_scenarios);
    const double feasible_ratio = total_scenarios > 0.0 ? static_cast<double>(aggregate.feasible_scenarios) / total_scenarios : 0.0;
    result.metadata["correctness_semantics"] = "cpu_gpu_agreement_not_plan_quality";
    result.metadata["feasibility_status"] = (aggregate.feasible_scenarios == 0)
        ? "all_infeasible"
        : ((aggregate.infeasible_scenarios == 0) ? "all_feasible" : "mixed_feasible_infeasible");
    result.metadata["feasible_ratio"] = as_string(feasible_ratio);
    result.metadata["feasible_scenarios"] = std::to_string(aggregate.feasible_scenarios);
    result.metadata["infeasible_scenarios"] = std::to_string(aggregate.infeasible_scenarios);
    result.metadata["failure_scenarios"] = std::to_string(aggregate.failure_scenarios);
    result.metadata["capacity_scenarios"] = std::to_string(aggregate.capacity_scenarios);
    result.metadata["risk_scenarios"] = std::to_string(aggregate.risk_scenarios);
    result.metadata["delay_scenarios"] = std::to_string(aggregate.delay_scenarios);
    result.metadata["total_violations"] = std::to_string(aggregate.total_violations);
    result.metadata["mean_score"] = as_string(aggregate.mean_score);
    result.metadata["best_score"] = as_string(aggregate.best_score);
    result.metadata["worst_score"] = as_string(aggregate.worst_score);
    result.metadata["robustness_score"] = as_string(aggregate.robustness_score);
    result.metadata["score_checksum"] = as_string(aggregate.score_checksum);
    result.metadata["reference_checksum"] = as_string(validation.reference_checksum);
    result.metadata["feasible_mismatches"] = std::to_string(validation.feasible_mismatches);
    result.metadata["violation_mismatches"] = std::to_string(validation.violation_mismatches);
    result.metadata["count_mismatches"] = std::to_string(validation.count_mismatches);
    result.metadata["max_abs_error"] = as_string(validation.max_abs_error);
    result.metadata["max_rel_error"] = as_string(validation.max_rel_error);
}

} // namespace algobench::scenario
