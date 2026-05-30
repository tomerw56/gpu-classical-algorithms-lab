#include "combinations/combination_finder.hpp"

#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

namespace algobench::combinations
{

std::vector<BenchmarkResult> run_combination_finder_cpu(const BenchmarkConfig& config)
{
    const auto shape = combination_shape_for_config(config);
    const auto problem = make_combination_problem(shape);

    CombinationAggregate aggregate;
    for (int i = 0; i < config.warmup; ++i)
    {
        aggregate = evaluate_combinations_cpu_reference(problem);
    }

    CpuTimer timer;
    timer.start();
    for (int r = 0; r < config.repeat; ++r)
    {
        aggregate = evaluate_combinations_cpu_reference(problem);
    }
    const double elapsed_ms = timer.stop_ms();

    const auto reference = evaluate_combinations_cpu_reference(problem);

    BenchmarkResult result;
    result.benchmark = "combination_finder";
    result.variant = "cpu";
    result.preset = combination_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["items"] = shape.item_count;
    result.input_size["k"] = shape.choose_k;
    result.input_size["combinations"] = shape.combination_count;
    result.total_ms = elapsed_ms;
    result.correct = compare_combination_aggregates(aggregate, reference);
    result.device = cpu_device_name();
    result.notes = "CPU exhaustive k-combination evaluation";
    fill_combination_metadata(result, aggregate, reference);
    return {result};
}

} // namespace algobench::combinations
