#include "local_search/local_search_moves.hpp"
#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <cuda_runtime.h>

namespace algobench::local_search
{
namespace
{
__global__ void evaluate_moves_kernel(const SearchTask* tasks,
                                      const SearchResource* resources,
                                      const int* current_assignment,
                                      const CandidateMove* moves,
                                      std::int64_t move_count,
                                      MoveEvaluation* output)
{
    const std::int64_t idx = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (idx >= move_count)
    {
        return;
    }

    const CandidateMove move = moves[idx];
    const SearchTask task = tasks[move.task_index];
    const int old_resource_index = current_assignment[move.task_index];
    const SearchResource old_resource = resources[old_resource_index];
    const SearchResource new_resource = resources[move.new_resource_index];
    output[idx] = evaluate_candidate_move(task, old_resource, new_resource, old_resource_index, move.new_resource_index);
}
} // namespace

std::vector<BenchmarkResult> run_local_search_moves_gpu(const BenchmarkConfig& config)
{
    const auto shape = local_search_shape_for_config(config);
    const auto label = local_search_label_for_config(config);
    const auto problem = make_local_search_problem(shape);

    const auto task_bytes = problem.tasks.size() * sizeof(SearchTask);
    const auto resource_bytes = problem.resources.size() * sizeof(SearchResource);
    const auto assignment_bytes = problem.current_assignment.size() * sizeof(std::int32_t);
    const auto move_bytes = problem.moves.size() * sizeof(CandidateMove);
    const auto output_bytes = problem.moves.size() * sizeof(MoveEvaluation);

    SearchTask* d_tasks = nullptr;
    SearchResource* d_resources = nullptr;
    std::int32_t* d_assignment = nullptr;
    CandidateMove* d_moves = nullptr;
    MoveEvaluation* d_output = nullptr;

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

    std::vector<MoveEvaluation> output(problem.moves.size());

    CUDA_CHECK(cudaEventRecord(start));
    CUDA_CHECK(cudaMalloc(&d_tasks, task_bytes));
    CUDA_CHECK(cudaMalloc(&d_resources, resource_bytes));
    CUDA_CHECK(cudaMalloc(&d_assignment, assignment_bytes));
    CUDA_CHECK(cudaMalloc(&d_moves, move_bytes));
    CUDA_CHECK(cudaMalloc(&d_output, output_bytes));
    CUDA_CHECK(cudaMemcpy(d_tasks, problem.tasks.data(), task_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_resources, problem.resources.data(), resource_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_assignment, problem.current_assignment.data(), assignment_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_moves, problem.moves.data(), move_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(after_h2d));

    const int threads = 256;
    const int blocks = static_cast<int>((shape.move_count + threads - 1) / threads);
    for (int w = 0; w < config.warmup; ++w)
    {
        evaluate_moves_kernel<<<blocks, threads>>>(d_tasks, d_resources, d_assignment, d_moves, shape.move_count, d_output);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        evaluate_moves_kernel<<<blocks, threads>>>(d_tasks, d_resources, d_assignment, d_moves, shape.move_count, d_output);
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
    CUDA_CHECK(cudaFree(d_moves));
    CUDA_CHECK(cudaFree(d_output));
    CUDA_CHECK(cudaEventDestroy(start));
    CUDA_CHECK(cudaEventDestroy(after_h2d));
    CUDA_CHECK(cudaEventDestroy(kernel_start));
    CUDA_CHECK(cudaEventDestroy(after_kernel));
    CUDA_CHECK(cudaEventDestroy(after_d2h));

    const auto reference = evaluate_moves_cpu_reference(problem);
    const auto aggregate = aggregate_move_evaluations(output);
    const auto validation = validate_move_evaluations(output, reference);

    BenchmarkResult result{};
    result.benchmark = "local_search_moves";
    result.variant = "gpu";
    result.preset = label;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["moves"] = shape.move_count;
    result.total_ms = static_cast<double>(h2d_ms + kernel_ms + d2h_ms);
    result.h2d_ms = h2d_ms;
    result.kernel_ms = kernel_ms;
    result.d2h_ms = d2h_ms;
    result.correct = validation.correct;
    result.device = cuda_device_name();
    result.notes = "GPU one-thread-per-candidate local-search move evaluation.";
    fill_local_search_metadata(result, aggregate, validation);
    return {result};
}

} // namespace algobench::local_search
