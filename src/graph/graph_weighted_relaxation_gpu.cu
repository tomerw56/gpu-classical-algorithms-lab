#include "graph/graph_weighted_relaxation.hpp"

#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace algobench::graph_weighted
{
namespace
{

enum class GpuWeightedAlgorithm
{
    GlobalEdgeScan,
    FrontierQueue,
};

struct GpuRelaxationRunStats
{
    std::int32_t iterations = 0;
    bool converged = false;
    bool frontier_overflow = false;
    std::int64_t edge_iterations = 0;
    std::int64_t active_frontier_node_visits = 0;
    std::int32_t max_frontier_size = 0;
    std::int32_t frontier_capacity = 0;
};

__global__ void fill_distances_kernel(std::int32_t* distances, std::int32_t node_count, std::int32_t value)
{
    const auto index = static_cast<std::int32_t>(blockIdx.x * blockDim.x + threadIdx.x);
    if (index < node_count)
    {
        distances[index] = value;
    }
}

__global__ void relax_edges_kernel(const std::int32_t* edge_sources,
                                   const std::int32_t* column_indices,
                                   const std::int32_t* weights,
                                   std::int64_t edge_count,
                                   std::int32_t* distances,
                                   std::int32_t* changed)
{
    const auto edge = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (edge >= edge_count)
    {
        return;
    }

    const auto source = edge_sources[edge];
    const auto target = column_indices[edge];
    const auto source_distance = distances[source];
    if (source_distance >= kWeightedInfDistance)
    {
        return;
    }

    const auto candidate = source_distance + weights[edge];
    const auto old = atomicMin(&distances[target], candidate);
    if (candidate < old)
    {
        *changed = 1;
    }
}

__global__ void frontier_append_relax_kernel(const std::int64_t* row_offsets,
                                             const std::int32_t* column_indices,
                                             const std::int32_t* weights,
                                             const std::int32_t* current_frontier,
                                             std::int32_t current_frontier_size,
                                             std::int32_t* distances,
                                             std::int32_t* next_frontier,
                                             std::int32_t* next_frontier_size,
                                             std::int32_t next_frontier_capacity,
                                             std::int32_t* overflow)
{
    const auto index = static_cast<std::int32_t>(blockIdx.x * blockDim.x + threadIdx.x);
    if (index >= current_frontier_size)
    {
        return;
    }

    const auto source = current_frontier[index];
    const auto source_distance = distances[source];
    if (source_distance >= kWeightedInfDistance)
    {
        return;
    }

    const auto begin = row_offsets[source];
    const auto end = row_offsets[source + 1];
    for (auto edge = begin; edge < end; ++edge)
    {
        const auto target = column_indices[edge];
        const auto candidate = source_distance + weights[edge];
        const auto old = atomicMin(&distances[target], candidate);
        if (candidate < old)
        {
            const auto position = atomicAdd(next_frontier_size, 1);
            if (position < next_frontier_capacity)
            {
                next_frontier[position] = target;
            }
            else
            {
                *overflow = 1;
            }
        }
    }
}

std::vector<std::int32_t> make_edge_sources(const graph::CsrGraph& graph)
{
    std::vector<std::int32_t> sources(static_cast<std::size_t>(graph.edge_count()));
    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const auto begin = graph.row_offsets[static_cast<std::size_t>(node)];
        const auto end = graph.row_offsets[static_cast<std::size_t>(node + 1)];
        for (auto edge = begin; edge < end; ++edge)
        {
            sources[static_cast<std::size_t>(edge)] = node;
        }
    }
    return sources;
}

std::string variant_name(GpuWeightedAlgorithm algorithm)
{
    return algorithm == GpuWeightedAlgorithm::GlobalEdgeScan ? "gpu" : "gpu-frontier";
}

std::string algorithm_note(GpuWeightedAlgorithm algorithm)
{
    if (algorithm == GpuWeightedAlgorithm::GlobalEdgeScan)
    {
        return "iterative global edge relaxation over positive integer weighted CSR graph";
    }
    return "frontier-driven weighted relaxation over positive integer weighted CSR graph";
}

std::string algorithm_description(GpuWeightedAlgorithm algorithm)
{
    if (algorithm == GpuWeightedAlgorithm::GlobalEdgeScan)
    {
        return "global Bellman-Ford-style edge scan using atomicMin on every pass";
    }
    return "sparse append-frontier relaxation: only nodes whose distance changed expand outgoing edges; next frontier is appended directly without a full-node compaction pass";
}

void fill_metadata(BenchmarkResult& result,
                   const WeightedRelaxationShape& shape,
                   const WeightedCsrGraph& weighted,
                   const graph::GraphStats& stats,
                   const WeightedDistanceSummary& summary,
                   const GpuRelaxationRunStats& run_stats,
                   GpuWeightedAlgorithm algorithm)
{
    result.metadata["graph_kind"] = shape.graph_kind;
    result.metadata["shape"] = describe_weighted_shape(shape);
    result.metadata["source"] = std::to_string(shape.source);
    result.metadata["edge_count"] = std::to_string(weighted.graph.edge_count());
    result.metadata["min_out_degree"] = std::to_string(stats.min_out_degree);
    result.metadata["max_out_degree"] = std::to_string(stats.max_out_degree);
    result.metadata["mean_out_degree"] = std::to_string(stats.mean_out_degree);
    result.metadata["zero_out_degree_count"] = std::to_string(stats.zero_out_degree_count);
    result.metadata["reached_count"] = std::to_string(summary.reached_count);
    result.metadata["max_finite_distance"] = std::to_string(summary.max_finite_distance);
    result.metadata["mismatch_count"] = std::to_string(summary.mismatch_count);
    result.metadata["checksum"] = std::to_string(summary.checksum);
    result.metadata["reference_checksum"] = std::to_string(summary.reference_checksum);
    result.metadata["iterations"] = std::to_string(run_stats.iterations);
    result.metadata["max_iterations"] = std::to_string(weighted_default_max_iterations(shape, weighted));
    result.metadata["converged"] = run_stats.converged ? "true" : "false";
    result.metadata["edge_iterations"] = std::to_string(run_stats.edge_iterations);
    result.metadata["active_frontier_node_visits"] = std::to_string(run_stats.active_frontier_node_visits);
    result.metadata["max_frontier_size"] = std::to_string(run_stats.max_frontier_size);
    result.metadata["frontier_capacity"] = std::to_string(run_stats.frontier_capacity);
    result.metadata["frontier_overflow"] = run_stats.frontier_overflow ? "true" : "false";
    result.metadata["cuda_status"] = cuda_runtime_status();
    result.metadata["gpu_algorithm"] = algorithm_description(algorithm);
    result.metadata["kernel_ms_semantics"] = algorithm == GpuWeightedAlgorithm::GlobalEdgeScan
        ? "timed global GPU relaxation loop; repeated full edge scans and host convergence checks are included"
        : "timed sparse active-frontier GPU relaxation loop; direct frontier append and host frontier-size checks are included";
    result.metadata["validation"] = "exact weighted-distance comparison against CPU Dijkstra reference";
}

void initialize_distances(const WeightedCsrGraph& weighted,
                          const WeightedRelaxationShape& shape,
                          std::int32_t* d_distances)
{
    const int threads = 256;
    const int node_blocks = (weighted.graph.node_count + threads - 1) / threads;
    fill_distances_kernel<<<node_blocks, threads>>>(d_distances, weighted.graph.node_count, kWeightedInfDistance);
    CUDA_CHECK(cudaGetLastError());
    const std::int32_t zero = 0;
    CUDA_CHECK(cudaMemcpy(d_distances + shape.source, &zero, sizeof(std::int32_t), cudaMemcpyHostToDevice));
}

GpuRelaxationRunStats run_global_edge_scan_once(const WeightedCsrGraph& weighted,
                                                const WeightedRelaxationShape& shape,
                                                const std::int32_t* d_edge_sources,
                                                const std::int32_t* d_column_indices,
                                                const std::int32_t* d_weights,
                                                std::int32_t* d_distances,
                                                std::int32_t* d_changed)
{
    const int threads = 256;
    const int edge_blocks = static_cast<int>((weighted.graph.edge_count() + threads - 1) / threads);
    const auto max_iterations = weighted_default_max_iterations(shape, weighted);

    initialize_distances(weighted, shape, d_distances);

    GpuRelaxationRunStats stats;
    for (std::int32_t iteration = 0; iteration < max_iterations; ++iteration)
    {
        std::int32_t host_changed = 0;
        CUDA_CHECK(cudaMemcpy(d_changed, &host_changed, sizeof(std::int32_t), cudaMemcpyHostToDevice));
        relax_edges_kernel<<<edge_blocks, threads>>>(d_edge_sources,
                                                     d_column_indices,
                                                     d_weights,
                                                     weighted.graph.edge_count(),
                                                     d_distances,
                                                     d_changed);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaMemcpy(&host_changed, d_changed, sizeof(std::int32_t), cudaMemcpyDeviceToHost));
        ++stats.iterations;
        stats.edge_iterations += weighted.graph.edge_count();
        if (host_changed == 0)
        {
            stats.converged = true;
            break;
        }
    }

    return stats;
}

GpuRelaxationRunStats run_frontier_relaxation_once(const WeightedCsrGraph& weighted,
                                                   const WeightedRelaxationShape& shape,
                                                   const std::int64_t* d_row_offsets,
                                                   const std::int32_t* d_column_indices,
                                                   const std::int32_t* d_weights,
                                                   std::int32_t* d_distances,
                                                   std::int32_t* d_current_frontier,
                                                   std::int32_t* d_next_frontier,
                                                   std::int32_t* d_next_frontier_size,
                                                   std::int32_t* d_frontier_overflow,
                                                   std::int32_t frontier_capacity)
{
    const int threads = 256;
    const auto max_iterations = weighted_default_max_iterations(shape, weighted);

    initialize_distances(weighted, shape, d_distances);
    CUDA_CHECK(cudaMemcpy(d_current_frontier, &shape.source, sizeof(std::int32_t), cudaMemcpyHostToDevice));

    std::int32_t current_frontier_size = weighted.graph.node_count > 0 ? 1 : 0;
    GpuRelaxationRunStats stats;
    stats.frontier_capacity = frontier_capacity;

    for (std::int32_t iteration = 0; iteration < max_iterations && current_frontier_size > 0; ++iteration)
    {
        stats.max_frontier_size = std::max(stats.max_frontier_size, current_frontier_size);
        stats.active_frontier_node_visits += current_frontier_size;

        CUDA_CHECK(cudaMemset(d_next_frontier_size, 0, sizeof(std::int32_t)));
        CUDA_CHECK(cudaMemset(d_frontier_overflow, 0, sizeof(std::int32_t)));

        const int frontier_blocks = (current_frontier_size + threads - 1) / threads;
        frontier_append_relax_kernel<<<frontier_blocks, threads>>>(d_row_offsets,
                                                                   d_column_indices,
                                                                   d_weights,
                                                                   d_current_frontier,
                                                                   current_frontier_size,
                                                                   d_distances,
                                                                   d_next_frontier,
                                                                   d_next_frontier_size,
                                                                   frontier_capacity,
                                                                   d_frontier_overflow);
        CUDA_CHECK(cudaGetLastError());

        std::int32_t next_frontier_size = 0;
        std::int32_t overflow = 0;
        CUDA_CHECK(cudaMemcpy(&next_frontier_size, d_next_frontier_size, sizeof(std::int32_t), cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(&overflow, d_frontier_overflow, sizeof(std::int32_t), cudaMemcpyDeviceToHost));

        ++stats.iterations;
        if (overflow != 0)
        {
            stats.frontier_overflow = true;
            stats.converged = false;
            break;
        }

        std::swap(d_current_frontier, d_next_frontier);
        current_frontier_size = next_frontier_size;
    }

    stats.converged = !stats.frontier_overflow && current_frontier_size == 0;
    // The append-frontier variant does not scan all edges. We expose active
    // node visits and mean degree to the analysis layer, which estimates active
    // edge work as active_frontier_node_visits * mean_out_degree.
    stats.edge_iterations = 0;
    return stats;
}

BenchmarkResult run_gpu_variant(const BenchmarkConfig& config, GpuWeightedAlgorithm algorithm)
{
    BenchmarkResult result;
    result.benchmark = "graph_weighted_relaxation";
    result.variant = variant_name(algorithm);
    result.preset = weighted_result_preset_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.device = cuda_device_name();
    result.notes = algorithm_note(algorithm);
    result.metadata["cuda_status"] = cuda_runtime_status();

    const auto shape = weighted_shape_for_config(config);
    const auto weighted = make_weighted_graph_for_shape(shape);
    const auto stats = graph::summarize_graph(weighted.graph);
    const auto edge_sources = make_edge_sources(weighted.graph);
    result.input_size["nodes"] = weighted.graph.node_count;
    result.input_size["edges"] = weighted.graph.edge_count();

    if (!cuda_runtime_available())
    {
        result.correct = false;
        result.metadata["status"] = "CUDA runtime unavailable";
        return result;
    }

    std::int32_t* d_edge_sources = nullptr;
    std::int64_t* d_row_offsets = nullptr;
    std::int32_t* d_column_indices = nullptr;
    std::int32_t* d_weights = nullptr;
    std::int32_t* d_distances = nullptr;
    std::int32_t* d_changed = nullptr;
    std::int32_t* d_current_frontier = nullptr;
    std::int32_t* d_next_frontier = nullptr;
    std::int32_t* d_next_frontier_size = nullptr;
    std::int32_t* d_frontier_overflow = nullptr;

    const std::size_t edge_bytes = static_cast<std::size_t>(weighted.graph.edge_count()) * sizeof(std::int32_t);
    const std::size_t row_offset_bytes = static_cast<std::size_t>(weighted.graph.node_count + 1) * sizeof(std::int64_t);
    const std::size_t distance_bytes = static_cast<std::size_t>(weighted.graph.node_count) * sizeof(std::int32_t);
    const std::int64_t frontier_capacity64 = std::max<std::int64_t>(
        weighted.graph.node_count,
        std::min<std::int64_t>(weighted.graph.edge_count() * 4, 50'000'000));
    const auto frontier_capacity = static_cast<std::int32_t>(frontier_capacity64);
    const std::size_t frontier_bytes = static_cast<std::size_t>(frontier_capacity) * sizeof(std::int32_t);

    CUDA_CHECK(cudaMalloc(&d_edge_sources, edge_bytes));
    CUDA_CHECK(cudaMalloc(&d_row_offsets, row_offset_bytes));
    CUDA_CHECK(cudaMalloc(&d_column_indices, edge_bytes));
    CUDA_CHECK(cudaMalloc(&d_weights, edge_bytes));
    CUDA_CHECK(cudaMalloc(&d_distances, distance_bytes));
    CUDA_CHECK(cudaMalloc(&d_changed, sizeof(std::int32_t)));
    CUDA_CHECK(cudaMalloc(&d_current_frontier, frontier_bytes));
    CUDA_CHECK(cudaMalloc(&d_next_frontier, frontier_bytes));
    CUDA_CHECK(cudaMalloc(&d_next_frontier_size, sizeof(std::int32_t)));
    CUDA_CHECK(cudaMalloc(&d_frontier_overflow, sizeof(std::int32_t)));

    cudaEvent_t h2d_start{}, h2d_stop{};
    CUDA_CHECK(cudaEventCreate(&h2d_start));
    CUDA_CHECK(cudaEventCreate(&h2d_stop));
    CUDA_CHECK(cudaEventRecord(h2d_start));
    CUDA_CHECK(cudaMemcpy(d_edge_sources, edge_sources.data(), edge_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_row_offsets, weighted.graph.row_offsets.data(), row_offset_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_column_indices, weighted.graph.column_indices.data(), edge_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_weights, weighted.weights.data(), edge_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(h2d_stop));
    CUDA_CHECK(cudaEventSynchronize(h2d_stop));
    float h2d_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&h2d_ms, h2d_start, h2d_stop));

    auto run_once = [&]() {
        if (algorithm == GpuWeightedAlgorithm::GlobalEdgeScan)
        {
            return run_global_edge_scan_once(weighted, shape, d_edge_sources, d_column_indices, d_weights, d_distances, d_changed);
        }
        return run_frontier_relaxation_once(weighted,
                                            shape,
                                            d_row_offsets,
                                            d_column_indices,
                                            d_weights,
                                            d_distances,
                                            d_current_frontier,
                                            d_next_frontier,
                                            d_next_frontier_size,
                                            d_frontier_overflow,
                                            frontier_capacity);
    };

    GpuRelaxationRunStats run_stats;
    for (int i = 0; i < config.warmup; ++i)
    {
        run_stats = run_once();
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    cudaEvent_t kernel_start{}, kernel_stop{};
    CUDA_CHECK(cudaEventCreate(&kernel_start));
    CUDA_CHECK(cudaEventCreate(&kernel_stop));
    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        run_stats = run_once();
    }
    CUDA_CHECK(cudaEventRecord(kernel_stop));
    CUDA_CHECK(cudaEventSynchronize(kernel_stop));
    CUDA_CHECK(cudaGetLastError());
    float kernel_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&kernel_ms, kernel_start, kernel_stop));

    std::vector<std::int32_t> host_distances(static_cast<std::size_t>(weighted.graph.node_count), kWeightedInfDistance);
    cudaEvent_t d2h_start{}, d2h_stop{};
    CUDA_CHECK(cudaEventCreate(&d2h_start));
    CUDA_CHECK(cudaEventCreate(&d2h_stop));
    CUDA_CHECK(cudaEventRecord(d2h_start));
    CUDA_CHECK(cudaMemcpy(host_distances.data(), d_distances, distance_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(d2h_stop));
    CUDA_CHECK(cudaEventSynchronize(d2h_stop));
    float d2h_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&d2h_ms, d2h_start, d2h_stop));

    const auto summary = validate_weighted_distances(weighted, shape.source, host_distances);

    result.h2d_ms = static_cast<double>(h2d_ms);
    result.kernel_ms = static_cast<double>(kernel_ms);
    result.d2h_ms = static_cast<double>(d2h_ms);
    result.total_ms = result.h2d_ms + result.kernel_ms + result.d2h_ms;
    result.correct = summary.correct && run_stats.converged && !run_stats.frontier_overflow;
    fill_metadata(result, shape, weighted, stats, summary, run_stats, algorithm);

    CUDA_CHECK(cudaEventDestroy(h2d_start));
    CUDA_CHECK(cudaEventDestroy(h2d_stop));
    CUDA_CHECK(cudaEventDestroy(kernel_start));
    CUDA_CHECK(cudaEventDestroy(kernel_stop));
    CUDA_CHECK(cudaEventDestroy(d2h_start));
    CUDA_CHECK(cudaEventDestroy(d2h_stop));
    CUDA_CHECK(cudaFree(d_edge_sources));
    CUDA_CHECK(cudaFree(d_row_offsets));
    CUDA_CHECK(cudaFree(d_column_indices));
    CUDA_CHECK(cudaFree(d_weights));
    CUDA_CHECK(cudaFree(d_distances));
    CUDA_CHECK(cudaFree(d_changed));
    CUDA_CHECK(cudaFree(d_current_frontier));
    CUDA_CHECK(cudaFree(d_next_frontier));
    CUDA_CHECK(cudaFree(d_next_frontier_size));
    CUDA_CHECK(cudaFree(d_frontier_overflow));

    return result;
}

} // namespace

std::vector<BenchmarkResult> run_graph_weighted_relaxation_gpu(const BenchmarkConfig& config)
{
    std::vector<BenchmarkResult> results;
    results.push_back(run_gpu_variant(config, GpuWeightedAlgorithm::GlobalEdgeScan));
    results.push_back(run_gpu_variant(config, GpuWeightedAlgorithm::FrontierQueue));
    return results;
}

} // namespace algobench::graph_weighted
