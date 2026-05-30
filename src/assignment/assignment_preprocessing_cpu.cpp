#include "assignment/assignment_preprocessing.hpp"
#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

namespace algobench::assignment
{

std::vector<BenchmarkResult> run_assignment_preprocessing_cpu(const BenchmarkConfig& config)
{
    const auto shape = assignment_shape_for_config(config);
    const auto label = assignment_label_for_config(config);
    const auto problem = make_assignment_problem(shape);

    std::vector<TopKEntry> topk;
    for (int w = 0; w < config.warmup; ++w)
    {
        topk = compute_assignment_topk_cpu_reference(problem);
    }

    CpuTimer timer;
    timer.start();
    for (int r = 0; r < config.repeat; ++r)
    {
        topk = compute_assignment_topk_cpu_reference(problem);
    }
    const double total_ms = timer.stop_ms();

    const auto reference = compute_assignment_topk_cpu_reference(problem);
    const auto aggregate = aggregate_assignment_solution(problem, topk);
    const auto validation = validate_assignment_topk(problem, topk, reference);

    BenchmarkResult result{};
    result.benchmark = "assignment_preprocessing";
    result.variant = "cpu";
    result.preset = label;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["pairs"] = static_cast<std::int64_t>(shape.task_count) * shape.resource_count;
    result.input_size["top_k"] = shape.top_k;
    result.total_ms = total_ms;
    result.correct = validation.correct;
    result.device = cpu_device_name();
    result.notes = "CPU nested task/resource scan with per-task top-K insertion.";
    fill_assignment_metadata(result, aggregate, validation);
    return {result};
}

} // namespace algobench::assignment
