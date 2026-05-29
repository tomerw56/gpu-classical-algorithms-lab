#include "constraints/constraint_network.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>

namespace algobench::constraints
{
namespace
{

constexpr double kPenaltyAbsTolerance = 5.0e-2;
constexpr double kPenaltyRelTolerance = 1.0e-5;

std::int64_t param_i64(const BenchmarkConfig& config, const std::string& key, std::int64_t fallback)
{
    const auto it = config.params.find(key);
    if (it == config.params.end())
    {
        return fallback;
    }
    const long long value = std::atoll(it->second.c_str());
    return value > 0 ? static_cast<std::int64_t>(value) : fallback;
}

std::uint32_t skill_bit(std::int64_t index)
{
    return 1u << static_cast<std::uint32_t>(index & 7);
}

} // namespace

ConstraintShape constraint_shape_for_config(const BenchmarkConfig& config)
{
    ConstraintShape shape;
    if (config.preset == "tiny")
    {
        shape.task_count = 64;
        shape.resource_count = 32;
        shape.candidate_count = 4'096;
    }
    else if (config.preset == "small")
    {
        shape.task_count = 512;
        shape.resource_count = 256;
        shape.candidate_count = 262'144;
    }
    else if (config.preset == "medium")
    {
        shape.task_count = 2'048;
        shape.resource_count = 1'024;
        shape.candidate_count = 1'048'576;
    }
    else if (config.preset == "large")
    {
        shape.task_count = 8'192;
        shape.resource_count = 2'048;
        shape.candidate_count = 4'194'304;
    }
    else
    {
        shape.task_count = 512;
        shape.resource_count = 256;
        shape.candidate_count = 262'144;
    }

    shape.task_count = param_i64(config, "tasks", shape.task_count);
    shape.resource_count = param_i64(config, "resources", shape.resource_count);
    shape.candidate_count = param_i64(config, "candidates", shape.candidate_count);
    return shape;
}

std::string constraint_label_for_config(const BenchmarkConfig& config)
{
    const auto it = config.params.find("sweep_label");
    if (it != config.params.end() && !it->second.empty())
    {
        return it->second;
    }
    return config.preset;
}

ConstraintProblem make_constraint_problem(const ConstraintShape& shape)
{
    ConstraintProblem problem;
    problem.shape = shape;
    problem.tasks.resize(static_cast<std::size_t>(shape.task_count));
    problem.resources.resize(static_cast<std::size_t>(shape.resource_count));
    problem.candidates.resize(static_cast<std::size_t>(shape.candidate_count));

    for (std::int64_t i = 0; i < shape.task_count; ++i)
    {
        Task task;
        task.x = static_cast<float>((i * 37 + 11) % 2000) * 0.5f;
        task.y = static_cast<float>((i * 53 + 19) % 2000) * 0.5f;
        task.required_skill_mask = skill_bit(i) | (((i % 5) == 0) ? skill_bit(i + 3) : 0u);
        task.required_capacity = 1 + static_cast<std::int32_t>((i * 7) % 8);
        task.earliest_start = static_cast<std::int32_t>((i * 13) % 900);
        task.latest_end = task.earliest_start + 90 + static_cast<std::int32_t>((i % 6) * 30);
        task.zone_id = static_cast<std::int32_t>(i % 8);
        task.risk = 0.15f + static_cast<float>((i * 29) % 85) / 100.0f;
        problem.tasks[static_cast<std::size_t>(i)] = task;
    }

    for (std::int64_t i = 0; i < shape.resource_count; ++i)
    {
        Resource resource;
        resource.home_x = static_cast<float>((i * 97 + 23) % 2000) * 0.5f;
        resource.home_y = static_cast<float>((i * 71 + 41) % 2000) * 0.5f;
        resource.skill_mask = skill_bit(i) | skill_bit(i * 3 + 1) | (((i % 4) == 0) ? skill_bit(i + 5) : 0u);
        resource.capacity = 2 + static_cast<std::int32_t>((i * 5) % 10);
        resource.available_start = static_cast<std::int32_t>((i % 6) * 40);
        resource.available_end = 1'120 + static_cast<std::int32_t>((i % 5) * 50);
        resource.forbidden_zone_mask = 1u << static_cast<std::uint32_t>((i * 5 + 2) % 8);
        if ((i % 9) == 0)
        {
            resource.forbidden_zone_mask |= 1u << static_cast<std::uint32_t>((i + 3) % 8);
        }
        resource.max_distance = 180.0f + static_cast<float>((i % 7) * 55);
        resource.risk_tolerance = 0.35f + static_cast<float>((i * 11) % 65) / 100.0f;
        problem.resources[static_cast<std::size_t>(i)] = resource;
    }

    for (std::int64_t i = 0; i < shape.candidate_count; ++i)
    {
        CandidateAssignment candidate;
        candidate.task_index = static_cast<std::int32_t>((i * 37 + 17) % shape.task_count);
        candidate.resource_index = static_cast<std::int32_t>((i * 53 + 3) % shape.resource_count);
        candidate.start_time = static_cast<std::int32_t>((i * 11 + (i / 17) * 23) % 1'300);
        candidate.duration = 20 + static_cast<std::int32_t>((i * 7 + candidate.task_index) % 100);
        problem.candidates[static_cast<std::size_t>(i)] = candidate;
    }

    return problem;
}

std::vector<CandidateEvaluation> evaluate_candidates_cpu_reference(const ConstraintProblem& problem)
{
    std::vector<CandidateEvaluation> out(problem.candidates.size());
    for (std::size_t i = 0; i < problem.candidates.size(); ++i)
    {
        const auto& candidate = problem.candidates[i];
        out[i] = evaluate_candidate(problem.tasks[static_cast<std::size_t>(candidate.task_index)],
                                    problem.resources[static_cast<std::size_t>(candidate.resource_index)],
                                    candidate);
    }
    return out;
}

ConstraintValidation validate_constraint_evaluations(const ConstraintProblem& problem,
                                                     const std::vector<CandidateEvaluation>& evaluations)
{
    ConstraintValidation summary;
    const auto reference = evaluate_candidates_cpu_reference(problem);
    summary.correct = evaluations.size() == reference.size();
    if (!summary.correct)
    {
        return summary;
    }

    for (std::size_t i = 0; i < evaluations.size(); ++i)
    {
        const auto actual = evaluations[i];
        const auto expected = reference[i];
        summary.valid_count += actual.valid ? 1 : 0;
        summary.reference_valid_count += expected.valid ? 1 : 0;
        summary.invalid_count += actual.valid ? 0 : 1;
        summary.total_violations += actual.violation_count;
        summary.reference_total_violations += expected.violation_count;

        summary.skill_violations += (actual.violation_mask & kViolationSkill) != 0u ? 1 : 0;
        summary.capacity_violations += (actual.violation_mask & kViolationCapacity) != 0u ? 1 : 0;
        summary.task_window_violations += (actual.violation_mask & kViolationTaskWindow) != 0u ? 1 : 0;
        summary.resource_window_violations += (actual.violation_mask & kViolationResourceWindow) != 0u ? 1 : 0;
        summary.distance_violations += (actual.violation_mask & kViolationDistance) != 0u ? 1 : 0;
        summary.forbidden_zone_violations += (actual.violation_mask & kViolationForbiddenZone) != 0u ? 1 : 0;
        summary.risk_violations += (actual.violation_mask & kViolationRisk) != 0u ? 1 : 0;

        summary.checksum += static_cast<double>(actual.penalty) + static_cast<double>(actual.violation_mask) * 0.125;
        summary.reference_checksum += static_cast<double>(expected.penalty) + static_cast<double>(expected.violation_mask) * 0.125;

        if (actual.valid != expected.valid)
        {
            ++summary.validity_mismatches;
            summary.correct = false;
        }
        if (actual.violation_mask != expected.violation_mask)
        {
            ++summary.mask_mismatches;
            summary.correct = false;
        }
        if (actual.violation_count != expected.violation_count)
        {
            ++summary.violation_count_mismatches;
            summary.correct = false;
        }

        const double abs_error = std::abs(static_cast<double>(actual.penalty) - static_cast<double>(expected.penalty));
        const double rel_error = abs_error / std::max(1.0, std::abs(static_cast<double>(expected.penalty)));
        summary.max_abs_error = std::max(summary.max_abs_error, abs_error);
        summary.max_rel_error = std::max(summary.max_rel_error, rel_error);
        // CPU and CUDA both evaluate the same single-precision expression,
        // but the generated machine code may differ slightly because the CUDA
        // compiler can fuse multiply-add style operations differently from the
        // host compiler. The categorical outputs remain exact; the penalty is
        // validated with combined absolute/relative tolerances so larger
        // penalty magnitudes do not fail due only to harmless float rounding.
        if (!std::isfinite(actual.penalty) ||
            (abs_error > kPenaltyAbsTolerance && rel_error > kPenaltyRelTolerance))
        {
            summary.correct = false;
        }
    }

    return summary;
}

void fill_constraint_metadata(BenchmarkResult& result, const ConstraintValidation& validation)
{
    result.metadata["valid_count"] = std::to_string(validation.valid_count);
    result.metadata["reference_valid_count"] = std::to_string(validation.reference_valid_count);
    result.metadata["invalid_count"] = std::to_string(validation.invalid_count);
    result.metadata["total_violations"] = std::to_string(validation.total_violations);
    result.metadata["reference_total_violations"] = std::to_string(validation.reference_total_violations);
    result.metadata["skill_violations"] = std::to_string(validation.skill_violations);
    result.metadata["capacity_violations"] = std::to_string(validation.capacity_violations);
    result.metadata["task_window_violations"] = std::to_string(validation.task_window_violations);
    result.metadata["resource_window_violations"] = std::to_string(validation.resource_window_violations);
    result.metadata["distance_violations"] = std::to_string(validation.distance_violations);
    result.metadata["forbidden_zone_violations"] = std::to_string(validation.forbidden_zone_violations);
    result.metadata["risk_violations"] = std::to_string(validation.risk_violations);
    result.metadata["validity_mismatches"] = std::to_string(validation.validity_mismatches);
    result.metadata["mask_mismatches"] = std::to_string(validation.mask_mismatches);
    result.metadata["violation_count_mismatches"] = std::to_string(validation.violation_count_mismatches);
    result.metadata["checksum"] = std::to_string(validation.checksum);
    result.metadata["reference_checksum"] = std::to_string(validation.reference_checksum);
    result.metadata["max_abs_error"] = std::to_string(validation.max_abs_error);
    result.metadata["max_rel_error"] = std::to_string(validation.max_rel_error);
    result.metadata["constraint_model"] = "skill, capacity, task window, resource window, distance, zone, risk";
    result.metadata["validation"] = "element-wise candidate validity, violation mask, violation count, and penalty comparison";
    result.metadata["penalty_abs_tolerance"] = std::to_string(kPenaltyAbsTolerance);
    result.metadata["penalty_rel_tolerance"] = std::to_string(kPenaltyRelTolerance);
}

} // namespace algobench::constraints
