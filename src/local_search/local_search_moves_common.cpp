#include "local_search/local_search_moves.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace algobench::local_search
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

std::uint32_t skill_mask(std::int32_t a, std::int32_t b)
{
    return (1u << static_cast<std::uint32_t>(a & 7)) |
           (1u << static_cast<std::uint32_t>(b & 7));
}

std::string to_string_double(double value)
{
    return std::to_string(value);
}
}

LocalSearchShape local_search_shape_for_config(const BenchmarkConfig& config)
{
    LocalSearchShape shape{};
    if (config.preset == "tiny")
    {
        shape = LocalSearchShape{128, 256, 4096};
    }
    else if (config.preset == "small")
    {
        shape = LocalSearchShape{512, 1024, 65536};
    }
    else if (config.preset == "medium")
    {
        shape = LocalSearchShape{2048, 2048, 1048576};
    }
    else if (config.preset == "large")
    {
        shape = LocalSearchShape{4096, 4096, 4194304};
    }
    else
    {
        shape = LocalSearchShape{512, 1024, 65536};
    }

    shape.task_count = int_param(config, "tasks", shape.task_count);
    shape.resource_count = int_param(config, "resources", shape.resource_count);
    shape.move_count = int64_param(config, "moves", shape.move_count);

    if (shape.task_count <= 0 || shape.resource_count <= 1 || shape.move_count <= 0)
    {
        throw std::runtime_error("local_search_moves requires positive tasks, resources > 1, and moves");
    }
    return shape;
}

std::string local_search_label_for_config(const BenchmarkConfig& config)
{
    const auto it = config.params.find("sweep_label");
    if (it != config.params.end() && !it->second.empty())
    {
        return it->second;
    }
    return config.preset;
}

LocalSearchProblem make_local_search_problem(const LocalSearchShape& shape)
{
    LocalSearchProblem problem{};
    problem.shape = shape;
    problem.tasks.resize(static_cast<std::size_t>(shape.task_count));
    problem.resources.resize(static_cast<std::size_t>(shape.resource_count));
    problem.current_assignment.resize(static_cast<std::size_t>(shape.task_count));
    problem.moves.resize(static_cast<std::size_t>(shape.move_count));

    for (std::int32_t i = 0; i < shape.task_count; ++i)
    {
        auto& task = problem.tasks[static_cast<std::size_t>(i)];
        task.x = static_cast<float>((i * 37 + 13) % 4096) * 0.20f;
        task.y = static_cast<float>((i * 61 + 29) % 4096) * 0.20f;
        task.required_skill_mask = skill_mask(i, i + 3);
        task.demand = 1 + (i % 5);
        task.priority = 1.0f + static_cast<float>((i * 11) % 100) / 25.0f;
        task.risk = static_cast<float>((i * 17 + 7) % 100) / 100.0f;
        problem.current_assignment[static_cast<std::size_t>(i)] = (i * 7 + 3) % shape.resource_count;
    }

    for (std::int32_t r = 0; r < shape.resource_count; ++r)
    {
        auto& resource = problem.resources[static_cast<std::size_t>(r)];
        resource.x = static_cast<float>((r * 41 + 5) % 4096) * 0.20f;
        resource.y = static_cast<float>((r * 67 + 19) % 4096) * 0.20f;
        resource.skill_mask = skill_mask(r, r + 3) | (1u << static_cast<std::uint32_t>((r + 5) & 7));
        resource.capacity = 2 + (r % 7);
        resource.max_distance = 110.0f + static_cast<float>((r * 13) % 240);
        resource.risk_tolerance = 0.25f + static_cast<float>((r * 23) % 75) / 100.0f;
        resource.load = static_cast<float>((r * 31) % 100) / 100.0f;
    }

    for (std::int64_t m = 0; m < shape.move_count; ++m)
    {
        auto& move = problem.moves[static_cast<std::size_t>(m)];
        move.task_index = static_cast<std::int32_t>((m * 37 + 11) % shape.task_count);
        move.new_resource_index = static_cast<std::int32_t>((m * 53 + 17 + (m / 97)) % shape.resource_count);
    }

    return problem;
}

std::vector<MoveEvaluation> evaluate_moves_cpu_reference(const LocalSearchProblem& problem)
{
    std::vector<MoveEvaluation> out(problem.moves.size());
    for (std::size_t i = 0; i < problem.moves.size(); ++i)
    {
        const auto move = problem.moves[i];
        const auto task = problem.tasks[static_cast<std::size_t>(move.task_index)];
        const auto old_resource_index = problem.current_assignment[static_cast<std::size_t>(move.task_index)];
        const auto old_resource = problem.resources[static_cast<std::size_t>(old_resource_index)];
        const auto new_resource = problem.resources[static_cast<std::size_t>(move.new_resource_index)];
        out[i] = evaluate_candidate_move(task, old_resource, new_resource, old_resource_index, move.new_resource_index);
    }
    return out;
}

LocalSearchAggregate aggregate_move_evaluations(const std::vector<MoveEvaluation>& evaluations)
{
    LocalSearchAggregate aggregate{};
    aggregate.best_delta = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < evaluations.size(); ++i)
    {
        const auto& e = evaluations[i];
        if (e.valid)
        {
            ++aggregate.valid_moves;
            aggregate.valid_delta_sum += e.delta;
        }
        else
        {
            ++aggregate.invalid_moves;
        }
        if (e.improving)
        {
            ++aggregate.improving_moves;
            if (static_cast<double>(e.delta) < aggregate.best_delta)
            {
                aggregate.best_delta = e.delta;
                aggregate.best_move_index = static_cast<std::int64_t>(i);
            }
        }
        if (e.violation_mask & kMoveViolationSkill) ++aggregate.skill_violations;
        if (e.violation_mask & kMoveViolationCapacity) ++aggregate.capacity_violations;
        if (e.violation_mask & kMoveViolationDistance) ++aggregate.distance_violations;
        if (e.violation_mask & kMoveViolationRisk) ++aggregate.risk_violations;
        if (e.violation_mask & kMoveViolationNoChange) ++aggregate.no_change_violations;
        aggregate.delta_checksum += static_cast<double>(e.delta) * static_cast<double>((i % 1021) + 1);
    }
    if (aggregate.best_move_index < 0)
    {
        aggregate.best_delta = 0.0;
    }
    return aggregate;
}

LocalSearchValidation validate_move_evaluations(const std::vector<MoveEvaluation>& actual,
                                                const std::vector<MoveEvaluation>& reference)
{
    LocalSearchValidation validation{};
    if (actual.size() != reference.size())
    {
        validation.correct = false;
        validation.valid_mismatches = static_cast<std::int64_t>(std::max(actual.size(), reference.size()));
        return validation;
    }

    constexpr double abs_tol = 5.0e-3;
    constexpr double rel_tol = 1.0e-5;
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        const auto& a = actual[i];
        const auto& b = reference[i];
        if (a.valid != b.valid)
        {
            ++validation.valid_mismatches;
        }
        if (a.improving != b.improving)
        {
            ++validation.improving_mismatches;
        }
        if (a.violation_mask != b.violation_mask)
        {
            ++validation.violation_mismatches;
        }
        const double err = std::abs(static_cast<double>(a.delta) - static_cast<double>(b.delta));
        const double denom = std::max(1.0, std::abs(static_cast<double>(b.delta)));
        const double rel = err / denom;
        validation.max_abs_error = std::max(validation.max_abs_error, err);
        validation.max_rel_error = std::max(validation.max_rel_error, rel);
        validation.checksum += static_cast<double>(a.delta) * static_cast<double>((i % 1021) + 1);
        validation.reference_checksum += static_cast<double>(b.delta) * static_cast<double>((i % 1021) + 1);
        if (err > abs_tol && rel > rel_tol)
        {
            validation.correct = false;
        }
    }
    if (validation.valid_mismatches || validation.improving_mismatches || validation.violation_mismatches)
    {
        validation.correct = false;
    }
    return validation;
}

void fill_local_search_metadata(BenchmarkResult& result,
                                const LocalSearchAggregate& aggregate,
                                const LocalSearchValidation& validation)
{
    result.metadata["valid_moves"] = std::to_string(aggregate.valid_moves);
    result.metadata["invalid_moves"] = std::to_string(aggregate.invalid_moves);
    result.metadata["improving_moves"] = std::to_string(aggregate.improving_moves);
    result.metadata["skill_violations"] = std::to_string(aggregate.skill_violations);
    result.metadata["capacity_violations"] = std::to_string(aggregate.capacity_violations);
    result.metadata["distance_violations"] = std::to_string(aggregate.distance_violations);
    result.metadata["risk_violations"] = std::to_string(aggregate.risk_violations);
    result.metadata["no_change_violations"] = std::to_string(aggregate.no_change_violations);
    result.metadata["best_move_index"] = std::to_string(aggregate.best_move_index);
    result.metadata["best_delta"] = to_string_double(aggregate.best_delta);
    result.metadata["delta_checksum"] = to_string_double(aggregate.delta_checksum);
    result.metadata["valid_delta_sum"] = to_string_double(aggregate.valid_delta_sum);
    result.metadata["reference_checksum"] = to_string_double(validation.reference_checksum);
    result.metadata["max_abs_error"] = to_string_double(validation.max_abs_error);
    result.metadata["max_rel_error"] = to_string_double(validation.max_rel_error);
    result.metadata["valid_mismatches"] = std::to_string(validation.valid_mismatches);
    result.metadata["improving_mismatches"] = std::to_string(validation.improving_mismatches);
    result.metadata["violation_mismatches"] = std::to_string(validation.violation_mismatches);
}

} // namespace algobench::local_search
