#include "constraints/constraint_network.hpp"

#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

#include <string>
#include <vector>

namespace algobench::constraints
{

std::vector<BenchmarkResult> run_constraint_network_cpu(const BenchmarkConfig& config)
{
    const auto shape = constraint_shape_for_config(config);
    const auto problem = make_constraint_problem(shape);
    std::vector<CandidateEvaluation> evaluations(problem.candidates.size());

    for (int i = 0; i < config.warmup; ++i)
    {
        evaluations = evaluate_candidates_cpu_reference(problem);
    }

    CpuTimer timer;
    timer.start();
    for (int r = 0; r < config.repeat; ++r)
    {
        evaluations = evaluate_candidates_cpu_reference(problem);
    }
    const double elapsed_ms = timer.stop_ms();

    const auto validation = validate_constraint_evaluations(problem, evaluations);

    BenchmarkResult result;
    result.benchmark = "constraint_network";
    result.variant = "cpu";
    result.preset = constraint_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["candidates"] = shape.candidate_count;
    result.total_ms = elapsed_ms;
    result.correct = validation.correct;
    result.device = cpu_device_name();
    result.notes = "candidate assignment validation using CPU loop";
    fill_constraint_metadata(result, validation);
    return {result};
}

} // namespace algobench::constraints
