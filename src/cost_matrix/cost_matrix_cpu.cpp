#include "cost_matrix/cost_matrix.hpp"

#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace algobench::cost_matrix
{
namespace
{

// Materialize the dense row-major matrix on the CPU.
//
// Logical shape:
//     costs[task_index][resource_index]
//
// Flat storage used by both CPU and GPU:
//     flat_index = task_index * resource_count + resource_index
//
// Example with four resources:
//     (task=2, resource=3) -> 2 * 4 + 3 -> flat index 11
void evaluate_matrix(std::vector<float>& costs,
                     std::vector<std::uint8_t>& feasible,
                     const std::vector<Task>& tasks,
                     const std::vector<Resource>& resources)
{
    const std::int64_t task_count = static_cast<std::int64_t>(tasks.size());
    const std::int64_t resource_count = static_cast<std::int64_t>(resources.size());

    for (std::int64_t task_index = 0; task_index < task_count; ++task_index)
    {
        for (std::int64_t resource_index = 0; resource_index < resource_count; ++resource_index)
        {
            const std::int64_t flat_index = task_index * resource_count + resource_index;
            const auto evaluation = evaluate_pair(tasks[static_cast<std::size_t>(task_index)],
                                                  resources[static_cast<std::size_t>(resource_index)]);
            costs[static_cast<std::size_t>(flat_index)] = evaluation.cost;
            feasible[static_cast<std::size_t>(flat_index)] = evaluation.feasible;
        }
    }
}

void fill_metadata(BenchmarkResult& result, const CostMatrixValidation& validation)
{
    result.metadata["checksum"] = std::to_string(validation.checksum);
    result.metadata["reference_checksum"] = std::to_string(validation.reference_checksum);
    result.metadata["feasible_count"] = std::to_string(validation.feasible_count);
    result.metadata["reference_feasible_count"] = std::to_string(validation.reference_feasible_count);
    result.metadata["feasibility_mismatches"] = std::to_string(validation.feasibility_mismatches);
    result.metadata["max_abs_error"] = std::to_string(validation.max_abs_error);
    result.metadata["max_rel_error"] = std::to_string(validation.max_rel_error);
    result.metadata["validation"] = "element-wise cost and feasibility comparison over final matrix";
    result.metadata["cost_model"] = "skill compatibility, dispatch radius, lateness rejection, urgency, load tiers, zone proxy";
}

} // namespace

std::vector<BenchmarkResult> run_cost_matrix_cpu(const BenchmarkConfig& config)
{
    const auto shape = shape_for_preset(config.preset);
    const auto tasks = make_tasks(shape.task_count);
    const auto resources = make_resources(shape.resource_count);

    std::vector<float> costs(static_cast<std::size_t>(shape.cell_count()), kInfeasibleCost);
    std::vector<std::uint8_t> feasible(static_cast<std::size_t>(shape.cell_count()), 0);

    for (int i = 0; i < config.warmup; ++i)
    {
        evaluate_matrix(costs, feasible, tasks, resources);
    }

    CpuTimer timer;
    timer.start();
    for (int r = 0; r < config.repeat; ++r)
    {
        evaluate_matrix(costs, feasible, tasks, resources);
    }
    const double elapsed_ms = timer.stop_ms();

    const auto validation = validate_cost_matrix(costs, feasible, tasks, resources);

    BenchmarkResult result;
    result.benchmark = "cost_matrix";
    result.variant = "cpu";
    result.preset = config.preset;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["cells"] = shape.cell_count();
    result.total_ms = elapsed_ms;
    result.correct = validation.correct;
    result.device = cpu_device_name();
    result.notes = "branch-heavy task/resource cost matrix using nested CPU loops";
    fill_metadata(result, validation);

    return {result};
}

} // namespace algobench::cost_matrix
