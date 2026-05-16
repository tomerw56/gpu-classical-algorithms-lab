#include "spatial_events/spatial_events.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace algobench::spatial_events
{

namespace
{

void count_event(SpatialEventValidation& summary, SpatialEventCode code, bool reference)
{
    std::int64_t* none = reference ? &summary.reference_none_count : &summary.none_count;
    std::int64_t* enter = reference ? &summary.reference_enter_count : &summary.enter_count;
    std::int64_t* exit = reference ? &summary.reference_exit_count : &summary.exit_count;
    std::int64_t* stay = reference ? &summary.reference_stay_inside_count : &summary.stay_inside_count;
    std::int64_t* cross = reference ? &summary.reference_cross_through_count : &summary.cross_through_count;

    switch (code)
    {
    case SpatialEventCode::None:
        ++(*none);
        break;
    case SpatialEventCode::Enter:
        ++(*enter);
        break;
    case SpatialEventCode::Exit:
        ++(*exit);
        break;
    case SpatialEventCode::StayInside:
        ++(*stay);
        break;
    case SpatialEventCode::CrossThrough:
        ++(*cross);
        break;
    }
}

} // namespace

SpatialEventShape shape_for_preset(const std::string& preset)
{
    if (preset == "tiny")
    {
        return {64, 32};
    }
    if (preset == "small")
    {
        return {512, 256};
    }
    if (preset == "medium")
    {
        return {4096, 512};
    }
    if (preset == "large")
    {
        return {8192, 1024};
    }

    throw std::runtime_error("unknown spatial_events preset: " + preset);
}

std::vector<TrackSegment> make_track_segments(std::int64_t track_count)
{
    std::vector<TrackSegment> tracks(static_cast<std::size_t>(track_count));

    for (std::int64_t i = 0; i < track_count; ++i)
    {
        TrackSegment track;
        track.x0 = static_cast<float>((i * 37 + 11) % 2000) * 0.50f;
        track.y0 = static_cast<float>((i * 53 + 19) % 2000) * 0.50f;

        // Direction and span vary deterministically. The chosen magnitudes are
        // large enough for some segments to pass through zones while keeping the
        // synthetic map compact enough for plenty of pairwise interactions.
        const float dx = static_cast<float>(static_cast<int>((i * 19) % 61) - 30) * 5.0f;
        const float dy = static_cast<float>(static_cast<int>((i * 23) % 61) - 30) * 4.5f;
        track.x1 = track.x0 + dx;
        track.y1 = track.y0 + dy;
        track.speed = 4.0f + static_cast<float>((i * 7) % 37) * 0.65f;

        tracks[static_cast<std::size_t>(i)] = track;
    }

    return tracks;
}

std::vector<CircularZone> make_zones(std::int64_t zone_count)
{
    std::vector<CircularZone> zones(static_cast<std::size_t>(zone_count));

    for (std::int64_t j = 0; j < zone_count; ++j)
    {
        CircularZone zone;
        zone.x = static_cast<float>((j * 59 + 7) % 2000) * 0.50f;
        zone.y = static_cast<float>((j * 71 + 29) % 2000) * 0.50f;
        zone.radius = 28.0f + static_cast<float>(j % 9) * 14.0f;
        zone.severity = 1.0f + static_cast<float>(j % 6) * 0.45f;
        zones[static_cast<std::size_t>(j)] = zone;
    }

    return zones;
}

SpatialEventValidation validate_spatial_events(const std::vector<std::uint8_t>& event_codes,
                                               const std::vector<float>& scores,
                                               const std::vector<TrackSegment>& tracks,
                                               const std::vector<CircularZone>& zones)
{
    SpatialEventValidation summary;

    const std::int64_t track_count = static_cast<std::int64_t>(tracks.size());
    const std::int64_t zone_count = static_cast<std::int64_t>(zones.size());
    const std::int64_t expected_cells = track_count * zone_count;

    if (static_cast<std::int64_t>(event_codes.size()) != expected_cells ||
        static_cast<std::int64_t>(scores.size()) != expected_cells)
    {
        summary.correct = false;
        return summary;
    }

    for (std::int64_t track_index = 0; track_index < track_count; ++track_index)
    {
        for (std::int64_t zone_index = 0; zone_index < zone_count; ++zone_index)
        {
            const std::int64_t flat_index = track_index * zone_count + zone_index;
            const auto reference = evaluate_track_zone(tracks[static_cast<std::size_t>(track_index)],
                                                       zones[static_cast<std::size_t>(zone_index)]);
            const auto actual_code = static_cast<SpatialEventCode>(event_codes[static_cast<std::size_t>(flat_index)]);
            const float actual_score = scores[static_cast<std::size_t>(flat_index)];

            count_event(summary, actual_code, false);
            count_event(summary, reference.code, true);

            if (actual_code != SpatialEventCode::None)
            {
                ++summary.event_count;
                summary.checksum += static_cast<double>(actual_score);
            }
            if (reference.code != SpatialEventCode::None)
            {
                ++summary.reference_event_count;
                summary.reference_checksum += static_cast<double>(reference.score);
            }

            if (actual_code != reference.code)
            {
                ++summary.event_mismatches;
                summary.correct = false;
            }

            const double abs_error = std::abs(static_cast<double>(actual_score) -
                                              static_cast<double>(reference.score));
            const double rel_denominator = std::max(1.0, std::abs(static_cast<double>(reference.score)));
            const double rel_error = abs_error / rel_denominator;
            summary.max_abs_error = std::max(summary.max_abs_error, abs_error);
            summary.max_rel_error = std::max(summary.max_rel_error, rel_error);

            const double tolerance = std::max(1.0e-5, std::abs(static_cast<double>(reference.score)) * 1.0e-5);
            if (!std::isfinite(actual_score) || abs_error > tolerance)
            {
                summary.correct = false;
            }
        }
    }

    return summary;
}

} // namespace algobench::spatial_events
