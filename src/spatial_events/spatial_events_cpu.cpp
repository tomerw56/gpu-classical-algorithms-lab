#include "spatial_events/spatial_events.hpp"

#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace algobench::spatial_events
{
namespace
{

// Materialize the dense row-major event-code and event-score matrices on the CPU.
//
// Logical shape:
//     event[track_index][zone_index]
//
// Flat storage:
//     flat_index = track_index * zone_count + zone_index
void evaluate_event_matrix(std::vector<std::uint8_t>& event_codes,
                           std::vector<float>& scores,
                           const std::vector<TrackSegment>& tracks,
                           const std::vector<CircularZone>& zones)
{
    const std::int64_t track_count = static_cast<std::int64_t>(tracks.size());
    const std::int64_t zone_count = static_cast<std::int64_t>(zones.size());

    for (std::int64_t track_index = 0; track_index < track_count; ++track_index)
    {
        for (std::int64_t zone_index = 0; zone_index < zone_count; ++zone_index)
        {
            const std::int64_t flat_index = track_index * zone_count + zone_index;
            const auto evaluation = evaluate_track_zone(tracks[static_cast<std::size_t>(track_index)],
                                                        zones[static_cast<std::size_t>(zone_index)]);
            event_codes[static_cast<std::size_t>(flat_index)] = event_code_value(evaluation.code);
            scores[static_cast<std::size_t>(flat_index)] = evaluation.score;
        }
    }
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

std::vector<BenchmarkResult> run_spatial_events_cpu(const BenchmarkConfig& config)
{
    const auto shape = shape_for_preset(config.preset);
    const auto tracks = make_track_segments(shape.track_count);
    const auto zones = make_zones(shape.zone_count);

    std::vector<std::uint8_t> event_codes(static_cast<std::size_t>(shape.cell_count()),
                                          event_code_value(SpatialEventCode::None));
    std::vector<float> scores(static_cast<std::size_t>(shape.cell_count()), 0.0f);

    for (int i = 0; i < config.warmup; ++i)
    {
        evaluate_event_matrix(event_codes, scores, tracks, zones);
    }

    CpuTimer timer;
    timer.start();
    for (int r = 0; r < config.repeat; ++r)
    {
        evaluate_event_matrix(event_codes, scores, tracks, zones);
    }
    const double elapsed_ms = timer.stop_ms();

    const auto validation = validate_spatial_events(event_codes, scores, tracks, zones);

    BenchmarkResult result;
    result.benchmark = "spatial_events";
    result.variant = "cpu";
    result.preset = config.preset;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["tracks"] = shape.track_count;
    result.input_size["zones"] = shape.zone_count;
    result.input_size["cells"] = shape.cell_count();
    result.total_ms = elapsed_ms;
    result.correct = validation.correct;
    result.device = cpu_device_name();
    result.notes = "track-segment versus circular-zone event matrix using nested CPU loops";
    fill_metadata(result, validation);

    return {result};
}

} // namespace algobench::spatial_events
