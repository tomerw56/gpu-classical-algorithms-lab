#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__CUDACC__)
#define ALGOBENCH_COST_HD __host__ __device__
#else
#define ALGOBENCH_COST_HD
#endif

namespace algobench::cost_matrix
{

constexpr std::uint32_t kSkillCount = 8;
constexpr float kInfeasibleCost = 1.0e30f;

/**
 * A synthetic task used by the cost-matrix benchmark.
 *
 * The benchmark treats a task as a destination plus a small set of business
 * attributes. `required_skill` stores exactly one bit, for example `1u << 3`,
 * while `urgent` selects tighter dispatch and lateness rules.
 */
struct Task
{
    float x = 0.0f;                        ///< Task x-coordinate in the synthetic map.
    float y = 0.0f;                        ///< Task y-coordinate in the synthetic map.
    float deadline = 0.0f;                 ///< Desired arrival time.
    float priority = 1.0f;                 ///< Weight that increases the final cost.
    std::uint32_t required_skill = 0;      ///< Single-bit skill requirement.
    std::uint8_t urgent = 0;               ///< Nonzero for urgent tasks.
};

/**
 * A synthetic resource used by the cost-matrix benchmark.
 *
 * Resources mimic workers, vehicles, or agents. `skill_mask` may contain
 * several bits, so a resource can satisfy several different task skill types.
 * For example, a mask of `(1u << 0) | (1u << 3) | (1u << 5)` can serve tasks
 * requiring skill 0, 3, or 5.
 */
struct Resource
{
    float x = 0.0f;                        ///< Resource x-coordinate in the synthetic map.
    float y = 0.0f;                        ///< Resource y-coordinate in the synthetic map.
    float load = 0.0f;                     ///< Current utilization ratio, approximately [0, 1].
    float speed = 1.0f;                    ///< Travel speed used to convert distance to arrival time.
    float available_at = 0.0f;             ///< Earliest time at which the resource can depart.
    std::uint32_t skill_mask = 0;          ///< Multi-bit set of skills this resource can satisfy.
};

/**
 * Dense matrix dimensions for `cost[task][resource]`.
 *
 * Both CPU and GPU implementations use a row-major flat array layout:
 *
 *     flat_index = task_index * resource_count + resource_index
 *
 * Example: in a matrix with 4 resources, cell `(task=2, resource=3)` is stored
 * at flat index `2 * 4 + 3 = 11`.
 */
struct CostMatrixShape
{
    std::int64_t task_count = 0;
    std::int64_t resource_count = 0;

    std::int64_t cell_count() const
    {
        return task_count * resource_count;
    }
};

struct PairEvaluation
{
    float cost = kInfeasibleCost;
    std::uint8_t feasible = 0;
};

struct CostMatrixValidation
{
    double checksum = 0.0;
    double reference_checksum = 0.0;
    double max_abs_error = 0.0;
    double max_rel_error = 0.0;
    std::int64_t feasible_count = 0;
    std::int64_t reference_feasible_count = 0;
    std::int64_t feasibility_mismatches = 0;
    bool correct = true;
};

/** Return the dense matrix dimensions associated with a benchmark preset. */
CostMatrixShape shape_for_preset(const std::string& preset);

/**
 * Build deterministic synthetic tasks for the benchmark.
 *
 * The generator intentionally avoids randomness so CPU and GPU runs receive the
 * same inputs across executions. Task coordinates, deadlines, priorities, skill
 * requirements, and urgency flags all follow fixed arithmetic patterns.
 */
std::vector<Task> make_tasks(std::int64_t task_count);

/**
 * Build deterministic synthetic resources for the benchmark.
 *
 * Each generated resource contains:
 *
 * - a reproducible `(x, y)` location,
 * - a load ratio used by the nonlinear load penalty,
 * - a speed and availability time used by arrival-time calculations,
 * - a three-bit skill mask.
 *
 * Example: resource 0 receives skills `{0, 3, 5}`, which produces the mask
 * `(1u << 0) | (1u << 3) | (1u << 5) == 41`.
 */
std::vector<Resource> make_resources(std::int64_t resource_count);

/**
 * Validate a row-major dense matrix against the shared CPU reference evaluator.
 *
 * `costs` and `feasible` must each contain exactly
 * `tasks.size() * resources.size()` cells, laid out with:
 *
 *     flat_index = task_index * resource_count + resource_index
 */
CostMatrixValidation validate_cost_matrix(const std::vector<float>& costs,
                                          const std::vector<std::uint8_t>& feasible,
                                          const std::vector<Task>& tasks,
                                          const std::vector<Resource>& resources);

std::vector<BenchmarkResult> run_cost_matrix_cpu(const BenchmarkConfig& config);
std::vector<BenchmarkResult> run_cost_matrix_gpu(const BenchmarkConfig& config);

ALGOBENCH_COST_HD inline float cost_max(float lhs, float rhs)
{
    return lhs > rhs ? lhs : rhs;
}

ALGOBENCH_COST_HD inline float cost_abs(float value)
{
    return value < 0.0f ? -value : value;
}

ALGOBENCH_COST_HD inline float cost_sqrt(float value)
{
#if defined(__CUDA_ARCH__)
    return sqrtf(value);
#else
    return static_cast<float>(std::sqrt(static_cast<double>(value)));
#endif
}

/**
 * Evaluate one task/resource pair.
 *
 * The same function is compiled for the CPU reference path and the CUDA kernel,
 * keeping the cost semantics aligned. It returns `{kInfeasibleCost, 0}` when any
 * hard rejection rule fails, or `{computed_cost, 1}` for a feasible pair.
 */
ALGOBENCH_COST_HD inline PairEvaluation evaluate_pair(const Task& task, const Resource& resource)
{
    PairEvaluation evaluation;

    // Branch 1: hard skill compatibility.
    const bool skill_ok = (resource.skill_mask & task.required_skill) != 0u;
    if (!skill_ok)
    {
        return evaluation;
    }

    const float dx = task.x - resource.x;
    const float dy = task.y - resource.y;
    const float distance = cost_sqrt(dx * dx + dy * dy);

    // Branch 2: urgent tasks are only considered within a tighter dispatch radius.
    const float dispatch_limit = task.urgent != 0 ? 620.0f : 980.0f;
    if (distance > dispatch_limit)
    {
        return evaluation;
    }

    const float travel_time = distance / resource.speed;
    const float arrival_time = resource.available_at + travel_time;
    const float lateness = cost_max(0.0f, arrival_time - task.deadline);

    // Branch 3: very late arrivals are rejected outright.
    const float lateness_limit = task.urgent != 0 ? 80.0f : 240.0f;
    if (lateness > lateness_limit)
    {
        return evaluation;
    }

    float cost = 0.0f;
    cost += distance * 0.11f;
    cost += travel_time * 0.65f;
    cost += resource.load * 31.0f;
    cost += task.priority * 7.0f;

    // Branch 4: urgent and non-urgent tasks use different lateness penalties.
    if (task.urgent != 0)
    {
        cost += lateness * 2.75f;
        cost += 15.0f;
    }
    else
    {
        cost += lateness * 0.85f;
    }

    // Branch 5: heavily loaded resources receive a nonlinear penalty.
    if (resource.load > 0.82f)
    {
        cost += 115.0f;
    }
    else if (resource.load > 0.62f)
    {
        cost += 35.0f;
    }

    // Branch 6: a small location-dependent penalty mimics zone/risk effects.
    const float zone_proxy = cost_abs(task.x - resource.y);
    if (zone_proxy > 410.0f)
    {
        cost += 22.0f;
    }

    evaluation.cost = cost;
    evaluation.feasible = 1;
    return evaluation;
}

} // namespace algobench::cost_matrix

#undef ALGOBENCH_COST_HD
