#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"

#include <cstdint>
#include <string>
#include <vector>

#if defined(__CUDACC__)
#define ALGOBENCH_HD __host__ __device__ inline
#else
#define ALGOBENCH_HD inline
#endif

namespace algobench::assignment
{

constexpr float kAssignmentInfeasibleCost = 1.0e30f;

constexpr std::uint32_t kAssignViolationSkill = 1u << 0;
constexpr std::uint32_t kAssignViolationCapacity = 1u << 1;
constexpr std::uint32_t kAssignViolationTime = 1u << 2;
constexpr std::uint32_t kAssignViolationDistance = 1u << 3;
constexpr std::uint32_t kAssignViolationZone = 1u << 4;
constexpr std::uint32_t kAssignViolationRisk = 1u << 5;

struct AssignmentShape
{
    std::int32_t task_count = 0;
    std::int32_t resource_count = 0;
    std::int32_t top_k = 8;
};

struct AssignmentTask
{
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t required_skill_mask = 0;
    std::int32_t demand = 0;
    std::int32_t earliest = 0;
    std::int32_t latest = 0;
    std::int32_t duration = 0;
    std::int32_t zone_id = 0;
    float priority = 0.0f;
    float risk = 0.0f;
};

struct AssignmentResource
{
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t skill_mask = 0;
    std::int32_t capacity = 0;
    std::int32_t available_start = 0;
    std::int32_t available_end = 0;
    std::uint32_t forbidden_zone_mask = 0;
    float max_distance = 0.0f;
    float risk_tolerance = 0.0f;
    float load = 0.0f;
};

struct AssignmentPairEvaluation
{
    std::uint8_t feasible = 0;
    std::uint32_t violation_mask = 0;
    float cost = kAssignmentInfeasibleCost;
};

struct TopKEntry
{
    std::int32_t resource_index = -1;
    float cost = kAssignmentInfeasibleCost;
};

struct AssignmentProblem
{
    AssignmentShape shape;
    std::vector<AssignmentTask> tasks;
    std::vector<AssignmentResource> resources;
};

struct AssignmentAggregate
{
    std::int64_t feasible_pair_count = 0;
    std::int64_t infeasible_pair_count = 0;
    std::int64_t selected_candidate_count = 0;
    std::int64_t tasks_with_any_candidate = 0;
    std::int64_t skill_violations = 0;
    std::int64_t capacity_violations = 0;
    std::int64_t time_violations = 0;
    std::int64_t distance_violations = 0;
    std::int64_t zone_violations = 0;
    std::int64_t risk_violations = 0;
    double selected_cost_checksum = 0.0;
    double best_cost_sum = 0.0;
    double mean_selected_cost = 0.0;
};

struct AssignmentValidation
{
    bool correct = true;
    std::int64_t selected_resource_mismatches = 0;
    std::int64_t feasible_count_mismatch = 0;
    std::int64_t tasks_with_any_mismatch = 0;
    double checksum = 0.0;
    double reference_checksum = 0.0;
    double max_abs_error = 0.0;
    double max_rel_error = 0.0;
};

ALGOBENCH_HD float assignment_squared_distance(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

ALGOBENCH_HD AssignmentPairEvaluation evaluate_assignment_pair(const AssignmentTask& task,
                                                               const AssignmentResource& resource)
{
    AssignmentPairEvaluation out{};
    std::uint32_t mask = 0;
    float cost = 0.0f;

    if ((resource.skill_mask & task.required_skill_mask) != task.required_skill_mask)
    {
        mask |= kAssignViolationSkill;
        cost += 3000.0f;
    }
    if (resource.capacity < task.demand)
    {
        mask |= kAssignViolationCapacity;
        cost += 180.0f * static_cast<float>(task.demand - resource.capacity);
    }

    const std::int32_t proposed_start = task.earliest + static_cast<std::int32_t>((resource.load * 17.0f));
    const std::int32_t proposed_end = proposed_start + task.duration;
    if (proposed_start < resource.available_start || proposed_end > resource.available_end || proposed_end > task.latest)
    {
        mask |= kAssignViolationTime;
        const std::int32_t late_resource = proposed_end - resource.available_end;
        const std::int32_t late_task = proposed_end - task.latest;
        cost += 4.0f * static_cast<float>((late_resource > 0 ? late_resource : 0) + (late_task > 0 ? late_task : 0));
    }

    const float distance2 = assignment_squared_distance(task.x, task.y, resource.x, resource.y);
    const float max_distance2 = resource.max_distance * resource.max_distance;
    if (distance2 > max_distance2)
    {
        mask |= kAssignViolationDistance;
        cost += 0.006f * (distance2 - max_distance2);
    }

    const std::uint32_t zone_bit = 1u << static_cast<std::uint32_t>(task.zone_id & 31);
    if ((resource.forbidden_zone_mask & zone_bit) != 0u)
    {
        mask |= kAssignViolationZone;
        cost += 900.0f;
    }

    if (task.risk > resource.risk_tolerance)
    {
        mask |= kAssignViolationRisk;
        cost += 1200.0f * (task.risk - resource.risk_tolerance);
    }

    // Base objective for feasible pairs. Lower is better. Priority lowers cost,
    // but load, distance, and risk increase it.
    cost += 0.035f * distance2;
    cost += 40.0f * resource.load;
    cost += 75.0f * task.risk;
    cost -= 18.0f * task.priority;
    cost += 0.1f * static_cast<float>(task.duration);

    out.violation_mask = mask;
    out.feasible = mask == 0u ? 1u : 0u;
    out.cost = mask == 0u ? cost : kAssignmentInfeasibleCost;
    return out;
}

AssignmentShape assignment_shape_for_config(const BenchmarkConfig& config);
std::string assignment_label_for_config(const BenchmarkConfig& config);
AssignmentProblem make_assignment_problem(const AssignmentShape& shape);

std::vector<TopKEntry> compute_assignment_topk_cpu_reference(const AssignmentProblem& problem);
AssignmentAggregate aggregate_assignment_solution(const AssignmentProblem& problem,
                                                  const std::vector<TopKEntry>& topk);
AssignmentValidation validate_assignment_topk(const AssignmentProblem& problem,
                                              const std::vector<TopKEntry>& actual,
                                              const std::vector<TopKEntry>& reference);
void fill_assignment_metadata(BenchmarkResult& result,
                              const AssignmentAggregate& aggregate,
                              const AssignmentValidation& validation);

std::vector<BenchmarkResult> run_assignment_preprocessing_cpu(const BenchmarkConfig& config);
std::vector<BenchmarkResult> run_assignment_preprocessing_gpu(const BenchmarkConfig& config);

} // namespace algobench::assignment

#undef ALGOBENCH_HD
