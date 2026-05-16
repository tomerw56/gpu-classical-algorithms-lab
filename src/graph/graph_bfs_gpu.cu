#include "graph/graph_bfs.hpp"

#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace algobench::graph_bfs
{
namespace
{

__global__ void bfs_frontier_kernel(const std::int64_t* row_offsets,
                                    const std::int32_t* column_indices,
                                    const std::int32_t* current_frontier,
                                    std::int32_t current_frontier_size,
                                    std::int32_t* next_frontier,
                                    std::int32_t* next_frontier_size,
                                    std::int32_t* distances,
                                    std::int32_t current_distance)
{
    const std::int32_t tid = static_cast<std::int32_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (tid >= current_frontier_size)
    {
        return;
    }

    const std::int32_t node = current_frontier[tid];
    const std::int64_t begin = row_offsets[node];
    const std::int64_t end = row_offsets[node + 1];

    for (std::int64_t edge = begin; edge < end; ++edge)
    {
        const std::int32_t neighbor = column_indices[edge];
        const std::int32_t previous = atomicCAS(&distances[neighbor], kUnreachedDistance, current_distance + 1);
        if (previous == kUnreachedDistance)
        {
            const std::int32_t position = atomicAdd(next_frontier_size, 1);
            next_frontier[position] = neighbor;
        }
    }
}

void fill_metadata(BenchmarkResult& result,
                   const GraphBfsShape& shape,
                   const graph::CsrGraph& graph,
                   const graph::GraphStats& stats,
                   const BfsValidationSummary& validation,
                   std::int32_t iterations)
{
    result.metadata["graph_kind"] = shape.graph_kind;
    result.metadata["shape"] = describe_shape(shape);
    result.metadata["source"] = std::to_string(shape.source);
    result.metadata["edge_count"] = std::to_string(graph.edge_count());
    result.metadata["min_out_degree"] = std::to_string(stats.min_out_degree);
    result.metadata["max_out_degree"] = std::to_string(stats.max_out_degree);
    result.metadata["mean_out_degree"] = std::to_string(stats.mean_out_degree);
    result.metadata["zero_out_degree_count"] = std::to_string(stats.zero_out_degree_count);
    result.metadata["reached_count"] = std::to_string(validation.reached_count);
    result.metadata["max_distance"] = std::to_string(validation.max_distance);
    result.metadata["mismatch_count"] = std::to_string(validation.mismatch_count);
    result.metadata["checksum"] = std::to_string(validation.checksum);
    result.metadata["reference_checksum"] = std::to_string(validation.reference_checksum);
    result.metadata["frontier_iterations"] = std::to_string(iterations);
    result.metadata["cuda_status"] = cuda_runtime_status();
    result.metadata["validation"] = "element-wise BFS distance comparison against CPU queue reference";
    result.metadata["gpu_algorithm"] = "level-synchronous frontier BFS using atomicCAS for first discovery and atomicAdd for next frontier append";
    result.metadata["kernel_ms_semantics"] = "timed GPU traversal loop, not a pure single-kernel interval; it brackets repeated BFS levels, frontier resets, and per-level frontier-size control synchronization";
}

std::int32_t run_gpu_bfs_once(const graph::CsrGraph& graph,
                              std::int32_t source,
                              const std::int64_t* d_row_offsets,
                              const std::int32_t* d_column_indices,
                              std::int32_t* d_current_frontier,
                              std::int32_t* d_next_frontier,
                              std::int32_t* d_next_frontier_size,
                              std::int32_t* d_distances)
{
    const int threads = 256;
    std::int32_t current_frontier_size = 1;
    std::int32_t current_distance = 0;
    std::int32_t iterations = 0;

    CUDA_CHECK(cudaMemset(d_distances, 0xFF, static_cast<std::size_t>(graph.node_count) * sizeof(std::int32_t)));
    CUDA_CHECK(cudaMemcpy(d_distances + source, &current_distance, sizeof(std::int32_t), cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_current_frontier, &source, sizeof(std::int32_t), cudaMemcpyHostToDevice));

    while (current_frontier_size > 0)
    {
        CUDA_CHECK(cudaMemset(d_next_frontier_size, 0, sizeof(std::int32_t)));
        const int blocks = (current_frontier_size + threads - 1) / threads;

        bfs_frontier_kernel<<<blocks, threads>>>(d_row_offsets,
                                                d_column_indices,
                                                d_current_frontier,
                                                current_frontier_size,
                                                d_next_frontier,
                                                d_next_frontier_size,
                                                d_distances,
                                                current_distance);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaMemcpy(&current_frontier_size,
                              d_next_frontier_size,
                              sizeof(std::int32_t),
                              cudaMemcpyDeviceToHost));

        std::swap(d_current_frontier, d_next_frontier);
        ++current_distance;
        ++iterations;
    }

    return iterations;
}

} // namespace

std::vector<BenchmarkResult> run_graph_bfs_gpu(const BenchmarkConfig& config)
{
    BenchmarkResult result;
    result.benchmark = "graph_bfs";
    result.variant = "gpu";
    result.preset = result_preset_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.device = cuda_device_name();
    result.notes = "frontier-based BFS over CSR graph";
    result.metadata["cuda_status"] = cuda_runtime_status();

    const auto shape = shape_for_config(config);
    const auto graph = make_graph_for_shape(shape);
    const auto stats = graph::summarize_graph(graph);
    result.input_size["nodes"] = graph.node_count;
    result.input_size["edges"] = graph.edge_count();

    if (!cuda_runtime_available())
    {
        result.correct = false;
        result.metadata["status"] = "CUDA runtime unavailable";
        return {result};
    }

    std::int64_t* d_row_offsets = nullptr;
    std::int32_t* d_column_indices = nullptr;
    std::int32_t* d_current_frontier = nullptr;
    std::int32_t* d_next_frontier = nullptr;
    std::int32_t* d_next_frontier_size = nullptr;
    std::int32_t* d_distances = nullptr;

    const std::size_t row_bytes = graph.row_offsets.size() * sizeof(std::int64_t);
    const std::size_t col_bytes = graph.column_indices.size() * sizeof(std::int32_t);
    const std::size_t frontier_bytes = static_cast<std::size_t>(graph.node_count) * sizeof(std::int32_t);
    const std::size_t distance_bytes = static_cast<std::size_t>(graph.node_count) * sizeof(std::int32_t);

    CUDA_CHECK(cudaMalloc(&d_row_offsets, row_bytes));
    CUDA_CHECK(cudaMalloc(&d_column_indices, col_bytes));
    CUDA_CHECK(cudaMalloc(&d_current_frontier, frontier_bytes));
    CUDA_CHECK(cudaMalloc(&d_next_frontier, frontier_bytes));
    CUDA_CHECK(cudaMalloc(&d_next_frontier_size, sizeof(std::int32_t)));
    CUDA_CHECK(cudaMalloc(&d_distances, distance_bytes));

    cudaEvent_t h2d_start{}, h2d_stop{};
    CUDA_CHECK(cudaEventCreate(&h2d_start));
    CUDA_CHECK(cudaEventCreate(&h2d_stop));
    CUDA_CHECK(cudaEventRecord(h2d_start));
    CUDA_CHECK(cudaMemcpy(d_row_offsets, graph.row_offsets.data(), row_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_column_indices, graph.column_indices.data(), col_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(h2d_stop));
    CUDA_CHECK(cudaEventSynchronize(h2d_stop));

    float h2d_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&h2d_ms, h2d_start, h2d_stop));

    std::int32_t iterations = 0;
    for (int i = 0; i < config.warmup; ++i)
    {
        iterations = run_gpu_bfs_once(graph,
                                      shape.source,
                                      d_row_offsets,
                                      d_column_indices,
                                      d_current_frontier,
                                      d_next_frontier,
                                      d_next_frontier_size,
                                      d_distances);
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    cudaEvent_t kernel_start{}, kernel_stop{};
    CUDA_CHECK(cudaEventCreate(&kernel_start));
    CUDA_CHECK(cudaEventCreate(&kernel_stop));
    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        iterations = run_gpu_bfs_once(graph,
                                      shape.source,
                                      d_row_offsets,
                                      d_column_indices,
                                      d_current_frontier,
                                      d_next_frontier,
                                      d_next_frontier_size,
                                      d_distances);
    }
    CUDA_CHECK(cudaEventRecord(kernel_stop));
    CUDA_CHECK(cudaEventSynchronize(kernel_stop));
    CUDA_CHECK(cudaGetLastError());

    float kernel_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&kernel_ms, kernel_start, kernel_stop));

    std::vector<std::int32_t> host_distances(static_cast<std::size_t>(graph.node_count), kUnreachedDistance);
    cudaEvent_t d2h_start{}, d2h_stop{};
    CUDA_CHECK(cudaEventCreate(&d2h_start));
    CUDA_CHECK(cudaEventCreate(&d2h_stop));
    CUDA_CHECK(cudaEventRecord(d2h_start));
    CUDA_CHECK(cudaMemcpy(host_distances.data(), d_distances, distance_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(d2h_stop));
    CUDA_CHECK(cudaEventSynchronize(d2h_stop));

    float d2h_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&d2h_ms, d2h_start, d2h_stop));

    const auto validation = validate_bfs_distances(graph, shape.source, host_distances);

    result.h2d_ms = static_cast<double>(h2d_ms);
    result.kernel_ms = static_cast<double>(kernel_ms);
    result.d2h_ms = static_cast<double>(d2h_ms);
    result.total_ms = result.h2d_ms + result.kernel_ms + result.d2h_ms;
    result.correct = validation.correct;
    fill_metadata(result, shape, graph, stats, validation, iterations);

    CUDA_CHECK(cudaEventDestroy(h2d_start));
    CUDA_CHECK(cudaEventDestroy(h2d_stop));
    CUDA_CHECK(cudaEventDestroy(kernel_start));
    CUDA_CHECK(cudaEventDestroy(kernel_stop));
    CUDA_CHECK(cudaEventDestroy(d2h_start));
    CUDA_CHECK(cudaEventDestroy(d2h_stop));
    CUDA_CHECK(cudaFree(d_row_offsets));
    CUDA_CHECK(cudaFree(d_column_indices));
    CUDA_CHECK(cudaFree(d_current_frontier));
    CUDA_CHECK(cudaFree(d_next_frontier));
    CUDA_CHECK(cudaFree(d_next_frontier_size));
    CUDA_CHECK(cudaFree(d_distances));

    return {result};
}

} // namespace algobench::graph_bfs
