#include "spatial_events/spatial_events.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace
{

double metadata_double(const algobench::BenchmarkResult& result, const std::string& key)
{
    const auto it = result.metadata.find(key);
    TEST_CHECK(it != result.metadata.end());
    if (it == result.metadata.end())
    {
        return 0.0;
    }
    return std::strtod(it->second.c_str(), nullptr);
}

std::int64_t metadata_int64(const algobench::BenchmarkResult& result, const std::string& key)
{
    const auto it = result.metadata.find(key);
    TEST_CHECK(it != result.metadata.end());
    if (it == result.metadata.end())
    {
        return 0;
    }
    return static_cast<std::int64_t>(std::strtoll(it->second.c_str(), nullptr, 10));
}

} // namespace

int main()
{
    using namespace algobench::spatial_events;

    const auto tiny = shape_for_preset("tiny");
    const auto small = shape_for_preset("small");
    const auto medium = shape_for_preset("medium");
    const auto large = shape_for_preset("large");

    TEST_CHECK_EQ(tiny.track_count, 64);
    TEST_CHECK_EQ(tiny.zone_count, 32);
    TEST_CHECK_EQ(tiny.cell_count(), 2048);
    TEST_CHECK_EQ(small.track_count, 512);
    TEST_CHECK_EQ(small.zone_count, 256);
    TEST_CHECK_EQ(medium.track_count, 4096);
    TEST_CHECK_EQ(large.zone_count, 1024);
    TEST_CHECK_THROWS((void)shape_for_preset("invalid"));

    const auto tracks = make_track_segments(8);
    const auto zones = make_zones(8);
    TEST_CHECK_EQ(tracks.size(), static_cast<std::size_t>(8));
    TEST_CHECK_EQ(zones.size(), static_cast<std::size_t>(8));
    TEST_CHECK(zones[0].radius > 0.0f);
    TEST_CHECK(zones[0].severity >= 1.0f);

    const CircularZone zone{0.0f, 0.0f, 10.0f, 2.0f};

    const TrackSegment enter_track{-20.0f, 0.0f, 0.0f, 0.0f, 5.0f};
    const auto enter = evaluate_track_zone(enter_track, zone);
    TEST_CHECK(enter.code == SpatialEventCode::Enter);
    TEST_CHECK(enter.score > 0.0f);

    const TrackSegment exit_track{0.0f, 0.0f, 20.0f, 0.0f, 5.0f};
    const auto exit = evaluate_track_zone(exit_track, zone);
    TEST_CHECK(exit.code == SpatialEventCode::Exit);
    TEST_CHECK(exit.score > 0.0f);

    const TrackSegment stay_track{0.0f, 0.0f, 5.0f, 0.0f, 5.0f};
    const auto stay = evaluate_track_zone(stay_track, zone);
    TEST_CHECK(stay.code == SpatialEventCode::StayInside);
    TEST_CHECK(stay.score > 0.0f);

    const TrackSegment cross_track{-20.0f, 0.0f, 20.0f, 0.0f, 5.0f};
    const auto cross = evaluate_track_zone(cross_track, zone);
    TEST_CHECK(cross.code == SpatialEventCode::CrossThrough);
    TEST_CHECK(cross.score > 0.0f);

    const TrackSegment none_track{-30.0f, 25.0f, -20.0f, 25.0f, 5.0f};
    const auto none = evaluate_track_zone(none_track, zone);
    TEST_CHECK(none.code == SpatialEventCode::None);
    TEST_CHECK_EQ(none.score, 0.0f);

    algobench::BenchmarkConfig config;
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;

    const auto once = run_spatial_events_cpu(config);
    TEST_CHECK_EQ(once.size(), static_cast<std::size_t>(1));
    const auto& cpu_once = once.front();
    TEST_CHECK_EQ(cpu_once.benchmark, std::string("spatial_events"));
    TEST_CHECK_EQ(cpu_once.variant, std::string("cpu"));
    TEST_CHECK_EQ(cpu_once.preset, std::string("tiny"));
    TEST_CHECK_EQ(cpu_once.input_size.at("tracks"), 64);
    TEST_CHECK_EQ(cpu_once.input_size.at("zones"), 32);
    TEST_CHECK_EQ(cpu_once.input_size.at("cells"), 2048);
    TEST_CHECK(cpu_once.correct);
    TEST_CHECK(cpu_once.total_ms >= 0.0);
    TEST_CHECK(cpu_once.metadata.count("checksum") == 1);
    TEST_CHECK(cpu_once.metadata.count("reference_checksum") == 1);
    TEST_CHECK(cpu_once.metadata.count("event_count") == 1);
    TEST_CHECK(cpu_once.metadata.count("event_mismatches") == 1);

    const double checksum_once = metadata_double(cpu_once, "checksum");
    const double reference_once = metadata_double(cpu_once, "reference_checksum");
    TEST_CHECK_NEAR(checksum_once, reference_once,
                    std::max(1.0e-5, std::abs(reference_once) * 1.0e-6));
    TEST_CHECK_EQ(metadata_int64(cpu_once, "event_mismatches"), 0);
    TEST_CHECK(metadata_int64(cpu_once, "event_count") > 0);

    config.repeat = 3;
    const auto repeated = run_spatial_events_cpu(config);
    TEST_CHECK_EQ(repeated.size(), static_cast<std::size_t>(1));
    const double checksum_repeated = metadata_double(repeated.front(), "checksum");
    const double reference_repeated = metadata_double(repeated.front(), "reference_checksum");

    // Repeat controls timing only. It must not multiply the final event-score checksum.
    TEST_CHECK_NEAR(checksum_repeated, checksum_once,
                    std::max(1.0e-5, std::abs(checksum_once) * 1.0e-6));
    TEST_CHECK_NEAR(reference_repeated, reference_once,
                    std::max(1.0e-5, std::abs(reference_once) * 1.0e-6));

    return algobench::test::finish();
}
