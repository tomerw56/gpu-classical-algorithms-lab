#include "combinations/combination_finder.hpp"

#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <cuda_runtime.h>

#include <vector>

namespace algobench::combinations
{
namespace
{
struct DeviceAggregate
{
    unsigned long long valid_count;
    unsigned long long invalid_count;
    unsigned long long budget_violations;
    unsigned long long risk_violations;
    unsigned long long coverage_violations;
    unsigned long long spread_violations;
    unsigned long long checksum;
    int best_score;
};

__device__ unsigned long long checksum_value_device(long long rank, CombinationEvaluation eval)
{
    const long long mask_part = static_cast<long long>(eval.violation_mask) * 1'000'003LL;
    const long long score_part = static_cast<long long>(eval.score) * 97LL;
    return static_cast<unsigned long long>(((rank + 1) * 13LL) + mask_part + score_part);
}

__global__ void evaluate_combinations_kernel(const Item* items,
                                             CombinationShape shape,
                                             CombinationRules rules,
                                             DeviceAggregate* aggregate)
{
    const std::int64_t rank = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (rank >= shape.combination_count)
    {
        return;
    }

    std::int32_t indices[4] = {0, 0, 0, 0};
    unrank_combination_lex(shape.item_count, shape.choose_k, rank, indices);
    const CombinationEvaluation eval = evaluate_combination_indices(items, indices, shape.choose_k, rules);

    if (eval.violation_mask == 0u)
    {
        atomicAdd(&aggregate->valid_count, 1ull);
        atomicMax(&aggregate->best_score, eval.score);
    }
    else
    {
        atomicAdd(&aggregate->invalid_count, 1ull);
    }

    if ((eval.violation_mask & kCombinationViolationBudget) != 0u)
    {
        atomicAdd(&aggregate->budget_violations, 1ull);
    }
    if ((eval.violation_mask & kCombinationViolationRisk) != 0u)
    {
        atomicAdd(&aggregate->risk_violations, 1ull);
    }
    if ((eval.violation_mask & kCombinationViolationCoverage) != 0u)
    {
        atomicAdd(&aggregate->coverage_violations, 1ull);
    }
    if ((eval.violation_mask & kCombinationViolationSpread) != 0u)
    {
        atomicAdd(&aggregate->spread_violations, 1ull);
    }

    atomicAdd(&aggregate->checksum, checksum_value_device(rank, eval));
}

CombinationAggregate to_host_aggregate(const DeviceAggregate& device)
{
    CombinationAggregate aggregate;
    aggregate.valid_count = static_cast<std::int64_t>(device.valid_count);
    aggregate.invalid_count = static_cast<std::int64_t>(device.invalid_count);
    aggregate.budget_violations = static_cast<std::int64_t>(device.budget_violations);
    aggregate.risk_violations = static_cast<std::int64_t>(device.risk_violations);
    aggregate.coverage_violations = static_cast<std::int64_t>(device.coverage_violations);
    aggregate.spread_violations = static_cast<std::int64_t>(device.spread_violations);
    aggregate.checksum = static_cast<std::int64_t>(device.checksum);
    aggregate.best_score = device.best_score == -2147483647 ? 0 : device.best_score;
    aggregate.best_rank = -1; // The aggregate comparison intentionally validates best score, not tie rank.
    return aggregate;
}

CombinationAggregate run_gpu_once(const CombinationProblem& problem,
                                  double* h2d_ms,
                                  double* kernel_ms,
                                  double* d2h_ms)
{
    Item* d_items = nullptr;
    DeviceAggregate* d_aggregate = nullptr;

    cudaEvent_t start{};
    cudaEvent_t after_h2d{};
    cudaEvent_t after_kernel{};
    cudaEvent_t after_d2h{};
    CUDA_CHECK(cudaEventCreate(&start));
    CUDA_CHECK(cudaEventCreate(&after_h2d));
    CUDA_CHECK(cudaEventCreate(&after_kernel));
    CUDA_CHECK(cudaEventCreate(&after_d2h));

    const std::size_t item_bytes = problem.items.size() * sizeof(Item);
    CUDA_CHECK(cudaMalloc(&d_items, item_bytes));
    CUDA_CHECK(cudaMalloc(&d_aggregate, sizeof(DeviceAggregate)));

    DeviceAggregate initial{};
    initial.best_score = -2147483647;
    DeviceAggregate host_device_result{};

    CUDA_CHECK(cudaEventRecord(start));
    CUDA_CHECK(cudaMemcpy(d_items, problem.items.data(), item_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_aggregate, &initial, sizeof(DeviceAggregate), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(after_h2d));

    const int threads = 256;
    const int blocks = static_cast<int>((problem.shape.combination_count + threads - 1) / threads);
    evaluate_combinations_kernel<<<blocks, threads>>>(d_items, problem.shape, problem.rules, d_aggregate);
    CUDA_CHECK(cudaGetLastError());
    CUDA_CHECK(cudaEventRecord(after_kernel));

    CUDA_CHECK(cudaMemcpy(&host_device_result, d_aggregate, sizeof(DeviceAggregate), cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(after_d2h));
    CUDA_CHECK(cudaEventSynchronize(after_d2h));

    float h2d = 0.0f;
    float kernel = 0.0f;
    float d2h = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&h2d, start, after_h2d));
    CUDA_CHECK(cudaEventElapsedTime(&kernel, after_h2d, after_kernel));
    CUDA_CHECK(cudaEventElapsedTime(&d2h, after_kernel, after_d2h));

    *h2d_ms += h2d;
    *kernel_ms += kernel;
    *d2h_ms += d2h;

    CUDA_CHECK(cudaFree(d_items));
    CUDA_CHECK(cudaFree(d_aggregate));
    CUDA_CHECK(cudaEventDestroy(start));
    CUDA_CHECK(cudaEventDestroy(after_h2d));
    CUDA_CHECK(cudaEventDestroy(after_kernel));
    CUDA_CHECK(cudaEventDestroy(after_d2h));

    return to_host_aggregate(host_device_result);
}
} // namespace

std::vector<BenchmarkResult> run_combination_finder_gpu(const BenchmarkConfig& config)
{
    const auto shape = combination_shape_for_config(config);
    const auto problem = make_combination_problem(shape);
    const auto reference = evaluate_combinations_cpu_reference(problem);

    double warmup_h2d = 0.0;
    double warmup_kernel = 0.0;
    double warmup_d2h = 0.0;
    for (int i = 0; i < config.warmup; ++i)
    {
        (void)run_gpu_once(problem, &warmup_h2d, &warmup_kernel, &warmup_d2h);
    }

    CombinationAggregate aggregate;
    double h2d_ms = 0.0;
    double kernel_ms = 0.0;
    double d2h_ms = 0.0;
    for (int r = 0; r < config.repeat; ++r)
    {
        aggregate = run_gpu_once(problem, &h2d_ms, &kernel_ms, &d2h_ms);
    }

    BenchmarkResult result;
    result.benchmark = "combination_finder";
    result.variant = "gpu";
    result.preset = combination_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["items"] = shape.item_count;
    result.input_size["k"] = shape.choose_k;
    result.input_size["combinations"] = shape.combination_count;
    result.h2d_ms = h2d_ms;
    result.kernel_ms = kernel_ms;
    result.d2h_ms = d2h_ms;
    result.total_ms = h2d_ms + kernel_ms + d2h_ms;
    result.correct = compare_combination_aggregates(aggregate, reference);
    result.device = cuda_device_name();
    result.notes = "CUDA one-thread-per-combination aggregate evaluation";
    fill_combination_metadata(result, aggregate, reference);
    return {result};
}

} // namespace algobench::combinations
