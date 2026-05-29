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

namespace algobench::constraints
{

constexpr std::uint32_t kViolationSkill = 1u << 0;
constexpr std::uint32_t kViolationCapacity = 1u << 1;
constexpr std::uint32_t kViolationTaskWindow = 1u << 2;
constexpr std::uint32_t kViolationResourceWindow = 1u << 3;
constexpr std::uint32_t kViolationDistance = 1u << 4;
constexpr std::uint32_t kViolationForbiddenZone = 1u << 5;
constexpr std::uint32_t kViolationRisk = 1u << 6;

struct ConstraintShape
{
    std::int64_t task_count = 0;
    std::int64_t resource_count = 0;
    std::int64_t candidate_count = 0;
};

struct Task
{
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t required_skill_mask = 0;
    std::int32_t required_capacity = 0;
    std::int32_t earliest_start = 0;
    std::int32_t latest_end = 0;
    std::int32_t zone_id = 0;
    float risk = 0.0f;
};

struct Resource
{
    float home_x = 0.0f;
    float home_y = 0.0f;
    std::uint32_t skill_mask = 0;
    std::int32_t capacity = 0;
    std::int32_t available_start = 0;
    std::int32_t available_end = 0;
    std::uint32_t forbidden_zone_mask = 0;
    float max_distance = 0.0f;
    float risk_tolerance = 0.0f;
};

struct CandidateAssignment
{
    std::int32_t task_index = 0;
    std::int32_t resource_index = 0;
    std::int32_t start_time = 0;
    std::int32_t duration = 0;
};

struct CandidateEvaluation
{
    std::uint8_t valid = 0;
    std::int32_t violation_count = 0;
    std::uint32_t violation_mask = 0;
    float penalty = 0.0f;
};

struct ConstraintProblem
{
    ConstraintShape shape;
    std::vector<Task> tasks;
    std::vector<Resource> resources;
    std::vector<CandidateAssignment> candidates;
};

struct ConstraintValidation
{
    bool correct = true;
    std::int64_t valid_count = 0;
    std::int64_t reference_valid_count = 0;
    std::int64_t invalid_count = 0;
    std::int64_t total_violations = 0;
    std::int64_t reference_total_violations = 0;

    std::int64_t skill_violations = 0;
    std::int64_t capacity_violations = 0;
    std::int64_t task_window_violations = 0;
    std::int64_t resource_window_violations = 0;
    std::int64_t distance_violations = 0;
    std::int64_t forbidden_zone_violations = 0;
    std::int64_t risk_violations = 0;

    std::int64_t mask_mismatches = 0;
    std::int64_t validity_mismatches = 0;
    std::int64_t violation_count_mismatches = 0;
    double checksum = 0.0;
    double reference_checksum = 0.0;
    double max_abs_error = 0.0;
    double max_rel_error = 0.0;
};

ALGOBENCH_HD float squared_distance(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

ALGOBENCH_HD std::int32_t popcount7(std::uint32_t mask)
{
    std::int32_t count = 0;
    for (std::int32_t i = 0; i < 7; ++i)
    {
        count += static_cast<std::int32_t>((mask >> i) & 1u);
    }
    return count;
}

ALGOBENCH_HD CandidateEvaluation evaluate_candidate(const Task& task,
                                                    const Resource& resource,
                                                    const CandidateAssignment& candidate)
{
    CandidateEvaluation out{};
    const std::int32_t end_time = candidate.start_time + candidate.duration;
    const float distance2 = squared_distance(task.x, task.y, resource.home_x, resource.home_y);
    const float max_distance2 = resource.max_distance * resource.max_distance;

    std::uint32_t mask = 0;
    float penalty = 0.0f;

    if ((resource.skill_mask & task.required_skill_mask) != task.required_skill_mask)
    {
        mask |= kViolationSkill;
        penalty += 1200.0f;
    }

    if (resource.capacity < task.required_capacity)
    {
        mask |= kViolationCapacity;
        penalty += 150.0f * static_cast<float>(task.required_capacity - resource.capacity);
    }

    if (candidate.start_time < task.earliest_start || end_time > task.latest_end)
    {
        mask |= kViolationTaskWindow;
        const std::int32_t early = task.earliest_start - candidate.start_time;
        const std::int32_t late = end_time - task.latest_end;
        penalty += 3.0f * static_cast<float>((early > 0 ? early : 0) + (late > 0 ? late : 0));
    }

    if (candidate.start_time < resource.available_start || end_time > resource.available_end)
    {
        mask |= kViolationResourceWindow;
        const std::int32_t early = resource.available_start - candidate.start_time;
        const std::int32_t late = end_time - resource.available_end;
        penalty += 2.0f * static_cast<float>((early > 0 ? early : 0) + (late > 0 ? late : 0));
    }

    if (distance2 > max_distance2)
    {
        mask |= kViolationDistance;
        penalty += 0.01f * (distance2 - max_distance2);
    }

    const std::uint32_t zone_bit = 1u << static_cast<std::uint32_t>(task.zone_id & 31);
    if ((resource.forbidden_zone_mask & zone_bit) != 0u)
    {
        mask |= kViolationForbiddenZone;
        penalty += 450.0f;
    }

    if (task.risk > resource.risk_tolerance)
    {
        mask |= kViolationRisk;
        penalty += 700.0f * (task.risk - resource.risk_tolerance);
    }

    // Keep a small continuous score for valid candidates too, so validation
    // checks both categorical constraint state and floating-point arithmetic.
    penalty += 0.00025f * distance2 + 0.05f * static_cast<float>(candidate.duration);

    out.violation_mask = mask;
    out.violation_count = popcount7(mask);
    out.valid = mask == 0u ? 1u : 0u;
    out.penalty = penalty;
    return out;
}

ConstraintShape constraint_shape_for_config(const BenchmarkConfig& config);
std::string constraint_label_for_config(const BenchmarkConfig& config);
ConstraintProblem make_constraint_problem(const ConstraintShape& shape);

std::vector<CandidateEvaluation> evaluate_candidates_cpu_reference(const ConstraintProblem& problem);
ConstraintValidation validate_constraint_evaluations(const ConstraintProblem& problem,
                                                     const std::vector<CandidateEvaluation>& evaluations);
void fill_constraint_metadata(BenchmarkResult& result, const ConstraintValidation& validation);

std::vector<BenchmarkResult> run_constraint_network_cpu(const BenchmarkConfig& config);
std::vector<BenchmarkResult> run_constraint_network_gpu(const BenchmarkConfig& config);

} // namespace algobench::constraints

#undef ALGOBENCH_HD
