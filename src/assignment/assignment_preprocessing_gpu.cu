#include "assignment/assignment_preprocessing.hpp"
#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <cuda_runtime.h>

namespace algobench::assignment
{
namespace
{
__device__ void consider_topk_device(TopKEntry* local, int top_k, int resource_index, float cost)
{
    int worst = 0;
    for (int i = 1; i < top_k; ++i)
    {
        if (local[i].cost > local[worst].cost)
        {
            worst = i;
        }
    }
    if (cost < local[worst].cost || (cost == local[worst].cost && resource_index < local[worst].resource_index))
    {
        local[worst].resource_index = resource_index;
        local[worst].cost = cost;
    }
}

__device__ void sort_topk_device(TopKEntry* local, int top_k)
{
    for (int i = 0; i < top_k; ++i)
    {
        for (int j = i + 1; j < top_k; ++j)
        {
            if (local[j].cost < local[i].cost ||
                (local[j].cost == local[i].cost && local[j].resource_index < local[i].resource_index))
            {
                const TopKEntry tmp = local[i];
                local[i] = local[j];
                local[j] = tmp;
            }
        }
    }
}

__global__ void assignment_topk_kernel(const AssignmentTask* tasks,
                                       const AssignmentResource* resources,
                                       int task_count,
                                       int resource_count,
                                       int top_k,
                                       TopKEntry* output)
{
    const int task_index = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
    if (task_index >= task_count)
    {
        return;
    }

    TopKEntry local[16];
    for (int k = 0; k < top_k; ++k)
    {
        local[k].resource_index = -1;
        local[k].cost = kAssignmentInfeasibleCost;
    }

    const auto task = tasks[task_index];
    for (int resource_index = 0; resource_index < resource_count; ++resource_index)
    {
        const auto eval = evaluate_assignment_pair(task, resources[resource_index]);
        if (eval.feasible)
        {
            consider_topk_device(local, top_k, resource_index, eval.cost);
        }
    }

    sort_topk_device(local, top_k);
    for (int k = 0; k < top_k; ++k)
    {
        output[task_index * top_k + k] = local[k];
    }
}

} // namespace

std::vector<BenchmarkResult> run_assignment_preprocessing_gpu(const BenchmarkConfig& config)
{
    const auto shape = assignment_shape_for_config(config);
    const auto label = assignment_label_for_config(config);
    const auto problem = make_assignment_problem(shape);

    if (shape.top_k > 16)
    {
        BenchmarkResult result{};
        result.benchmark = "assignment_preprocessing";
        result.variant = "gpu";
        result.preset = label;
        result.repeat = config.repeat;
        result.warmup = config.warmup;
        result.correct = false;
        result.device = cuda_device_name();
        result.notes = "GPU demo supports top_k <= 16.";
        return {result};
    }

    const auto task_bytes = problem.tasks.size() * sizeof(AssignmentTask);
    const auto resource_bytes = problem.resources.size() * sizeof(AssignmentResource);
    const auto output_count = static_cast<std::size_t>(shape.task_count * shape.top_k);
    const auto output_bytes = output_count * sizeof(TopKEntry);

    AssignmentTask* d_tasks = nullptr;
    AssignmentResource* d_resources = nullptr;
    TopKEntry* d_output = nullptr;

    cudaEvent_t start{};
    cudaEvent_t after_h2d{};
    cudaEvent_t after_kernel{};
    cudaEvent_t after_d2h{};
    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&after_h2d));
    CUDA_CHECK(cudaEventCreate(&after_kernel));
    CUDA_CHECK(cudaEventCreate(&after_d2h));

    std::vector<TopKEntry> output(output_count);

    CUDA_CHECK(cudaEventRecord(start));
    CUDA_CHECK(cudaMalloc(&d_tasks, task_bytes));
    CUDA_CHECK(cudaMalloc(&d_resources, resource_bytes));
    CUDA_CHECK(cudaMalloc(&d_output, output_bytes));
    CUDA_CHECK(cudaMemcpy(d_tasks, problem.tasks.data(), task_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_resources, problem.resources.data(), resource_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(after_h2d));

    const int threads = 128;
    const int blocks = (shape.task_count + threads - 1) / threads;
    for (int w = 0; w < config.warmup; ++w)
    {
        assignment_topk_kernel<<<blocks, threads>>>(d_tasks, d_resources, shape.task_count, shape.resource_count, shape.top_k, d_output);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    cudaEvent_t kernel_start{};
    CUDA_CHECK(cudaEventCreate(&kernel_start));
    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        assignment_topk_kernel<<<blocks, threads>>>(d_tasks, d_resources, shape.task_count, shape.resource_count, shape.top_k, d_output);
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
    const float total_ms = h2d_ms + kernel_ms + d2h_ms;

    CUDA_CHECK(cudaFree(d_tasks));
    CUDA_CHECK(cudaFree(d_resources));
    CUDA_CHECK(cudaFree(d_output));
    CUDA_CHECK(cudaEventDestroy(start));
    CUDA_CHECK(cudaEventDestroy(after_h2d));
    CUDA_CHECK(cudaEventDestroy(after_kernel));
    CUDA_CHECK(cudaEventDestroy(after_d2h));
    CUDA_CHECK(cudaEventDestroy(kernel_start));

    const auto reference = compute_assignment_topk_cpu_reference(problem);
    const auto aggregate = aggregate_assignment_solution(problem, output);
    const auto validation = validate_assignment_topk(problem, output, reference);

    BenchmarkResult result{};
    result.benchmark = "assignment_preprocessing";
    result.variant = "gpu";
    result.preset = label;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["pairs"] = static_cast<std::int64_t>(shape.task_count) * shape.resource_count;
    result.input_size["top_k"] = shape.top_k;
    result.total_ms = static_cast<double>(total_ms);
    result.h2d_ms = static_cast<double>(h2d_ms);
    result.kernel_ms = static_cast<double>(kernel_ms);
    result.d2h_ms = static_cast<double>(d2h_ms);
    result.correct = validation.correct;
    result.device = cuda_device_name();
    result.notes = "GPU one-thread-per-task top-K scan over all resources.";
    fill_assignment_metadata(result, aggregate, validation);
    return {result};
}

} // namespace algobench::assignment
