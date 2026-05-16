#include "cost_matrix/cost_matrix.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace algobench::cost_matrix
{

CostMatrixShape shape_for_preset(const std::string& preset)
{
    if (preset == "tiny")
    {
        return {64, 64};
    }
    if (preset == "small")
    {
        return {512, 512};
    }
    if (preset == "medium")
    {
        return {2048, 2048};
    }
    if (preset == "large")
    {
        return {4096, 4096};
    }

    throw std::runtime_error("unknown cost_matrix preset: " + preset);
}

std::vector<Task> make_tasks(std::int64_t task_count)
{
    std::vector<Task> tasks(static_cast<std::size_t>(task_count));

    for (std::int64_t i = 0; i < task_count; ++i)
    {
        Task task;
        task.x = static_cast<float>((i * 17) % 1600) * 0.45f;
        task.y = static_cast<float>((i * 31 + 13) % 1600) * 0.35f;
        task.deadline = 150.0f + static_cast<float>((i * 19) % 120) * 3.5f;
        task.priority = 1.0f + static_cast<float>(i % 7) * 0.35f;
        task.required_skill = 1u << static_cast<std::uint32_t>(i % kSkillCount);
        task.urgent = static_cast<std::uint8_t>((i % 6) == 0 ? 1 : 0);
        tasks[static_cast<std::size_t>(i)] = task;
    }

    return tasks;
}

std::vector<Resource> make_resources(std::int64_t resource_count)
{
    std::vector<Resource> resources(static_cast<std::size_t>(resource_count));

    // This is benchmark data generation, not a real dispatch model. The arithmetic
    // patterns create varied yet deterministic resource properties so every run sees
    // exactly the same resource set and validation remains reproducible.
    for (std::int64_t j = 0; j < resource_count; ++j)
    {
        Resource resource;

        // Synthetic resource locations. Different multipliers from make_tasks()
        // avoid aligning every resource with a task on the same simple lattice.
        resource.x = static_cast<float>((j * 29 + 7) % 1600) * 0.42f;
        resource.y = static_cast<float>((j * 43 + 5) % 1600) * 0.38f;

        // Load feeds the branchy nonlinear load penalty in evaluate_pair(). Values
        // cycle deterministically through roughly [0.00, 1.00].
        resource.load = static_cast<float>((j * 11) % 101) / 100.0f;

        // Speed and availability influence travel time and lateness rejection.
        resource.speed = 0.85f + static_cast<float>(j % 6) * 0.18f;
        resource.available_at = static_cast<float>((j * 23) % 90) * 2.0f;

        // Each resource receives three supported skills. For example:
        //   j = 0 -> skills 0, 3, 5 -> mask 41
        //   j = 1 -> skills 1, 4, 6 -> mask 82
        const std::uint32_t skill0 = static_cast<std::uint32_t>(j % kSkillCount);
        const std::uint32_t skill1 = static_cast<std::uint32_t>((j + 3) % kSkillCount);
        const std::uint32_t skill2 = static_cast<std::uint32_t>((j + 5) % kSkillCount);
        resource.skill_mask = (1u << skill0) | (1u << skill1) | (1u << skill2);

        resources[static_cast<std::size_t>(j)] = resource;
    }

    return resources;
}

CostMatrixValidation validate_cost_matrix(const std::vector<float>& costs,
                                          const std::vector<std::uint8_t>& feasible,
                                          const std::vector<Task>& tasks,
                                          const std::vector<Resource>& resources)
{
    CostMatrixValidation summary;

    const std::int64_t task_count = static_cast<std::int64_t>(tasks.size());
    const std::int64_t resource_count = static_cast<std::int64_t>(resources.size());
    const std::int64_t expected_cells = task_count * resource_count;

    if (static_cast<std::int64_t>(costs.size()) != expected_cells ||
        static_cast<std::int64_t>(feasible.size()) != expected_cells)
    {
        summary.correct = false;
        return summary;
    }

    for (std::int64_t task_index = 0; task_index < task_count; ++task_index)
    {
        for (std::int64_t resource_index = 0; resource_index < resource_count; ++resource_index)
        {
            const std::int64_t flat_index = task_index * resource_count + resource_index;
            const auto reference = evaluate_pair(tasks[static_cast<std::size_t>(task_index)],
                                                 resources[static_cast<std::size_t>(resource_index)]);
            const float actual_cost = costs[static_cast<std::size_t>(flat_index)];
            const std::uint8_t actual_feasible = feasible[static_cast<std::size_t>(flat_index)];

            summary.feasible_count += actual_feasible != 0 ? 1 : 0;
            summary.reference_feasible_count += reference.feasible != 0 ? 1 : 0;

            if (actual_feasible != reference.feasible)
            {
                ++summary.feasibility_mismatches;
                summary.correct = false;
            }

            if (actual_feasible != 0)
            {
                summary.checksum += static_cast<double>(actual_cost);
            }
            if (reference.feasible != 0)
            {
                summary.reference_checksum += static_cast<double>(reference.cost);
            }

            const double abs_error = std::abs(static_cast<double>(actual_cost) -
                                              static_cast<double>(reference.cost));
            const double rel_denominator = std::max(1.0, std::abs(static_cast<double>(reference.cost)));
            const double rel_error = abs_error / rel_denominator;
            summary.max_abs_error = std::max(summary.max_abs_error, abs_error);
            summary.max_rel_error = std::max(summary.max_rel_error, rel_error);

            const double tolerance = std::max(1.0e-3, std::abs(static_cast<double>(reference.cost)) * 1.0e-5);
            if (!std::isfinite(actual_cost) || abs_error > tolerance)
            {
                summary.correct = false;
            }
        }
    }

    return summary;
}

} // namespace algobench::cost_matrix
