#include "assignment/assignment_preprocessing.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace algobench::assignment
{
namespace
{
std::int32_t int_param(const BenchmarkConfig& config, const std::string& key, std::int32_t fallback)
{
    const auto it = config.params.find(key);
    if (it == config.params.end())
    {
        return fallback;
    }
    return std::stoi(it->second);
}

std::uint32_t make_skill_mask(std::int32_t a, std::int32_t b, std::int32_t c)
{
    return (1u << static_cast<std::uint32_t>(a & 7)) |
           (1u << static_cast<std::uint32_t>(b & 7)) |
           (1u << static_cast<std::uint32_t>(c & 7));
}

void consider_topk(std::vector<TopKEntry>& entries, std::int32_t top_k, std::int32_t resource_index, float cost)
{
    std::int32_t worst = 0;
    for (std::int32_t i = 1; i < top_k; ++i)
    {
        if (entries[static_cast<std::size_t>(i)].cost > entries[static_cast<std::size_t>(worst)].cost)
        {
            worst = i;
        }
    }

    auto& slot = entries[static_cast<std::size_t>(worst)];
    if (cost < slot.cost || (cost == slot.cost && resource_index < slot.resource_index))
    {
        slot.resource_index = resource_index;
        slot.cost = cost;
    }
}

void sort_task_topk(std::vector<TopKEntry>& entries)
{
    std::sort(entries.begin(), entries.end(), [](const TopKEntry& a, const TopKEntry& b) {
        if (a.cost != b.cost)
        {
            return a.cost < b.cost;
        }
        return a.resource_index < b.resource_index;
    });
}
} // namespace

AssignmentShape assignment_shape_for_config(const BenchmarkConfig& config)
{
    AssignmentShape shape{};

    if (config.preset == "tiny")
    {
        shape = AssignmentShape{64, 128, 4};
    }
    else if (config.preset == "small")
    {
        shape = AssignmentShape{512, 1024, 8};
    }
    else if (config.preset == "medium")
    {
        shape = AssignmentShape{2048, 2048, 8};
    }
    else if (config.preset == "large")
    {
        shape = AssignmentShape{4096, 4096, 8};
    }
    else
    {
        shape = AssignmentShape{512, 1024, 8};
    }

    shape.task_count = int_param(config, "tasks", shape.task_count);
    shape.resource_count = int_param(config, "resources", shape.resource_count);
    shape.top_k = int_param(config, "top_k", shape.top_k);

    if (shape.task_count <= 0 || shape.resource_count <= 0 || shape.top_k <= 0)
    {
        throw std::runtime_error("assignment_preprocessing requires positive tasks/resources/top_k");
    }
    if (shape.top_k > shape.resource_count)
    {
        shape.top_k = shape.resource_count;
    }
    return shape;
}

std::string assignment_label_for_config(const BenchmarkConfig& config)
{
    const auto it = config.params.find("sweep_label");
    if (it != config.params.end() && !it->second.empty())
    {
        return it->second;
    }
    return config.preset;
}

AssignmentProblem make_assignment_problem(const AssignmentShape& shape)
{
    AssignmentProblem problem{};
    problem.shape = shape;
    problem.tasks.resize(static_cast<std::size_t>(shape.task_count));
    problem.resources.resize(static_cast<std::size_t>(shape.resource_count));

    for (std::int32_t i = 0; i < shape.task_count; ++i)
    {
        auto& task = problem.tasks[static_cast<std::size_t>(i)];
        task.x = static_cast<float>((i * 37 + 11) % 4000) * 0.25f;
        task.y = static_cast<float>((i * 53 + 23) % 4000) * 0.25f;
        task.required_skill_mask = (1u << static_cast<std::uint32_t>(i % 8));
        if ((i % 7) == 0)
        {
            task.required_skill_mask |= 1u << static_cast<std::uint32_t>((i + 3) % 8);
        }
        task.demand = 1 + (i % 5);
        task.earliest = (i * 13) % 600;
        task.duration = 20 + (i % 90);
        task.latest = task.earliest + task.duration + 120 + ((i * 5) % 220);
        task.zone_id = i % 12;
        task.priority = 1.0f + static_cast<float>((i * 17) % 100) / 20.0f;
        task.risk = static_cast<float>((i * 19) % 100) / 100.0f;
    }

    for (std::int32_t r = 0; r < shape.resource_count; ++r)
    {
        auto& resource = problem.resources[static_cast<std::size_t>(r)];
        resource.x = static_cast<float>((r * 31 + 7) % 4000) * 0.25f;
        resource.y = static_cast<float>((r * 47 + 29) % 4000) * 0.25f;
        resource.skill_mask = make_skill_mask(r, r + 2, r + 5);
        if ((r % 11) == 0)
        {
            resource.skill_mask |= 0xFFu;
        }
        resource.capacity = 2 + (r % 7);
        resource.available_start = (r * 17) % 500;
        resource.available_end = resource.available_start + 300 + ((r * 13) % 300);
        resource.forbidden_zone_mask = 0u;
        if ((r % 5) == 0)
        {
            resource.forbidden_zone_mask |= 1u << static_cast<std::uint32_t>((r + 1) % 12);
        }
        if ((r % 9) == 0)
        {
            resource.forbidden_zone_mask |= 1u << static_cast<std::uint32_t>((r + 4) % 12);
        }
        resource.max_distance = 240.0f + static_cast<float>((r * 7) % 280);
        resource.risk_tolerance = 0.35f + static_cast<float>((r * 11) % 65) / 100.0f;
        resource.load = static_cast<float>((r * 23) % 100) / 100.0f;
    }

    return problem;
}

std::vector<TopKEntry> compute_assignment_topk_cpu_reference(const AssignmentProblem& problem)
{
    const auto tasks = problem.shape.task_count;
    const auto resources = problem.shape.resource_count;
    const auto top_k = problem.shape.top_k;
    std::vector<TopKEntry> output(static_cast<std::size_t>(tasks * top_k));

    for (std::int32_t t = 0; t < tasks; ++t)
    {
        std::vector<TopKEntry> local(static_cast<std::size_t>(top_k));
        for (auto& entry : local)
        {
            entry.resource_index = -1;
            entry.cost = kAssignmentInfeasibleCost;
        }

        const auto& task = problem.tasks[static_cast<std::size_t>(t)];
        for (std::int32_t r = 0; r < resources; ++r)
        {
            const auto eval = evaluate_assignment_pair(task, problem.resources[static_cast<std::size_t>(r)]);
            if (eval.feasible)
            {
                consider_topk(local, top_k, r, eval.cost);
            }
        }
        sort_task_topk(local);
        for (std::int32_t k = 0; k < top_k; ++k)
        {
            output[static_cast<std::size_t>(t * top_k + k)] = local[static_cast<std::size_t>(k)];
        }
    }

    return output;
}

AssignmentAggregate aggregate_assignment_solution(const AssignmentProblem& problem,
                                                  const std::vector<TopKEntry>& topk)
{
    AssignmentAggregate aggregate{};
    const auto tasks = problem.shape.task_count;
    const auto resources = problem.shape.resource_count;
    const auto top_k = problem.shape.top_k;

    for (std::int32_t t = 0; t < tasks; ++t)
    {
        bool has_candidate = false;
        for (std::int32_t r = 0; r < resources; ++r)
        {
            const auto eval = evaluate_assignment_pair(problem.tasks[static_cast<std::size_t>(t)],
                                                       problem.resources[static_cast<std::size_t>(r)]);
            if (eval.feasible)
            {
                ++aggregate.feasible_pair_count;
            }
            else
            {
                ++aggregate.infeasible_pair_count;
            }
            if (eval.violation_mask & kAssignViolationSkill) ++aggregate.skill_violations;
            if (eval.violation_mask & kAssignViolationCapacity) ++aggregate.capacity_violations;
            if (eval.violation_mask & kAssignViolationTime) ++aggregate.time_violations;
            if (eval.violation_mask & kAssignViolationDistance) ++aggregate.distance_violations;
            if (eval.violation_mask & kAssignViolationZone) ++aggregate.zone_violations;
            if (eval.violation_mask & kAssignViolationRisk) ++aggregate.risk_violations;
        }

        for (std::int32_t k = 0; k < top_k; ++k)
        {
            const auto& entry = topk[static_cast<std::size_t>(t * top_k + k)];
            if (entry.resource_index >= 0)
            {
                ++aggregate.selected_candidate_count;
                aggregate.selected_cost_checksum += static_cast<double>(entry.cost) * static_cast<double>((t + 1) * (k + 3));
                aggregate.mean_selected_cost += entry.cost;
                if (k == 0)
                {
                    aggregate.best_cost_sum += entry.cost;
                    has_candidate = true;
                }
            }
        }
        if (has_candidate)
        {
            ++aggregate.tasks_with_any_candidate;
        }
    }

    if (aggregate.selected_candidate_count > 0)
    {
        aggregate.mean_selected_cost /= static_cast<double>(aggregate.selected_candidate_count);
    }

    return aggregate;
}

AssignmentValidation validate_assignment_topk(const AssignmentProblem& problem,
                                              const std::vector<TopKEntry>& actual,
                                              const std::vector<TopKEntry>& reference)
{
    AssignmentValidation validation{};
    const auto tasks = problem.shape.task_count;
    const auto top_k = problem.shape.top_k;
    const double abs_tol = 5.0e-3;
    const double rel_tol = 1.0e-5;

    for (std::int32_t i = 0; i < tasks * top_k; ++i)
    {
        const auto& a = actual[static_cast<std::size_t>(i)];
        const auto& b = reference[static_cast<std::size_t>(i)];
        if (a.resource_index != b.resource_index)
        {
            ++validation.selected_resource_mismatches;
        }
        const double diff = std::abs(static_cast<double>(a.cost) - static_cast<double>(b.cost));
        const double denom = std::max(1.0, std::abs(static_cast<double>(b.cost)));
        validation.max_abs_error = std::max(validation.max_abs_error, diff);
        validation.max_rel_error = std::max(validation.max_rel_error, diff / denom);
        validation.checksum += static_cast<double>(a.cost) * static_cast<double>(i + 1);
        validation.reference_checksum += static_cast<double>(b.cost) * static_cast<double>(i + 1);
        if (diff > abs_tol && diff / denom > rel_tol)
        {
            validation.correct = false;
        }
    }

    if (validation.selected_resource_mismatches != 0)
    {
        validation.correct = false;
    }
    return validation;
}

void fill_assignment_metadata(BenchmarkResult& result,
                              const AssignmentAggregate& aggregate,
                              const AssignmentValidation& validation)
{
    result.metadata["feasible_pair_count"] = std::to_string(aggregate.feasible_pair_count);
    result.metadata["infeasible_pair_count"] = std::to_string(aggregate.infeasible_pair_count);
    result.metadata["selected_candidate_count"] = std::to_string(aggregate.selected_candidate_count);
    result.metadata["tasks_with_any_candidate"] = std::to_string(aggregate.tasks_with_any_candidate);
    result.metadata["skill_violations"] = std::to_string(aggregate.skill_violations);
    result.metadata["capacity_violations"] = std::to_string(aggregate.capacity_violations);
    result.metadata["time_violations"] = std::to_string(aggregate.time_violations);
    result.metadata["distance_violations"] = std::to_string(aggregate.distance_violations);
    result.metadata["zone_violations"] = std::to_string(aggregate.zone_violations);
    result.metadata["risk_violations"] = std::to_string(aggregate.risk_violations);
    result.metadata["selected_cost_checksum"] = std::to_string(aggregate.selected_cost_checksum);
    result.metadata["best_cost_sum"] = std::to_string(aggregate.best_cost_sum);
    result.metadata["mean_selected_cost"] = std::to_string(aggregate.mean_selected_cost);
    result.metadata["selected_resource_mismatches"] = std::to_string(validation.selected_resource_mismatches);
    result.metadata["max_abs_error"] = std::to_string(validation.max_abs_error);
    result.metadata["max_rel_error"] = std::to_string(validation.max_rel_error);
    const auto total_pairs = aggregate.feasible_pair_count + aggregate.infeasible_pair_count;
    if (total_pairs > 0)
    {
        result.metadata["candidate_reduction_ratio"] = std::to_string(
            static_cast<double>(aggregate.selected_candidate_count) / static_cast<double>(total_pairs));
    }
}

} // namespace algobench::assignment
