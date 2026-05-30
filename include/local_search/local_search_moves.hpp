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

namespace algobench::local_search
{

constexpr std::uint32_t kMoveViolationSkill = 1u << 0;
constexpr std::uint32_t kMoveViolationCapacity = 1u << 1;
constexpr std::uint32_t kMoveViolationDistance = 1u << 2;
constexpr std::uint32_t kMoveViolationRisk = 1u << 3;
constexpr std::uint32_t kMoveViolationNoChange = 1u << 4;

struct LocalSearchShape
{
    std::int32_t task_count = 0;
    std::int32_t resource_count = 0;
    std::int64_t move_count = 0;
};

struct SearchTask
{
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t required_skill_mask = 0;
    std::int32_t demand = 0;
    float priority = 0.0f;
    float risk = 0.0f;
};

struct SearchResource
{
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t skill_mask = 0;
    std::int32_t capacity = 0;
    float max_distance = 0.0f;
    float risk_tolerance = 0.0f;
    float load = 0.0f;
};

struct CandidateMove
{
    std::int32_t task_index = 0;
    std::int32_t new_resource_index = 0;
};

struct MoveEvaluation
{
    std::uint8_t valid = 0;
    std::uint8_t improving = 0;
    std::uint32_t violation_mask = 0;
    float old_cost = 0.0f;
    float new_cost = 0.0f;
    float delta = 0.0f;
};

struct LocalSearchProblem
{
    LocalSearchShape shape;
    std::vector<SearchTask> tasks;
    std::vector<SearchResource> resources;
    std::vector<std::int32_t> current_assignment;
    std::vector<CandidateMove> moves;
};

struct LocalSearchAggregate
{
    std::int64_t valid_moves = 0;
    std::int64_t invalid_moves = 0;
    std::int64_t improving_moves = 0;
    std::int64_t skill_violations = 0;
    std::int64_t capacity_violations = 0;
    std::int64_t distance_violations = 0;
    std::int64_t risk_violations = 0;
    std::int64_t no_change_violations = 0;
    double delta_checksum = 0.0;
    double valid_delta_sum = 0.0;
    double best_delta = 0.0;
    std::int64_t best_move_index = -1;
};

struct LocalSearchValidation
{
    bool correct = true;
    std::int64_t valid_mismatches = 0;
    std::int64_t improving_mismatches = 0;
    std::int64_t violation_mismatches = 0;
    double checksum = 0.0;
    double reference_checksum = 0.0;
    double max_abs_error = 0.0;
    double max_rel_error = 0.0;
};

ALGOBENCH_HD float move_squared_distance(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

ALGOBENCH_HD float pair_soft_cost(const SearchTask& task, const SearchResource& resource)
{
    const float d2 = move_squared_distance(task.x, task.y, resource.x, resource.y);
    return 0.030f * d2 + 55.0f * resource.load + 90.0f * task.risk - 20.0f * task.priority + 3.0f * static_cast<float>(task.demand);
}

ALGOBENCH_HD std::uint32_t pair_violation_mask(const SearchTask& task, const SearchResource& resource)
{
    std::uint32_t mask = 0;
    if ((resource.skill_mask & task.required_skill_mask) != task.required_skill_mask)
    {
        mask |= kMoveViolationSkill;
    }
    if (resource.capacity < task.demand)
    {
        mask |= kMoveViolationCapacity;
    }
    const float d2 = move_squared_distance(task.x, task.y, resource.x, resource.y);
    if (d2 > resource.max_distance * resource.max_distance)
    {
        mask |= kMoveViolationDistance;
    }
    if (task.risk > resource.risk_tolerance)
    {
        mask |= kMoveViolationRisk;
    }
    return mask;
}

ALGOBENCH_HD MoveEvaluation evaluate_candidate_move(const SearchTask& task,
                                                    const SearchResource& old_resource,
                                                    const SearchResource& new_resource,
                                                    std::int32_t old_resource_index,
                                                    std::int32_t new_resource_index)
{
    MoveEvaluation out{};
    std::uint32_t mask = pair_violation_mask(task, new_resource);
    if (old_resource_index == new_resource_index)
    {
        mask |= kMoveViolationNoChange;
    }

    out.old_cost = pair_soft_cost(task, old_resource);
    out.new_cost = pair_soft_cost(task, new_resource);
    out.delta = out.new_cost - out.old_cost;
    out.violation_mask = mask;
    out.valid = mask == 0u ? 1u : 0u;
    out.improving = (out.valid != 0u && out.delta < -1.0e-5f) ? 1u : 0u;
    return out;
}

LocalSearchShape local_search_shape_for_config(const BenchmarkConfig& config);
std::string local_search_label_for_config(const BenchmarkConfig& config);
LocalSearchProblem make_local_search_problem(const LocalSearchShape& shape);
std::vector<MoveEvaluation> evaluate_moves_cpu_reference(const LocalSearchProblem& problem);
LocalSearchAggregate aggregate_move_evaluations(const std::vector<MoveEvaluation>& evaluations);
LocalSearchValidation validate_move_evaluations(const std::vector<MoveEvaluation>& actual,
                                                const std::vector<MoveEvaluation>& reference);
void fill_local_search_metadata(BenchmarkResult& result,
                                const LocalSearchAggregate& aggregate,
                                const LocalSearchValidation& validation);

std::vector<BenchmarkResult> run_local_search_moves_cpu(const BenchmarkConfig& config);
std::vector<BenchmarkResult> run_local_search_moves_gpu(const BenchmarkConfig& config);

} // namespace algobench::local_search

#undef ALGOBENCH_HD
