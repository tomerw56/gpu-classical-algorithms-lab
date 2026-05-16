#include "spatial_events/spatial_events.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct Options
{
    std::string preset = "tiny";
    std::filesystem::path output_dir = "results/spatial_events_visualization";
    std::int64_t track_count = 32;
    std::int64_t zone_count = 12;
    bool use_preset_shape = false;
    bool help = false;
};

std::string require_value(int& index, int argc, char** argv, const std::string& option)
{
    if (index + 1 >= argc)
    {
        throw std::runtime_error("missing value for " + option);
    }

    ++index;
    return argv[index];
}

std::int64_t parse_positive_int64(const std::string& text, const std::string& option)
{
    std::size_t consumed = 0;
    const long long value = std::stoll(text, &consumed, 10);
    if (consumed != text.size() || value <= 0)
    {
        throw std::runtime_error(option + " must be a positive integer");
    }

    return static_cast<std::int64_t>(value);
}

Options parse_options(int argc, char** argv)
{
    Options options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            options.help = true;
        }
        else if (arg == "--preset")
        {
            options.preset = require_value(i, argc, argv, arg);
            options.use_preset_shape = true;
        }
        else if (arg == "--tracks")
        {
            options.track_count = parse_positive_int64(require_value(i, argc, argv, arg), arg);
            options.use_preset_shape = false;
        }
        else if (arg == "--zones")
        {
            options.zone_count = parse_positive_int64(require_value(i, argc, argv, arg), arg);
            options.use_preset_shape = false;
        }
        else if (arg == "--output-dir")
        {
            options.output_dir = require_value(i, argc, argv, arg);
        }
        else
        {
            throw std::runtime_error("unknown option: " + arg);
        }
    }

    return options;
}

void print_help(const char* executable_name)
{
    std::cout
        << "Usage:\n"
        << "  " << executable_name << " [options]\n\n"
        << "Options:\n"
        << "  --preset NAME        Use a benchmark preset shape: tiny, small, medium, large\n"
        << "  --tracks N           Override track count for an inspectable custom scene. Default: 32\n"
        << "  --zones N            Override zone count for an inspectable custom scene. Default: 12\n"
        << "  --output-dir PATH    Directory for CSV and metadata export\n"
        << "  --help               Show this help\n\n"
        << "Notes:\n"
        << "  The default 32-track x 12-zone export is intentionally smaller than the benchmark\n"
        << "  presets so the geometry plots remain readable. Passing --preset uses the full preset\n"
        << "  shape instead. Passing --tracks or --zones switches back to a custom display shape.\n\n"
        << "Examples:\n"
        << "  " << executable_name << " --output-dir results\\spatial_events_demo\n"
        << "  " << executable_name << " --tracks 48 --zones 16 --output-dir results\\spatial_events_48x16\n"
        << "  " << executable_name << " --preset tiny --output-dir results\\spatial_events_tiny_preset\n";
}

algobench::spatial_events::SpatialEventShape resolve_shape(const Options& options)
{
    if (options.use_preset_shape)
    {
        return algobench::spatial_events::shape_for_preset(options.preset);
    }

    return {options.track_count, options.zone_count};
}

const char* event_name(algobench::spatial_events::SpatialEventCode code)
{
    using algobench::spatial_events::SpatialEventCode;

    switch (code)
    {
    case SpatialEventCode::None:
        return "none";
    case SpatialEventCode::Enter:
        return "enter";
    case SpatialEventCode::Exit:
        return "exit";
    case SpatialEventCode::StayInside:
        return "stay_inside";
    case SpatialEventCode::CrossThrough:
        return "cross_through";
    }

    return "unknown";
}

struct EventMatrices
{
    std::vector<std::uint8_t> event_codes;
    std::vector<float> scores;
};

EventMatrices evaluate_matrix(const std::vector<algobench::spatial_events::TrackSegment>& tracks,
                              const std::vector<algobench::spatial_events::CircularZone>& zones)
{
    const auto track_count = static_cast<std::int64_t>(tracks.size());
    const auto zone_count = static_cast<std::int64_t>(zones.size());

    EventMatrices matrices;
    matrices.event_codes.resize(static_cast<std::size_t>(track_count * zone_count), 0);
    matrices.scores.resize(static_cast<std::size_t>(track_count * zone_count), 0.0f);

    for (std::int64_t track_index = 0; track_index < track_count; ++track_index)
    {
        for (std::int64_t zone_index = 0; zone_index < zone_count; ++zone_index)
        {
            const auto flat_index = track_index * zone_count + zone_index;
            const auto evaluation = algobench::spatial_events::evaluate_track_zone(
                tracks[static_cast<std::size_t>(track_index)],
                zones[static_cast<std::size_t>(zone_index)]);

            matrices.event_codes[static_cast<std::size_t>(flat_index)] =
                algobench::spatial_events::event_code_value(evaluation.code);
            matrices.scores[static_cast<std::size_t>(flat_index)] = evaluation.score;
        }
    }

    return matrices;
}

void write_tracks_csv(const std::filesystem::path& path,
                      const std::vector<algobench::spatial_events::TrackSegment>& tracks)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open track export: " + path.string());
    }

    file << "track_index,x0,y0,x1,y1,speed\n";
    file << std::setprecision(9);
    for (std::size_t i = 0; i < tracks.size(); ++i)
    {
        const auto& track = tracks[i];
        file << i << ','
             << track.x0 << ','
             << track.y0 << ','
             << track.x1 << ','
             << track.y1 << ','
             << track.speed
             << '\n';
    }
}

void write_zones_csv(const std::filesystem::path& path,
                     const std::vector<algobench::spatial_events::CircularZone>& zones)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open zone export: " + path.string());
    }

    file << "zone_index,x,y,radius,severity\n";
    file << std::setprecision(9);
    for (std::size_t i = 0; i < zones.size(); ++i)
    {
        const auto& zone = zones[i];
        file << i << ','
             << zone.x << ','
             << zone.y << ','
             << zone.radius << ','
             << zone.severity
             << '\n';
    }
}

void write_event_code_matrix_csv(const std::filesystem::path& path,
                                 const std::vector<std::uint8_t>& event_codes,
                                 const algobench::spatial_events::SpatialEventShape& shape)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open event-code matrix export: " + path.string());
    }

    for (std::int64_t track_index = 0; track_index < shape.track_count; ++track_index)
    {
        for (std::int64_t zone_index = 0; zone_index < shape.zone_count; ++zone_index)
        {
            if (zone_index > 0)
            {
                file << ',';
            }

            const auto flat_index = track_index * shape.zone_count + zone_index;
            file << static_cast<int>(event_codes[static_cast<std::size_t>(flat_index)]);
        }
        file << '\n';
    }
}

void write_score_matrix_csv(const std::filesystem::path& path,
                            const std::vector<float>& scores,
                            const algobench::spatial_events::SpatialEventShape& shape)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open score matrix export: " + path.string());
    }

    file << std::setprecision(9);
    for (std::int64_t track_index = 0; track_index < shape.track_count; ++track_index)
    {
        for (std::int64_t zone_index = 0; zone_index < shape.zone_count; ++zone_index)
        {
            if (zone_index > 0)
            {
                file << ',';
            }

            const auto flat_index = track_index * shape.zone_count + zone_index;
            file << scores[static_cast<std::size_t>(flat_index)];
        }
        file << '\n';
    }
}

struct EventCounts
{
    std::int64_t none = 0;
    std::int64_t enter = 0;
    std::int64_t exit = 0;
    std::int64_t stay_inside = 0;
    std::int64_t cross_through = 0;
    std::int64_t triggered = 0;
};

void increment_count(EventCounts& counts, algobench::spatial_events::SpatialEventCode code)
{
    using algobench::spatial_events::SpatialEventCode;

    switch (code)
    {
    case SpatialEventCode::None:
        ++counts.none;
        break;
    case SpatialEventCode::Enter:
        ++counts.enter;
        ++counts.triggered;
        break;
    case SpatialEventCode::Exit:
        ++counts.exit;
        ++counts.triggered;
        break;
    case SpatialEventCode::StayInside:
        ++counts.stay_inside;
        ++counts.triggered;
        break;
    case SpatialEventCode::CrossThrough:
        ++counts.cross_through;
        ++counts.triggered;
        break;
    }
}

EventCounts write_triggered_events_csv(const std::filesystem::path& path,
                                       const std::vector<std::uint8_t>& event_codes,
                                       const std::vector<float>& scores,
                                       const algobench::spatial_events::SpatialEventShape& shape)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open triggered-event export: " + path.string());
    }

    EventCounts counts;
    file << "track_index,zone_index,event_code,event_name,score\n";
    file << std::setprecision(9);

    for (std::int64_t track_index = 0; track_index < shape.track_count; ++track_index)
    {
        for (std::int64_t zone_index = 0; zone_index < shape.zone_count; ++zone_index)
        {
            const auto flat_index = track_index * shape.zone_count + zone_index;
            const auto raw_code = event_codes[static_cast<std::size_t>(flat_index)];
            const auto code = static_cast<algobench::spatial_events::SpatialEventCode>(raw_code);
            const auto score = scores[static_cast<std::size_t>(flat_index)];

            increment_count(counts, code);

            if (code == algobench::spatial_events::SpatialEventCode::None)
            {
                continue;
            }

            file << track_index << ','
                 << zone_index << ','
                 << static_cast<int>(raw_code) << ','
                 << event_name(code) << ','
                 << score
                 << '\n';
        }
    }

    return counts;
}

void write_metadata_json(const std::filesystem::path& path,
                         const Options& options,
                         const algobench::spatial_events::SpatialEventShape& shape,
                         const EventCounts& counts)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open metadata export: " + path.string());
    }

    file << "{\n"
         << "  \"shape_source\": \"" << (options.use_preset_shape ? "preset" : "custom_display_shape") << "\",\n"
         << "  \"preset\": \"" << options.preset << "\",\n"
         << "  \"track_count\": " << shape.track_count << ",\n"
         << "  \"zone_count\": " << shape.zone_count << ",\n"
         << "  \"cell_count\": " << shape.cell_count() << ",\n"
         << "  \"triggered_event_count\": " << counts.triggered << ",\n"
         << "  \"event_counts\": {\n"
         << "    \"none\": " << counts.none << ",\n"
         << "    \"enter\": " << counts.enter << ",\n"
         << "    \"exit\": " << counts.exit << ",\n"
         << "    \"stay_inside\": " << counts.stay_inside << ",\n"
         << "    \"cross_through\": " << counts.cross_through << "\n"
         << "  },\n"
         << "  \"event_code_meanings\": {\n"
         << "    \"0\": \"none\",\n"
         << "    \"1\": \"enter\",\n"
         << "    \"2\": \"exit\",\n"
         << "    \"3\": \"stay_inside\",\n"
         << "    \"4\": \"cross_through\"\n"
         << "  },\n"
         << "  \"layout\": \"row-major: flat_index = track_index * zone_count + zone_index\"\n"
         << "}\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const auto options = parse_options(argc, argv);
        if (options.help)
        {
            print_help(argv[0]);
            return 0;
        }

        const auto shape = resolve_shape(options);
        const auto tracks = algobench::spatial_events::make_track_segments(shape.track_count);
        const auto zones = algobench::spatial_events::make_zones(shape.zone_count);
        const auto matrices = evaluate_matrix(tracks, zones);

        std::filesystem::create_directories(options.output_dir);

        write_tracks_csv(options.output_dir / "tracks.csv", tracks);
        write_zones_csv(options.output_dir / "zones.csv", zones);
        write_event_code_matrix_csv(options.output_dir / "event_codes.csv", matrices.event_codes, shape);
        write_score_matrix_csv(options.output_dir / "scores.csv", matrices.scores, shape);
        const auto counts = write_triggered_events_csv(options.output_dir / "triggered_events.csv",
                                                       matrices.event_codes,
                                                       matrices.scores,
                                                       shape);
        write_metadata_json(options.output_dir / "metadata.json", options, shape, counts);

        std::cout << "Exported spatial-event visualization data to: " << options.output_dir.string() << '\n';
        std::cout << "  tracks: " << shape.track_count << '\n';
        std::cout << "  zones: " << shape.zone_count << '\n';
        std::cout << "  dense cells: " << shape.cell_count() << '\n';
        std::cout << "  triggered event pairs: " << counts.triggered << '\n';
        std::cout << "  files: tracks.csv, zones.csv, event_codes.csv, scores.csv, triggered_events.csv, metadata.json\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "export_spatial_events failed: " << ex.what() << '\n';
        return 1;
    }
}
