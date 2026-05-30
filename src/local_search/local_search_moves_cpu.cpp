#include "local_search/local_search_moves.hpp"
#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

namespace algobench::local_search
{

std::vector<BenchmarkResult> run_local_search_moves_cpu(const BenchmarkConfig& config)
{
    const auto shape = local_search_shape_for_config(config);
    const auto label = local_search_label_for_config(config);
    const auto problem = make_local_search_problem(shape);

    std::vector<MoveEvaluation> evaluations;
    for (int w = 0; w < config.warmup; ++w)
    {
        evaluations = evaluate_moves_cpu_reference(problem);
    }

    CpuTimer timer;
    timer.start();
    for (int r = 0; r < config.repeat; ++r)
    {
        evaluations = evaluate_moves_cpu_reference(problem);
    }
    const double total_ms = timer.stop_ms();

    const auto reference = evaluate_moves_cpu_reference(problem);
    const auto aggregate = aggregate_move_evaluations(evaluations);
    const auto validation = validate_move_evaluations(evaluations, reference);

    BenchmarkResult result{};
    result.benchmark = "local_search_moves";
    result.variant = "cpu";
    result.preset = label;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["moves"] = shape.move_count;
    result.total_ms = total_ms;
    result.correct = validation.correct;
    result.device = cpu_device_name();
    result.notes = "CPU local-search move evaluation over candidate replacement moves.";
    fill_local_search_metadata(result, aggregate, validation);
    return {result};
}

} // namespace algobench::local_search
