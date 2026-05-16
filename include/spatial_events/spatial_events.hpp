#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__CUDACC__)
#define ALGOBENCH_SPATIAL_HD __host__ __device__
#else
#define ALGOBENCH_SPATIAL_HD
#endif

namespace algobench::spatial_events
{

/**
 * Event classes emitted by the spatial-event benchmark.
 *
 * The benchmark evaluates one moving track segment against one circular zone:
 *
 * - `None`: the segment does not trigger the zone,
 * - `Enter`: previous position outside, current position inside,
 * - `Exit`: previous position inside, current position outside,
 * - `StayInside`: both endpoints remain inside,
 * - `CrossThrough`: both endpoints are outside, but the segment intersects the zone.
 */
enum class SpatialEventCode : std::uint8_t
{
    None = 0,
    Enter = 1,
    Exit = 2,
    StayInside = 3,
    CrossThrough = 4,
};

/** A two-sample movement segment for one tracked object. */
struct TrackSegment
{
    float x0 = 0.0f;     ///< Previous x-coordinate.
    float y0 = 0.0f;     ///< Previous y-coordinate.
    float x1 = 0.0f;     ///< Current x-coordinate.
    float y1 = 0.0f;     ///< Current y-coordinate.
    float speed = 0.0f;  ///< Synthetic movement speed used by the event score.
};

/** Circular event zone used by the benchmark. */
struct CircularZone
{
    float x = 0.0f;          ///< Zone-center x-coordinate.
    float y = 0.0f;          ///< Zone-center y-coordinate.
    float radius = 0.0f;     ///< Trigger radius.
    float severity = 1.0f;   ///< Weight used by the synthetic severity score.
};

/** Dense matrix dimensions for `event[track][zone]`. */
struct SpatialEventShape
{
    std::int64_t track_count = 0;
    std::int64_t zone_count = 0;

    std::int64_t cell_count() const
    {
        return track_count * zone_count;
    }
};

/** One pairwise spatial-event evaluation. */
struct SpatialEventEvaluation
{
    SpatialEventCode code = SpatialEventCode::None;
    float score = 0.0f;
};

/** Validation summary for the dense event-code and event-score matrices. */
struct SpatialEventValidation
{
    double checksum = 0.0;
    double reference_checksum = 0.0;
    double max_abs_error = 0.0;
    double max_rel_error = 0.0;

    std::int64_t event_count = 0;
    std::int64_t reference_event_count = 0;
    std::int64_t event_mismatches = 0;

    std::int64_t none_count = 0;
    std::int64_t enter_count = 0;
    std::int64_t exit_count = 0;
    std::int64_t stay_inside_count = 0;
    std::int64_t cross_through_count = 0;

    std::int64_t reference_none_count = 0;
    std::int64_t reference_enter_count = 0;
    std::int64_t reference_exit_count = 0;
    std::int64_t reference_stay_inside_count = 0;
    std::int64_t reference_cross_through_count = 0;

    bool correct = true;
};

/** Return the dense track-zone matrix dimensions associated with a preset. */
SpatialEventShape shape_for_preset(const std::string& preset);

/**
 * Build deterministic synthetic movement segments.
 *
 * The generator intentionally avoids randomness so CPU and GPU runs receive the
 * same tracks. Segment lengths and directions vary enough to produce enter,
 * exit, stay-inside, cross-through, and no-event pairs once combined with the
 * deterministic circular zones.
 */
std::vector<TrackSegment> make_track_segments(std::int64_t track_count);

/** Build deterministic circular zones for the benchmark. */
std::vector<CircularZone> make_zones(std::int64_t zone_count);

/**
 * Validate dense row-major event-code and event-score matrices against the
 * shared CPU/GPU pair evaluator.
 *
 * Storage layout:
 *
 *     flat_index = track_index * zone_count + zone_index
 */
SpatialEventValidation validate_spatial_events(const std::vector<std::uint8_t>& event_codes,
                                               const std::vector<float>& scores,
                                               const std::vector<TrackSegment>& tracks,
                                               const std::vector<CircularZone>& zones);

std::vector<BenchmarkResult> run_spatial_events_cpu(const BenchmarkConfig& config);
std::vector<BenchmarkResult> run_spatial_events_gpu(const BenchmarkConfig& config);

ALGOBENCH_SPATIAL_HD inline float spatial_min(float lhs, float rhs)
{
    return lhs < rhs ? lhs : rhs;
}

ALGOBENCH_SPATIAL_HD inline float spatial_max(float lhs, float rhs)
{
    return lhs > rhs ? lhs : rhs;
}

ALGOBENCH_SPATIAL_HD inline float spatial_clamp01(float value)
{
    return spatial_max(0.0f, spatial_min(1.0f, value));
}

ALGOBENCH_SPATIAL_HD inline float spatial_distance_squared(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return dx * dx + dy * dy;
}

ALGOBENCH_SPATIAL_HD inline std::uint8_t event_code_value(SpatialEventCode code)
{
    return static_cast<std::uint8_t>(code);
}

/**
 * Evaluate one track segment against one circular zone.
 *
 * This function is compiled for the CPU reference path and the CUDA kernel.
 * It deliberately includes several geometry branches, which makes the example
 * more realistic than a pure point-in-circle test while still remaining small
 * enough to audit.
 */
ALGOBENCH_SPATIAL_HD inline SpatialEventEvaluation evaluate_track_zone(const TrackSegment& track,
                                                                        const CircularZone& zone)
{
    SpatialEventEvaluation evaluation;

    const float radius_squared = zone.radius * zone.radius;
    const bool previous_inside = spatial_distance_squared(track.x0, track.y0, zone.x, zone.y) <= radius_squared;
    const bool current_inside = spatial_distance_squared(track.x1, track.y1, zone.x, zone.y) <= radius_squared;

    SpatialEventCode code = SpatialEventCode::None;

    if (!previous_inside && current_inside)
    {
        code = SpatialEventCode::Enter;
    }
    else if (previous_inside && !current_inside)
    {
        code = SpatialEventCode::Exit;
    }
    else if (previous_inside && current_inside)
    {
        code = SpatialEventCode::StayInside;
    }
    else
    {
        // Both endpoints are outside. Test whether the movement segment passes
        // through the circular zone by projecting the zone center onto the line
        // segment and comparing the closest-point distance to the radius.
        const float segment_dx = track.x1 - track.x0;
        const float segment_dy = track.y1 - track.y0;
        const float segment_length_squared = segment_dx * segment_dx + segment_dy * segment_dy;

        if (segment_length_squared > 1.0e-6f)
        {
            const float to_center_x = zone.x - track.x0;
            const float to_center_y = zone.y - track.y0;
            const float projection = spatial_clamp01((to_center_x * segment_dx + to_center_y * segment_dy) /
                                                     segment_length_squared);
            const float closest_x = track.x0 + projection * segment_dx;
            const float closest_y = track.y0 + projection * segment_dy;
            const float closest_distance_squared = spatial_distance_squared(closest_x, closest_y, zone.x, zone.y);

            if (closest_distance_squared <= radius_squared)
            {
                code = SpatialEventCode::CrossThrough;
            }
        }
    }

    float score = 0.0f;
    if (code == SpatialEventCode::Enter)
    {
        score = zone.severity * (4.0f + track.speed * 0.030f);
    }
    else if (code == SpatialEventCode::Exit)
    {
        score = zone.severity * (3.0f + track.speed * 0.020f);
    }
    else if (code == SpatialEventCode::StayInside)
    {
        score = zone.severity * (1.0f + track.speed * 0.010f);
    }
    else if (code == SpatialEventCode::CrossThrough)
    {
        score = zone.severity * (5.0f + track.speed * 0.040f);
    }

    evaluation.code = code;
    evaluation.score = score;
    return evaluation;
}

} // namespace algobench::spatial_events

#undef ALGOBENCH_SPATIAL_HD
