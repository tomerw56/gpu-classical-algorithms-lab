#include "cost_matrix/cost_matrix.hpp"

#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace algobench::cost_matrix
{
namespace
{

// CUDA materialization of the same dense row-major matrix used by the CPU path.
// Each thread owns one logical task/resource pair and one output cell.
__global__ void cost_matrix_kernel(const Task* tasks,
                                   std::int64_t task_count,
                                   const Resource* resources,
                                   std::int64_t resource_count,
                                   float* costs,
                                   std::uint8_t* feasible)
{
    const std::int64_t flat_index = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    const std::int64_t cell_count = task_count * resource_count;

    if (flat_index >= cell_count)
    {
        return;
    }

    // Inverse of:
    //     flat_index = task_index * resource_count + resource_index
    // This reconstructs the logical matrix coordinates owned by the thread.
    const std::int64_t task_index = flat_index / resource_count;
    const std::int64_t resource_index = flat_index - task_index * resource_count;

    const auto evaluation = evaluate_pair(tasks[task_index], resources[resource_index]);
    costs[flat_index] = evaluation.cost;
    feasible[flat_index] = evaluation.feasible;
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

std::vector<BenchmarkResult> run_cost_matrix_gpu(const BenchmarkConfig& config)
{
    BenchmarkResult result;
    result.benchmark = "cost_matrix";
    result.variant = "gpu";
    result.preset = config.preset;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.device = cuda_device_name();
    result.notes = "branch-heavy task/resource cost matrix with one CUDA thread per matrix cell";
    result.metadata["cuda_status"] = cuda_runtime_status();

    const auto shape = shape_for_preset(config.preset);
    const auto tasks = make_tasks(shape.task_count);
    const auto resources = make_resources(shape.resource_count);
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["cells"] = shape.cell_count();

    if (!cuda_runtime_available())
    {
        result.correct = false;
        result.metadata["status"] = "CUDA runtime unavailable";
        return {result};
    }

    Task* d_tasks = nullptr;
    Resource* d_resources = nullptr;
    float* d_costs = nullptr;
    std::uint8_t* d_feasible = nullptr;

    const std::size_t task_bytes = tasks.size() * sizeof(Task);
    const std::size_t resource_bytes = resources.size() * sizeof(Resource);
    const std::size_t cost_bytes = static_cast<std::size_t>(shape.cell_count()) * sizeof(float);
    const std::size_t feasible_bytes = static_cast<std::size_t>(shape.cell_count()) * sizeof(std::uint8_t);

    CUDA_CHECK(cudaMalloc(&d_tasks, task_bytes));
    CUDA_CHECK(cudaMalloc(&d_resources, resource_bytes));
    CUDA_CHECK(cudaMalloc(&d_costs, cost_bytes));
    CUDA_CHECK(cudaMalloc(&d_feasible, feasible_bytes));

    cudaEvent_t h2d_start{}, h2d_stop{};
    CUDA_CHECK(cudaEventCreate(&h2d_start));
    CUDA_CHECK(cudaEventCreate(&h2d_stop));
    CUDA_CHECK(cudaEventRecord(h2d_start));
    CUDA_CHECK(cudaMemcpy(d_tasks, tasks.data(), task_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_resources, resources.data(), resource_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(h2d_stop));
    CUDA_CHECK(cudaEventSynchronize(h2d_stop));

    float h2d_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&h2d_ms, h2d_start, h2d_stop));

    const int threads = 256;
    const int blocks = static_cast<int>((shape.cell_count() + threads - 1) / threads);

    for (int i = 0; i < config.warmup; ++i)
    {
        cost_matrix_kernel<<<blocks, threads>>>(d_tasks,
                                                shape.task_count,
                                                d_resources,
                                                shape.resource_count,
                                                d_costs,
                                                d_feasible);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    cudaEvent_t kernel_start{}, kernel_stop{};
    CUDA_CHECK(cudaEventCreate(&kernel_start));
    CUDA_CHECK(cudaEventCreate(&kernel_stop));
    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        cost_matrix_kernel<<<blocks, threads>>>(d_tasks,
                                                shape.task_count,
                                                d_resources,
                                                shape.resource_count,
                                                d_costs,
                                                d_feasible);
    }
    CUDA_CHECK(cudaEventRecord(kernel_stop));
    CUDA_CHECK(cudaEventSynchronize(kernel_stop));
    CUDA_CHECK(cudaGetLastError());

    float kernel_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&kernel_ms, kernel_start, kernel_stop));

    std::vector<float> host_costs(static_cast<std::size_t>(shape.cell_count()), kInfeasibleCost);
    std::vector<std::uint8_t> host_feasible(static_cast<std::size_t>(shape.cell_count()), 0);

    cudaEvent_t d2h_start{}, d2h_stop{};
    CUDA_CHECK(cudaEventCreate(&d2h_start));
    CUDA_CHECK(cudaEventCreate(&d2h_stop));
    CUDA_CHECK(cudaEventRecord(d2h_start));
    CUDA_CHECK(cudaMemcpy(host_costs.data(), d_costs, cost_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(host_feasible.data(), d_feasible, feasible_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(d2h_stop));
    CUDA_CHECK(cudaEventSynchronize(d2h_stop));

    float d2h_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&d2h_ms, d2h_start, d2h_stop));

    const auto validation = validate_cost_matrix(host_costs, host_feasible, tasks, resources);

    result.h2d_ms = static_cast<double>(h2d_ms);
    result.kernel_ms = static_cast<double>(kernel_ms);
    result.d2h_ms = static_cast<double>(d2h_ms);
    result.total_ms = result.h2d_ms + result.kernel_ms + result.d2h_ms;
    result.correct = validation.correct;
    fill_metadata(result, validation);

    CUDA_CHECK(cudaEventDestroy(h2d_start));
    CUDA_CHECK(cudaEventDestroy(h2d_stop));
    CUDA_CHECK(cudaEventDestroy(kernel_start));
    CUDA_CHECK(cudaEventDestroy(kernel_stop));
    CUDA_CHECK(cudaEventDestroy(d2h_start));
    CUDA_CHECK(cudaEventDestroy(d2h_stop));
    CUDA_CHECK(cudaFree(d_tasks));
    CUDA_CHECK(cudaFree(d_resources));
    CUDA_CHECK(cudaFree(d_costs));
    CUDA_CHECK(cudaFree(d_feasible));

    return {result};
}

} // namespace algobench::cost_matrix
