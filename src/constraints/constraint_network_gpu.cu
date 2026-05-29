#include "constraints/constraint_network.hpp"

#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace algobench::constraints
{
namespace
{

__global__ void evaluate_candidates_kernel(const Task* tasks,
                                           const Resource* resources,
                                           const CandidateAssignment* candidates,
                                           CandidateEvaluation* output,
                                           std::int64_t candidate_count)
{
    const std::int64_t idx = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (idx >= candidate_count)
    {
        return;
    }

    const auto candidate = candidates[idx];
    output[idx] = evaluate_candidate(tasks[candidate.task_index], resources[candidate.resource_index], candidate);
}

} // namespace

std::vector<BenchmarkResult> run_constraint_network_gpu(const BenchmarkConfig& config)
{
    const auto shape = constraint_shape_for_config(config);
    const auto problem = make_constraint_problem(shape);

    BenchmarkResult result;
    result.benchmark = "constraint_network";
    result.variant = "gpu";
    result.preset = constraint_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tasks"] = shape.task_count;
    result.input_size["resources"] = shape.resource_count;
    result.input_size["candidates"] = shape.candidate_count;
    result.device = cuda_device_name();
    result.notes = "one CUDA thread per candidate assignment";
    result.metadata["cuda_status"] = cuda_runtime_status();

    if (!cuda_runtime_available())
    {
        result.correct = false;
        result.metadata["status"] = "CUDA runtime unavailable";
        return {result};
    }

    Task* d_tasks = nullptr;
    Resource* d_resources = nullptr;
    CandidateAssignment* d_candidates = nullptr;
    CandidateEvaluation* d_output = nullptr;

    const std::size_t task_bytes = problem.tasks.size() * sizeof(Task);
    const std::size_t resource_bytes = problem.resources.size() * sizeof(Resource);
    const std::size_t candidate_bytes = problem.candidates.size() * sizeof(CandidateAssignment);
    const std::size_t output_bytes = problem.candidates.size() * sizeof(CandidateEvaluation);

    CUDA_CHECK(cudaMalloc(&d_tasks, task_bytes));
    CUDA_CHECK(cudaMalloc(&d_resources, resource_bytes));
    CUDA_CHECK(cudaMalloc(&d_candidates, candidate_bytes));
    CUDA_CHECK(cudaMalloc(&d_output, output_bytes));

    cudaEvent_t h2d_start{}, h2d_stop{};
    CUDA_CHECK(cudaEventCreate(&h2d_start));
    CUDA_CHECK(cudaEventCreate(&h2d_stop));
    CUDA_CHECK(cudaEventRecord(h2d_start));
    CUDA_CHECK(cudaMemcpy(d_tasks, problem.tasks.data(), task_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_resources, problem.resources.data(), resource_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_candidates, problem.candidates.data(), candidate_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(h2d_stop));
    CUDA_CHECK(cudaEventSynchronize(h2d_stop));
    float h2d_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&h2d_ms, h2d_start, h2d_stop));

    const int threads = 256;
    const int blocks = static_cast<int>((shape.candidate_count + threads - 1) / threads);

    for (int i = 0; i < config.warmup; ++i)
    {
        evaluate_candidates_kernel<<<blocks, threads>>>(d_tasks, d_resources, d_candidates, d_output, shape.candidate_count);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    cudaEvent_t kernel_start{}, kernel_stop{};
    CUDA_CHECK(cudaEventCreate(&kernel_start));
    CUDA_CHECK(cudaEventCreate(&kernel_stop));
    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        evaluate_candidates_kernel<<<blocks, threads>>>(d_tasks, d_resources, d_candidates, d_output, shape.candidate_count);
    }
    CUDA_CHECK(cudaEventRecord(kernel_stop));
    CUDA_CHECK(cudaEventSynchronize(kernel_stop));
    CUDA_CHECK(cudaGetLastError());
    float kernel_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&kernel_ms, kernel_start, kernel_stop));

    std::vector<CandidateEvaluation> host_output(problem.candidates.size());
    cudaEvent_t d2h_start{}, d2h_stop{};
    CUDA_CHECK(cudaEventCreate(&d2h_start));
    CUDA_CHECK(cudaEventCreate(&d2h_stop));
    CUDA_CHECK(cudaEventRecord(d2h_start));
    CUDA_CHECK(cudaMemcpy(host_output.data(), d_output, output_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(d2h_stop));
    CUDA_CHECK(cudaEventSynchronize(d2h_stop));
    float d2h_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&d2h_ms, d2h_start, d2h_stop));

    const auto validation = validate_constraint_evaluations(problem, host_output);

    result.h2d_ms = static_cast<double>(h2d_ms);
    result.kernel_ms = static_cast<double>(kernel_ms);
    result.d2h_ms = static_cast<double>(d2h_ms);
    result.total_ms = result.h2d_ms + result.kernel_ms + result.d2h_ms;
    result.correct = validation.correct;
    fill_constraint_metadata(result, validation);

    CUDA_CHECK(cudaEventDestroy(h2d_start));
    CUDA_CHECK(cudaEventDestroy(h2d_stop));
    CUDA_CHECK(cudaEventDestroy(kernel_start));
    CUDA_CHECK(cudaEventDestroy(kernel_stop));
    CUDA_CHECK(cudaEventDestroy(d2h_start));
    CUDA_CHECK(cudaEventDestroy(d2h_stop));
    CUDA_CHECK(cudaFree(d_tasks));
    CUDA_CHECK(cudaFree(d_resources));
    CUDA_CHECK(cudaFree(d_candidates));
    CUDA_CHECK(cudaFree(d_output));

    return {result};
}

} // namespace algobench::constraints
