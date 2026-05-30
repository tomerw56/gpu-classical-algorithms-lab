#include "scenario/scenario_simulation.hpp"
#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

namespace algobench::scenario
{

std::vector<BenchmarkResult> run_scenario_simulation_cpu(const BenchmarkConfig& config)
{
    const auto shape = scenario_shape_for_config(config);
    const auto label = scenario_label_for_config(config);
    const auto problem = make_scenario_problem(shape);

    std::vector<ScenarioEvaluation> evaluations;
    for (int w = 0; w < config.warmup; ++w)
    {
        evaluations = evaluate_scenarios_cpu_reference(problem);
    }

    CpuTimer timer;
    timer.start();
    for (int r = 0; r < config.repeat; ++r)
    {
        evaluations = evaluate_scenarios_cpu_reference(problem);
    }
    const double total_ms = timer.stop_ms();

    const auto reference = evaluate_scenarios_cpu_reference(problem);
    const auto aggregate = aggregate_scenario_evaluations(evaluations);
    const auto validation = validate_scenario_evaluations(evaluations, reference);

    BenchmarkResult result{};
    result.benchmark = "scenario_simulation";
    result.variant = "cpu";
    result.preset = label;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["scenarios"] = shape.scenario_count;
    result.total_ms = total_ms;
    result.correct = validation.correct;
    result.device = cpu_device_name();
    result.notes = "CPU robust-plan scenario simulation over generated uncertainty cases.";
    fill_scenario_metadata(result, aggregate, validation);
    return {result};
}

} // namespace algobench::scenario
