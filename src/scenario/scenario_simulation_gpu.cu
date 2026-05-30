#include "scenario/scenario_simulation.hpp"
#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <cuda_runtime.h>

namespace algobench::scenario
{
namespace
{
__global__ void evaluate_scenarios_kernel(const ScenarioTask* tasks,
                                          const ScenarioResource* resources,
                                          const int* assignment,
                                          const ScenarioCase* scenarios,
                                          std::int32_t task_count,
                                          std::int64_t scenario_count,
                                          ScenarioEvaluation* output)
{
    const std::int64_t idx = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (idx >= scenario_count)
    {
        return;
    }
    output[idx] = evaluate_scenario_case(tasks, resources, assignment, task_count, scenarios[idx]);
}
} // namespace

std::vector<BenchmarkResult> run_scenario_simulation_gpu(const BenchmarkConfig& config)
{
    const auto shape = scenario_shape_for_config(config);
    const auto label = scenario_label_for_config(config);
    const auto problem = make_scenario_problem(shape);

    const auto task_bytes = problem.tasks.size() * sizeof(ScenarioTask);
    const auto resource_bytes = problem.resources.size() * sizeof(ScenarioResource);
    const auto assignment_bytes = problem.assignment.size() * sizeof(std::int32_t);
    const auto scenario_bytes = problem.scenarios.size() * sizeof(ScenarioCase);
    const auto output_bytes = problem.scenarios.size() * sizeof(ScenarioEvaluation);

    ScenarioTask* d_tasks = nullptr;
    ScenarioResource* d_resources = nullptr;
    std::int32_t* d_assignment = nullptr;
    ScenarioCase* d_scenarios = nullptr;
    ScenarioEvaluation* d_output = nullptr;

    cudaEvent_t start{};
    cudaEvent_t after_h2d{};
    cudaEvent_t kernel_start{};
    cudaEvent_t after_kernel{};
    cudaEvent_t after_d2h{};
    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&after_h2d));
    CUDA_CHECK(cudaEventCreate(&kernel_start));
    CUDA_CHECK(cudaEventCreate(&after_kernel));
    CUDA_CHECK(cudaEventCreate(&after_d2h));

    std::vector<ScenarioEvaluation> output(problem.scenarios.size());

    CUDA_CHECK(cudaEventRecord(start));
    CUDA_CHECK(cudaMalloc(&d_tasks, task_bytes));
    CUDA_CHECK(cudaMalloc(&d_resources, resource_bytes));
    CUDA_CHECK(cudaMalloc(&d_assignment, assignment_bytes));
    CUDA_CHECK(cudaMalloc(&d_scenarios, scenario_bytes));
    CUDA_CHECK(cudaMalloc(&d_output, output_bytes));
    CUDA_CHECK(cudaMemcpy(d_tasks, problem.tasks.data(), task_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_resources, problem.resources.data(), resource_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_assignment, problem.assignment.data(), assignment_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_scenarios, problem.scenarios.data(), scenario_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(after_h2d));

    const int threads = 256;
    const int blocks = static_cast<int>((shape.scenario_count + threads - 1) / threads);
    for (int w = 0; w < config.warmup; ++w)
    {
        evaluate_scenarios_kernel<<<blocks, threads>>>(d_tasks, d_resources, d_assignment, d_scenarios, shape.task_count, shape.scenario_count, d_output);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        evaluate_scenarios_kernel<<<blocks, threads>>>(d_tasks, d_resources, d_assignment, d_scenarios, shape.task_count, shape.scenario_count, d_output);
        CUDA_CHECK(cudaGetLastError());
    }
    CUDA_CHECK(cudaEventRecord(after_kernel));
    CUDA_CHECK(cudaEventSynchronize(after_kernel));

    CUDA_CHECK(cudaMemcpy(output.data(), d_output, output_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(after_d2h));
    CUDA_CHECK(cudaEventSynchronize(after_d2h));

    float h2d_ms = 0.0f;
    float kernel_ms = 0.0f;
    float d2h_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&h2d_ms, start, after_h2d));
    CUDA_CHECK(cudaEventElapsedTime(&kernel_ms, kernel_start, after_kernel));
    CUDA_CHECK(cudaEventElapsedTime(&d2h_ms, after_kernel, after_d2h));

    CUDA_CHECK(cudaFree(d_tasks));
    CUDA_CHECK(cudaFree(d_resources));
    CUDA_CHECK(cudaFree(d_assignment));
    CUDA_CHECK(cudaFree(d_scenarios));
    CUDA_CHECK(cudaFree(d_output));
    CUDA_CHECK(cudaEventDestroy(start));
    CUDA_CHECK(cudaEventDestroy(after_h2d));
    CUDA_CHECK(cudaEventDestroy(kernel_start));
    CUDA_CHECK(cudaEventDestroy(after_kernel));
    CUDA_CHECK(cudaEventDestroy(after_d2h));

    const auto reference = evaluate_scenarios_cpu_reference(problem);
    const auto aggregate = aggregate_scenario_evaluations(output);
    const auto validation = validate_scenario_evaluations(output, reference);

    BenchmarkResult result{};
    result.benchmark = "scenario_simulation";
    result.variant = "gpu";
    result.preset = label;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["scenarios"] = shape.scenario_count;
    result.total_ms = static_cast<double>(h2d_ms + kernel_ms + d2h_ms);
    result.h2d_ms = h2d_ms;
    result.kernel_ms = kernel_ms;
    result.d2h_ms = d2h_ms;
    result.correct = validation.correct;
    result.device = cuda_device_name();
    result.notes = "GPU one-thread-per-scenario robust-plan evaluation.";
    fill_scenario_metadata(result, aggregate, validation);
    return {result};
}

} // namespace algobench::scenario
