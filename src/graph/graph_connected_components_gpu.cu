#include "graph/graph_connected_components.hpp"

#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace algobench::graph_cc
{
namespace
{

__global__ void initialize_labels_kernel(std::int32_t* labels, std::int32_t node_count)
{
    const auto index = static_cast<std::int32_t>(blockIdx.x * blockDim.x + threadIdx.x);
    if (index < node_count)
    {
        labels[index] = index;
    }
}

// -----------------------------------------------------------------------------
// Baseline GPU algorithm: node-frontier-ish label propagation.
//
// This is the original educational implementation. One thread owns a node,
// scans its outgoing CSR neighbors, and pushes the lower observed label with
// atomicMin. It is simple and easy to reason about, but it can require many
// synchronization rounds on chain-like / mixed anatomy.
// -----------------------------------------------------------------------------
__global__ void naive_hook_labels_kernel(const std::int64_t* row_offsets,
                                         const std::int32_t* column_indices,
                                         std::int32_t* labels,
                                         std::int32_t node_count,
                                         std::int32_t* changed)
{
    const auto node = static_cast<std::int32_t>(blockIdx.x * blockDim.x + threadIdx.x);
    if (node >= node_count)
    {
        return;
    }

    const auto node_label = labels[node];
    const auto begin = row_offsets[node];
    const auto end = row_offsets[node + 1];

    for (auto edge = begin; edge < end; ++edge)
    {
        const auto neighbor = column_indices[edge];
        const auto neighbor_label = labels[neighbor];
        const auto lower = node_label < neighbor_label ? node_label : neighbor_label;

        if (lower < labels[node])
        {
            atomicMin(&labels[node], lower);
            *changed = 1;
        }
        if (lower < labels[neighbor])
        {
            atomicMin(&labels[neighbor], lower);
            *changed = 1;
        }
    }
}

__global__ void compress_labels_kernel(std::int32_t* labels, std::int32_t node_count, std::int32_t* changed)
{
    const auto node = static_cast<std::int32_t>(blockIdx.x * blockDim.x + threadIdx.x);
    if (node >= node_count)
    {
        return;
    }

    const auto current = labels[node];
    const auto parent = labels[current];
    if (parent < current)
    {
        labels[node] = parent;
        *changed = 1;
    }
}

// -----------------------------------------------------------------------------
// Non-naive GPU algorithm: edge-parallel root hooking + full pointer jumping.
//
// This is still intentionally compact and educational, but it is more aggressive
// than the baseline:
//   1. work is distributed by edges rather than by nodes;
//   2. each edge hooks component roots, not just current endpoint labels;
//   3. a pointer-jumping pass compresses each node toward its current root.
//
// The goal is to reduce the number of global convergence rounds on graph
// families whose slow parts dominate the naive label-propagation loop.
// -----------------------------------------------------------------------------
__device__ std::int32_t find_root_device(const std::int32_t* labels, std::int32_t node)
{
    std::int32_t parent = labels[node];
    // Labels only move downward, so this cannot form cycles unless memory is
    // corrupt. The guard is deliberately high enough for our benchmark sizes but
    // prevents accidental infinite loops from hanging the device.
    std::int32_t guard = 0;
    while (parent != labels[parent] && guard < 1'000'000)
    {
        parent = labels[parent];
        ++guard;
    }
    return parent;
}

__global__ void root_hook_edges_kernel(const std::int32_t* edge_sources,
                                       const std::int32_t* column_indices,
                                       std::int64_t edge_count,
                                       std::int32_t* labels,
                                       std::int32_t* changed)
{
    const auto edge = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    if (edge >= edge_count)
    {
        return;
    }

    const auto source = edge_sources[edge];
    const auto target = column_indices[edge];
    auto source_root = find_root_device(labels, source);
    auto target_root = find_root_device(labels, target);

    while (source_root != target_root)
    {
        const auto high = source_root > target_root ? source_root : target_root;
        const auto low = source_root > target_root ? target_root : source_root;
        const auto old = atomicMin(&labels[high], low);
        if (old == high || old > low)
        {
            *changed = 1;
            break;
        }

        // Another thread already moved one of the roots. Re-read the roots and
        // try again. This keeps the kernel correct under concurrent hooking.
        source_root = find_root_device(labels, source_root);
        target_root = find_root_device(labels, target_root);
    }
}

__global__ void compress_to_root_kernel(std::int32_t* labels, std::int32_t node_count, std::int32_t* changed)
{
    const auto node = static_cast<std::int32_t>(blockIdx.x * blockDim.x + threadIdx.x);
    if (node >= node_count)
    {
        return;
    }

    const auto current = labels[node];
    const auto root = find_root_device(labels, current);
    if (root < current)
    {
        labels[node] = root;
        *changed = 1;
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

std::int32_t default_max_iterations(const GraphCcShape& shape, const graph::CsrGraph& graph)
{
    if (shape.max_iterations > 0)
    {
        return shape.max_iterations;
    }
    if (shape.graph_kind == "chains" || shape.graph_kind == "chain_components")
    {
        return std::max<std::int32_t>(1, shape.component_size + 8);
    }
    if (shape.graph_kind == "grid_components" || shape.graph_kind == "grids")
    {
        return std::max<std::int32_t>(1, shape.grid_width + shape.grid_height + 8);
    }
    return std::max<std::int32_t>(64, std::min<std::int32_t>(2048, graph.node_count));
}

void fill_metadata(BenchmarkResult& result,
                   const GraphCcShape& shape,
                   const graph::CsrGraph& graph,
                   const graph::GraphStats& stats,
                   const ConnectedComponentsSummary& summary,
                   std::int32_t iterations,
                   bool converged,
                   const std::string& gpu_algorithm)
{
    result.metadata["graph_kind"] = shape.graph_kind;
    result.metadata["shape"] = describe_cc_shape(shape);
    result.metadata["edge_count"] = std::to_string(graph.edge_count());
    result.metadata["min_out_degree"] = std::to_string(stats.min_out_degree);
    result.metadata["max_out_degree"] = std::to_string(stats.max_out_degree);
    result.metadata["mean_out_degree"] = std::to_string(stats.mean_out_degree);
    result.metadata["zero_out_degree_count"] = std::to_string(stats.zero_out_degree_count);
    result.metadata["component_count"] = std::to_string(summary.component_count);
    result.metadata["largest_component_size"] = std::to_string(summary.largest_component_size);
    result.metadata["isolated_node_count"] = std::to_string(summary.isolated_node_count);
    result.metadata["mismatch_count"] = std::to_string(summary.mismatch_count);
    result.metadata["checksum"] = std::to_string(summary.checksum);
    result.metadata["reference_checksum"] = std::to_string(summary.reference_checksum);
    result.metadata["iterations"] = std::to_string(iterations);
    result.metadata["converged"] = converged ? "true" : "false";
    result.metadata["max_iterations"] = std::to_string(default_max_iterations(shape, graph));
    result.metadata["validation"] = "component labels normalized and compared against CPU Union-Find reference";
    result.metadata["gpu_algorithm"] = gpu_algorithm;
    result.metadata["kernel_ms_semantics"] = "timed GPU convergence loop, not a pure single-kernel interval";
    result.metadata["cuda_status"] = cuda_runtime_status();
}

std::int32_t run_gpu_cc_naive_once(const graph::CsrGraph& graph,
                                   const GraphCcShape& shape,
                                   const std::int64_t* d_row_offsets,
                                   const std::int32_t* d_column_indices,
                                   std::int32_t* d_labels,
                                   std::int32_t* d_changed,
                                   bool& converged)
{
    const int threads = 256;
    const int node_blocks = (graph.node_count + threads - 1) / threads;
    const auto max_iterations = default_max_iterations(shape, graph);

    initialize_labels_kernel<<<node_blocks, threads>>>(d_labels, graph.node_count);
    CUDA_CHECK(cudaGetLastError());

    std::int32_t host_changed = 0;
    std::int32_t iterations = 0;
    converged = false;

    for (std::int32_t iteration = 0; iteration < max_iterations; ++iteration)
    {
        host_changed = 0;
        CUDA_CHECK(cudaMemcpy(d_changed, &host_changed, sizeof(std::int32_t), cudaMemcpyHostToDevice));

        naive_hook_labels_kernel<<<node_blocks, threads>>>(d_row_offsets, d_column_indices, d_labels, graph.node_count, d_changed);
        CUDA_CHECK(cudaGetLastError());
        compress_labels_kernel<<<node_blocks, threads>>>(d_labels, graph.node_count, d_changed);
        CUDA_CHECK(cudaGetLastError());

        CUDA_CHECK(cudaMemcpy(&host_changed, d_changed, sizeof(std::int32_t), cudaMemcpyDeviceToHost));
        ++iterations;
        if (host_changed == 0)
        {
            converged = true;
            break;
        }
    }

    return iterations;
}

std::int32_t run_gpu_cc_non_naive_once(const graph::CsrGraph& graph,
                                       const GraphCcShape& shape,
                                       const std::int32_t* d_edge_sources,
                                       const std::int32_t* d_column_indices,
                                       std::int32_t* d_labels,
                                       std::int32_t* d_changed,
                                       bool& converged)
{
    const int threads = 256;
    const int node_blocks = (graph.node_count + threads - 1) / threads;
    const auto edge_count = graph.edge_count();
    const int edge_blocks = static_cast<int>((edge_count + threads - 1) / threads);
    const auto max_iterations = default_max_iterations(shape, graph);

    initialize_labels_kernel<<<node_blocks, threads>>>(d_labels, graph.node_count);
    CUDA_CHECK(cudaGetLastError());

    std::int32_t host_changed = 0;
    std::int32_t iterations = 0;
    converged = false;

    for (std::int32_t iteration = 0; iteration < max_iterations; ++iteration)
    {
        host_changed = 0;
        CUDA_CHECK(cudaMemcpy(d_changed, &host_changed, sizeof(std::int32_t), cudaMemcpyHostToDevice));

        if (edge_count > 0)
        {
            root_hook_edges_kernel<<<edge_blocks, threads>>>(d_edge_sources, d_column_indices, edge_count, d_labels, d_changed);
            CUDA_CHECK(cudaGetLastError());
        }
        compress_to_root_kernel<<<node_blocks, threads>>>(d_labels, graph.node_count, d_changed);
        CUDA_CHECK(cudaGetLastError());

        CUDA_CHECK(cudaMemcpy(&host_changed, d_changed, sizeof(std::int32_t), cudaMemcpyDeviceToHost));
        ++iterations;
        if (host_changed == 0)
        {
            converged = true;
            break;
        }
    }

    return iterations;
}

enum class GpuCcVariant
{
    Naive,
    NonNaive,
};

BenchmarkResult run_one_gpu_variant(const BenchmarkConfig& config,
                                    const GraphCcShape& shape,
                                    const graph::CsrGraph& graph,
                                    const graph::GraphStats& stats,
                                    GpuCcVariant variant)
{
    BenchmarkResult result;
    result.benchmark = "graph_connected_components";
    result.variant = variant == GpuCcVariant::Naive ? "gpu" : "gpu-non-naive";
    result.preset = cc_result_preset_label_for_config(config);
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.device = cuda_device_name();
    result.notes = variant == GpuCcVariant::Naive
                       ? "naive iterative label-propagation connected components over CSR graph"
                       : "non-naive edge-root-hooking connected components over CSR graph";
    result.metadata["cuda_status"] = cuda_runtime_status();
    result.input_size["nodes"] = graph.node_count;
    result.input_size["edges"] = graph.edge_count();

    if (!cuda_runtime_available())
    {
        result.correct = false;
        result.metadata["status"] = "CUDA runtime unavailable";
        return result;
    }

    const auto edge_sources = variant == GpuCcVariant::NonNaive ? make_edge_sources(graph) : std::vector<std::int32_t>{};

    std::int64_t* d_row_offsets = nullptr;
    std::int32_t* d_column_indices = nullptr;
    std::int32_t* d_edge_sources = nullptr;
    std::int32_t* d_labels = nullptr;
    std::int32_t* d_changed = nullptr;

    const std::size_t row_bytes = graph.row_offsets.size() * sizeof(std::int64_t);
    const std::size_t col_bytes = graph.column_indices.size() * sizeof(std::int32_t);
    const std::size_t source_bytes = edge_sources.size() * sizeof(std::int32_t);
    const std::size_t label_bytes = static_cast<std::size_t>(graph.node_count) * sizeof(std::int32_t);

    CUDA_CHECK(cudaMalloc(&d_row_offsets, row_bytes));
    CUDA_CHECK(cudaMalloc(&d_column_indices, col_bytes));
    if (variant == GpuCcVariant::NonNaive && source_bytes > 0)
    {
        CUDA_CHECK(cudaMalloc(&d_edge_sources, source_bytes));
    }
    CUDA_CHECK(cudaMalloc(&d_labels, label_bytes));
    CUDA_CHECK(cudaMalloc(&d_changed, sizeof(std::int32_t)));

    cudaEvent_t h2d_start{}, h2d_stop{};
    CUDA_CHECK(cudaEventCreate(&h2d_start));
    CUDA_CHECK(cudaEventCreate(&h2d_stop));
    CUDA_CHECK(cudaEventRecord(h2d_start));
    CUDA_CHECK(cudaMemcpy(d_row_offsets, graph.row_offsets.data(), row_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_column_indices, graph.column_indices.data(), col_bytes, cudaMemcpyHostToDevice));
    if (variant == GpuCcVariant::NonNaive && source_bytes > 0)
    {
        CUDA_CHECK(cudaMemcpy(d_edge_sources, edge_sources.data(), source_bytes, cudaMemcpyHostToDevice));
    }
    CUDA_CHECK(cudaEventRecord(h2d_stop));
    CUDA_CHECK(cudaEventSynchronize(h2d_stop));

    float h2d_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&h2d_ms, h2d_start, h2d_stop));

    auto run_once = [&]() {
        bool local_converged = false;
        std::int32_t local_iterations = 0;
        if (variant == GpuCcVariant::Naive)
        {
            local_iterations = run_gpu_cc_naive_once(graph, shape, d_row_offsets, d_column_indices, d_labels, d_changed, local_converged);
        }
        else
        {
            local_iterations = run_gpu_cc_non_naive_once(graph, shape, d_edge_sources, d_column_indices, d_labels, d_changed, local_converged);
        }
        return std::pair<std::int32_t, bool>{local_iterations, local_converged};
    };

    std::int32_t iterations = 0;
    bool converged = false;
    for (int i = 0; i < config.warmup; ++i)
    {
        const auto run = run_once();
        iterations = run.first;
        converged = run.second;
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    cudaEvent_t kernel_start{}, kernel_stop{};
    CUDA_CHECK(cudaEventCreate(&kernel_start));
    CUDA_CHECK(cudaEventCreate(&kernel_stop));
    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        const auto run = run_once();
        iterations = run.first;
        converged = run.second;
    }
    CUDA_CHECK(cudaEventRecord(kernel_stop));
    CUDA_CHECK(cudaEventSynchronize(kernel_stop));
    CUDA_CHECK(cudaGetLastError());

    float kernel_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&kernel_ms, kernel_start, kernel_stop));

    std::vector<std::int32_t> labels(static_cast<std::size_t>(graph.node_count));
    cudaEvent_t d2h_start{}, d2h_stop{};
    CUDA_CHECK(cudaEventCreate(&d2h_start));
    CUDA_CHECK(cudaEventCreate(&d2h_stop));
    CUDA_CHECK(cudaEventRecord(d2h_start));
    CUDA_CHECK(cudaMemcpy(labels.data(), d_labels, label_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(d2h_stop));
    CUDA_CHECK(cudaEventSynchronize(d2h_stop));

    float d2h_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&d2h_ms, d2h_start, d2h_stop));

    const auto summary = validate_component_labels(graph, labels);

    result.h2d_ms = static_cast<double>(h2d_ms);
    result.kernel_ms = static_cast<double>(kernel_ms);
    result.d2h_ms = static_cast<double>(d2h_ms);
    result.total_ms = result.h2d_ms + result.kernel_ms + result.d2h_ms;
    result.correct = summary.correct && converged;
    fill_metadata(result,
                  shape,
                  graph,
                  stats,
                  summary,
                  iterations,
                  converged,
                  variant == GpuCcVariant::Naive
                      ? "naive node-parallel label propagation with atomicMin plus one-step pointer jumping"
                      : "non-naive edge-parallel root hooking plus full pointer-jumping compression");

    CUDA_CHECK(cudaEventDestroy(h2d_start));
    CUDA_CHECK(cudaEventDestroy(h2d_stop));
    CUDA_CHECK(cudaEventDestroy(kernel_start));
    CUDA_CHECK(cudaEventDestroy(kernel_stop));
    CUDA_CHECK(cudaEventDestroy(d2h_start));
    CUDA_CHECK(cudaEventDestroy(d2h_stop));
    CUDA_CHECK(cudaFree(d_row_offsets));
    CUDA_CHECK(cudaFree(d_column_indices));
    if (d_edge_sources != nullptr)
    {
        CUDA_CHECK(cudaFree(d_edge_sources));
    }
    CUDA_CHECK(cudaFree(d_labels));
    CUDA_CHECK(cudaFree(d_changed));

    return result;
}

} // namespace

std::vector<BenchmarkResult> run_graph_connected_components_gpu(const BenchmarkConfig& config)
{
    const auto shape = cc_shape_for_config(config);
    const auto graph = make_cc_graph_for_shape(shape);
    const auto stats = graph::summarize_graph(graph);

    std::vector<BenchmarkResult> results;
    results.push_back(run_one_gpu_variant(config, shape, graph, stats, GpuCcVariant::Naive));
    results.push_back(run_one_gpu_variant(config, shape, graph, stats, GpuCcVariant::NonNaive));
    return results;
}

} // namespace algobench::graph_cc
