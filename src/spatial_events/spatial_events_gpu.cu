#include "spatial_events/spatial_events.hpp"

#include "common/cuda_check.cuh"
#include "common/device_info.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace algobench::spatial_events
{
namespace
{

// CUDA materialization of the same dense row-major matrices used by the CPU path.
// One thread owns exactly one logical track-zone pair.
__global__ void spatial_events_kernel(const TrackSegment* tracks,
                                      std::int64_t track_count,
                                      const CircularZone* zones,
                                      std::int64_t zone_count,
                                      std::uint8_t* event_codes,
                                      float* scores)
{
    const std::int64_t flat_index = static_cast<std::int64_t>(blockIdx.x) * blockDim.x + threadIdx.x;
    const std::int64_t cell_count = track_count * zone_count;

    if (flat_index >= cell_count)
    {
        return;
    }

    const std::int64_t track_index = flat_index / zone_count;
    const std::int64_t zone_index = flat_index - track_index * zone_count;
    const auto evaluation = evaluate_track_zone(tracks[track_index], zones[zone_index]);
    event_codes[flat_index] = event_code_value(evaluation.code);
    scores[flat_index] = evaluation.score;
}

void fill_metadata(BenchmarkResult& result, const SpatialEventValidation& validation)
{
    result.metadata["checksum"] = std::to_string(validation.checksum);
    result.metadata["reference_checksum"] = std::to_string(validation.reference_checksum);
    result.metadata["event_count"] = std::to_string(validation.event_count);
    result.metadata["reference_event_count"] = std::to_string(validation.reference_event_count);
    result.metadata["event_mismatches"] = std::to_string(validation.event_mismatches);
    result.metadata["max_abs_error"] = std::to_string(validation.max_abs_error);
    result.metadata["max_rel_error"] = std::to_string(validation.max_rel_error);
    result.metadata["none_count"] = std::to_string(validation.none_count);
    result.metadata["enter_count"] = std::to_string(validation.enter_count);
    result.metadata["exit_count"] = std::to_string(validation.exit_count);
    result.metadata["stay_inside_count"] = std::to_string(validation.stay_inside_count);
    result.metadata["cross_through_count"] = std::to_string(validation.cross_through_count);
    result.metadata["reference_none_count"] = std::to_string(validation.reference_none_count);
    result.metadata["reference_enter_count"] = std::to_string(validation.reference_enter_count);
    result.metadata["reference_exit_count"] = std::to_string(validation.reference_exit_count);
    result.metadata["reference_stay_inside_count"] = std::to_string(validation.reference_stay_inside_count);
    result.metadata["reference_cross_through_count"] = std::to_string(validation.reference_cross_through_count);
    result.metadata["validation"] = "element-wise event-code and score comparison over final track-zone matrix";
    result.metadata["event_model"] = "none, enter, exit, stay_inside, cross_through";
}

} // namespace

std::vector<BenchmarkResult> run_spatial_events_gpu(const BenchmarkConfig& config)
{
    BenchmarkResult result;
    result.benchmark = "spatial_events";
    result.variant = "gpu";
    result.preset = config.preset;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.device = cuda_device_name();
    result.notes = "track-segment versus circular-zone event matrix with one CUDA thread per pair";
    result.metadata["cuda_status"] = cuda_runtime_status();

    const auto shape = shape_for_preset(config.preset);
    const auto tracks = make_track_segments(shape.track_count);
    const auto zones = make_zones(shape.zone_count);
    result.input_size["tracks"] = shape.track_count;
    result.input_size["zones"] = shape.zone_count;
    result.input_size["cells"] = shape.cell_count();

    if (!cuda_runtime_available())
    {
        result.correct = false;
        result.metadata["status"] = "CUDA runtime unavailable";
        return {result};
    }

    TrackSegment* d_tracks = nullptr;
    CircularZone* d_zones = nullptr;
    std::uint8_t* d_event_codes = nullptr;
    float* d_scores = nullptr;

    const std::size_t track_bytes = tracks.size() * sizeof(TrackSegment);
    const std::size_t zone_bytes = zones.size() * sizeof(CircularZone);
    const std::size_t event_code_bytes = static_cast<std::size_t>(shape.cell_count()) * sizeof(std::uint8_t);
    const std::size_t score_bytes = static_cast<std::size_t>(shape.cell_count()) * sizeof(float);

    CUDA_CHECK(cudaMalloc(&d_tracks, track_bytes));
    CUDA_CHECK(cudaMalloc(&d_zones, zone_bytes));
    CUDA_CHECK(cudaMalloc(&d_event_codes, event_code_bytes));
    CUDA_CHECK(cudaMalloc(&d_scores, score_bytes));

    cudaEvent_t h2d_start{}, h2d_stop{};
    CUDA_CHECK(cudaEventCreate(&h2d_start));
    CUDA_CHECK(cudaEventCreate(&h2d_stop));
    CUDA_CHECK(cudaEventRecord(h2d_start));
    CUDA_CHECK(cudaMemcpy(d_tracks, tracks.data(), track_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaMemcpy(d_zones, zones.data(), zone_bytes, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(h2d_stop));
    CUDA_CHECK(cudaEventSynchronize(h2d_stop));

    float h2d_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&h2d_ms, h2d_start, h2d_stop));

    const int threads = 256;
    const int blocks = static_cast<int>((shape.cell_count() + threads - 1) / threads);

    for (int i = 0; i < config.warmup; ++i)
    {
        spatial_events_kernel<<<blocks, threads>>>(d_tracks,
                                                   shape.track_count,
                                                   d_zones,
                                                   shape.zone_count,
                                                   d_event_codes,
                                                   d_scores);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    cudaEvent_t kernel_start{}, kernel_stop{};
    CUDA_CHECK(cudaEventCreate(&kernel_start));
    CUDA_CHECK(cudaEventCreate(&kernel_stop));
    CUDA_CHECK(cudaEventRecord(kernel_start));
    for (int r = 0; r < config.repeat; ++r)
    {
        spatial_events_kernel<<<blocks, threads>>>(d_tracks,
                                                   shape.track_count,
                                                   d_zones,
                                                   shape.zone_count,
                                                   d_event_codes,
                                                   d_scores);
    }
    CUDA_CHECK(cudaEventRecord(kernel_stop));
    CUDA_CHECK(cudaEventSynchronize(kernel_stop));
    CUDA_CHECK(cudaGetLastError());

    float kernel_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&kernel_ms, kernel_start, kernel_stop));

    std::vector<std::uint8_t> host_event_codes(static_cast<std::size_t>(shape.cell_count()),
                                               event_code_value(SpatialEventCode::None));
    std::vector<float> host_scores(static_cast<std::size_t>(shape.cell_count()), 0.0f);

    cudaEvent_t d2h_start{}, d2h_stop{};
    CUDA_CHECK(cudaEventCreate(&d2h_start));
    CUDA_CHECK(cudaEventCreate(&d2h_stop));
    CUDA_CHECK(cudaEventRecord(d2h_start));
    CUDA_CHECK(cudaMemcpy(host_event_codes.data(), d_event_codes, event_code_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaMemcpy(host_scores.data(), d_scores, score_bytes, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(d2h_stop));
    CUDA_CHECK(cudaEventSynchronize(d2h_stop));

    float d2h_ms = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&d2h_ms, d2h_start, d2h_stop));

    const auto validation = validate_spatial_events(host_event_codes, host_scores, tracks, zones);

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
    CUDA_CHECK(cudaFree(d_tracks));
    CUDA_CHECK(cudaFree(d_zones));
    CUDA_CHECK(cudaFree(d_event_codes));
    CUDA_CHECK(cudaFree(d_scores));

    return {result};
}

} // namespace algobench::spatial_events
