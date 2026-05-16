#include "graph/graph_foundation.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr double kPi = 3.141592653589793238462643383279502884;

struct Options
{
    std::filesystem::path output_dir = "results/graph_foundation_demo";

    std::int32_t chain_nodes = 16;
    std::int32_t grid_width = 6;
    std::int32_t grid_height = 5;
    std::int32_t layer_count = 4;
    std::int32_t nodes_per_layer = 6;
    std::int32_t fanout = 2;
    std::int32_t random_nodes = 24;
    std::int32_t random_out_degree = 3;
    std::uint32_t random_seed = 0xC001CAFEu;

    bool help = false;
};

struct NodeLayout
{
    std::int32_t node_id = 0;
    double x = 0.0;
    double y = 0.0;
    std::int64_t out_degree = 0;
    std::int32_t group_index = 0;
    std::int32_t slot_index = 0;
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

std::int32_t parse_positive_int32(const std::string& text, const std::string& option)
{
    std::size_t consumed = 0;
    const long long value = std::stoll(text, &consumed, 10);
    if (consumed != text.size() || value <= 0 || value > 2147483647LL)
    {
        throw std::runtime_error(option + " must be a positive int32 value");
    }

    return static_cast<std::int32_t>(value);
}

std::uint32_t parse_uint32(const std::string& text, const std::string& option)
{
    std::size_t consumed = 0;
    const unsigned long long value = std::stoull(text, &consumed, 0);
    if (consumed != text.size() || value > 0xFFFFFFFFULL)
    {
        throw std::runtime_error(option + " must be an unsigned 32-bit value");
    }

    return static_cast<std::uint32_t>(value);
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
        else if (arg == "--output-dir")
        {
            options.output_dir = require_value(i, argc, argv, arg);
        }
        else if (arg == "--chain-nodes")
        {
            options.chain_nodes = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--grid-width")
        {
            options.grid_width = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--grid-height")
        {
            options.grid_height = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--layers")
        {
            options.layer_count = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--nodes-per-layer")
        {
            options.nodes_per_layer = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--fanout")
        {
            options.fanout = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--random-nodes")
        {
            options.random_nodes = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--random-out-degree")
        {
            options.random_out_degree = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--seed")
        {
            options.random_seed = parse_uint32(require_value(i, argc, argv, arg), arg);
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
        << "Purpose:\n"
        << "  Export small deterministic graph snapshots for the Phase 3 graph\n"
        << "  foundation visualizer. One command writes chain, grid, layered,\n"
        << "  and random sparse demo bundles.\n\n"
        << "Options:\n"
        << "  --output-dir PATH        Root directory for the export bundle\n"
        << "  --chain-nodes N          Chain graph node count. Default: 16\n"
        << "  --grid-width W           Grid graph width. Default: 6\n"
        << "  --grid-height H          Grid graph height. Default: 5\n"
        << "  --layers L               Layered graph layer count. Default: 4\n"
        << "  --nodes-per-layer N      Layered graph nodes per layer. Default: 6\n"
        << "  --fanout F               Layered graph fanout. Default: 2\n"
        << "  --random-nodes N         Random sparse graph node count. Default: 24\n"
        << "  --random-out-degree D    Random sparse out-degree. Default: 3\n"
        << "  --seed VALUE             Random sparse graph seed. Accepts decimal or 0x...\n"
        << "  --help                   Show this help\n\n"
        << "Examples:\n"
        << "  " << executable_name << " --output-dir results\\graph_foundation_demo\n"
        << "  " << executable_name << " --grid-width 8 --grid-height 6 --output-dir results\\graph_foundation_grid_8x6\n";
}

std::vector<NodeLayout> make_chain_layout(const algobench::graph::CsrGraph& graph)
{
    std::vector<NodeLayout> nodes;
    nodes.reserve(static_cast<std::size_t>(graph.node_count));

    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        nodes.push_back({node,
                         static_cast<double>(node),
                         0.0,
                         graph.out_degree(node),
                         0,
                         node});
    }

    return nodes;
}

std::vector<NodeLayout> make_grid_layout(const algobench::graph::CsrGraph& graph,
                                         std::int32_t width,
                                         std::int32_t height)
{
    (void)height;
    std::vector<NodeLayout> nodes;
    nodes.reserve(static_cast<std::size_t>(graph.node_count));

    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const auto x = node % width;
        const auto y = node / width;
        nodes.push_back({node,
                         static_cast<double>(x),
                         -static_cast<double>(y),
                         graph.out_degree(node),
                         y,
                         x});
    }

    return nodes;
}

std::vector<NodeLayout> make_layered_layout(const algobench::graph::CsrGraph& graph,
                                            std::int32_t nodes_per_layer)
{
    std::vector<NodeLayout> nodes;
    nodes.reserve(static_cast<std::size_t>(graph.node_count));

    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const auto layer = node / nodes_per_layer;
        const auto position = node % nodes_per_layer;
        nodes.push_back({node,
                         static_cast<double>(layer) * 2.5,
                         -static_cast<double>(position),
                         graph.out_degree(node),
                         layer,
                         position});
    }

    return nodes;
}

std::vector<NodeLayout> make_random_circle_layout(const algobench::graph::CsrGraph& graph)
{
    std::vector<NodeLayout> nodes;
    nodes.reserve(static_cast<std::size_t>(graph.node_count));

    if (graph.node_count == 0)
    {
        return nodes;
    }

    const double radius = std::max(4.0, static_cast<double>(graph.node_count) / 4.0);
    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const double angle = (2.0 * kPi * static_cast<double>(node)) /
                             static_cast<double>(graph.node_count);
        nodes.push_back({node,
                         radius * std::cos(angle),
                         radius * std::sin(angle),
                         graph.out_degree(node),
                         0,
                         node});
    }

    return nodes;
}

void write_nodes_csv(const std::filesystem::path& path, const std::vector<NodeLayout>& nodes)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open graph node export: " + path.string());
    }

    file << "node_id,x,y,out_degree,group_index,slot_index\n";
    file << std::setprecision(12);
    for (const auto& node : nodes)
    {
        file << node.node_id << ','
             << node.x << ','
             << node.y << ','
             << node.out_degree << ','
             << node.group_index << ','
             << node.slot_index << '\n';
    }
}

void write_edges_csv(const std::filesystem::path& path, const algobench::graph::CsrGraph& graph)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open graph edge export: " + path.string());
    }

    file << "source,target\n";
    for (std::int32_t source = 0; source < graph.node_count; ++source)
    {
        const auto begin = graph.row_offsets[static_cast<std::size_t>(source)];
        const auto end = graph.row_offsets[static_cast<std::size_t>(source + 1)];
        for (std::int64_t edge_index = begin; edge_index < end; ++edge_index)
        {
            file << source << ','
                 << graph.column_indices[static_cast<std::size_t>(edge_index)] << '\n';
        }
    }
}

void write_summary_json(const std::filesystem::path& path,
                        const std::string& graph_type,
                        const std::string& display_name,
                        const std::string& layout_description,
                        const algobench::graph::GraphStats& stats,
                        const std::string& parameter_json)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open graph summary export: " + path.string());
    }

    file << std::setprecision(12);
    file << "{\n"
         << "  \"graph_type\": \"" << graph_type << "\",\n"
         << "  \"display_name\": \"" << display_name << "\",\n"
         << "  \"layout\": \"" << layout_description << "\",\n"
         << "  \"node_count\": " << stats.node_count << ",\n"
         << "  \"directed_adjacency_entry_count\": " << stats.edge_count << ",\n"
         << "  \"min_out_degree\": " << stats.min_out_degree << ",\n"
         << "  \"max_out_degree\": " << stats.max_out_degree << ",\n"
         << "  \"mean_out_degree\": " << stats.mean_out_degree << ",\n"
         << "  \"zero_out_degree_count\": " << stats.zero_out_degree_count << ",\n"
         << "  \"parameters\": " << parameter_json << "\n"
         << "}\n";
}

void export_graph_bundle(const std::filesystem::path& root,
                         const std::string& directory_name,
                         const std::string& graph_type,
                         const std::string& display_name,
                         const std::string& layout_description,
                         const algobench::graph::CsrGraph& graph,
                         const std::vector<NodeLayout>& nodes,
                         const std::string& parameter_json)
{
    const auto directory = root / directory_name;
    std::filesystem::create_directories(directory);

    const auto report = algobench::graph::validate_csr_graph(graph);
    if (!report.valid)
    {
        throw std::runtime_error("refusing to export invalid graph: " + report.message);
    }

    write_nodes_csv(directory / "nodes.csv", nodes);
    write_edges_csv(directory / "edges.csv", graph);
    write_summary_json(directory / "graph_summary.json",
                       graph_type,
                       display_name,
                       layout_description,
                       algobench::graph::summarize_graph(graph),
                       parameter_json);
}

void write_manifest_json(const std::filesystem::path& path)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open graph manifest export: " + path.string());
    }

    file << "{\n"
         << "  \"bundle\": \"graph_foundation_demo\",\n"
         << "  \"graphs\": [\n"
         << "    {\"directory\": \"chain\", \"graph_type\": \"chain\"},\n"
         << "    {\"directory\": \"grid\", \"graph_type\": \"grid\"},\n"
         << "    {\"directory\": \"layered\", \"graph_type\": \"layered\"},\n"
         << "    {\"directory\": \"random_sparse\", \"graph_type\": \"random_sparse\"}\n"
         << "  ]\n"
         << "}\n";
}

std::string bool_json(bool value)
{
    return value ? "true" : "false";
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

        if (options.fanout > options.nodes_per_layer)
        {
            throw std::runtime_error("--fanout cannot exceed --nodes-per-layer");
        }
        if (options.random_out_degree >= options.random_nodes)
        {
            throw std::runtime_error("--random-out-degree must be smaller than --random-nodes");
        }

        std::filesystem::create_directories(options.output_dir);

        const auto chain = algobench::graph::make_chain_graph(options.chain_nodes, true);
        export_graph_bundle(options.output_dir,
                            "chain",
                            "chain",
                            "Bidirectional chain graph",
                            "x=node id, y=0",
                            chain,
                            make_chain_layout(chain),
                            std::string("{\"node_count\": ") + std::to_string(options.chain_nodes) +
                                ", \"bidirectional\": " + bool_json(true) + "}");

        const auto grid = algobench::graph::make_grid_graph(options.grid_width, options.grid_height, true);
        export_graph_bundle(options.output_dir,
                            "grid",
                            "grid",
                            "Bidirectional 4-neighbor grid graph",
                            "x=column, y=-row",
                            grid,
                            make_grid_layout(grid, options.grid_width, options.grid_height),
                            std::string("{\"width\": ") + std::to_string(options.grid_width) +
                                ", \"height\": " + std::to_string(options.grid_height) +
                                ", \"bidirectional\": " + bool_json(true) + "}");

        const auto layered = algobench::graph::make_layered_graph(options.layer_count,
                                                                  options.nodes_per_layer,
                                                                  options.fanout);
        export_graph_bundle(options.output_dir,
                            "layered",
                            "layered",
                            "Directed layered wide-frontier graph",
                            "x=layer, y=-position within layer",
                            layered,
                            make_layered_layout(layered, options.nodes_per_layer),
                            std::string("{\"layer_count\": ") + std::to_string(options.layer_count) +
                                ", \"nodes_per_layer\": " + std::to_string(options.nodes_per_layer) +
                                ", \"fanout\": " + std::to_string(options.fanout) + "}");

        const auto random_sparse = algobench::graph::make_random_sparse_graph(options.random_nodes,
                                                                               options.random_out_degree,
                                                                               options.random_seed);
        export_graph_bundle(options.output_dir,
                            "random_sparse",
                            "random_sparse",
                            "Deterministic directed sparse graph",
                            "deterministic circle layout by node id",
                            random_sparse,
                            make_random_circle_layout(random_sparse),
                            std::string("{\"node_count\": ") + std::to_string(options.random_nodes) +
                                ", \"out_degree\": " + std::to_string(options.random_out_degree) +
                                ", \"seed\": " + std::to_string(options.random_seed) + "}");

        write_manifest_json(options.output_dir / "manifest.json");

        std::cout << "Exported graph foundation visualization bundle to "
                  << options.output_dir.string() << '\n';
        std::cout << "  chain/           nodes.csv, edges.csv, graph_summary.json\n";
        std::cout << "  grid/            nodes.csv, edges.csv, graph_summary.json\n";
        std::cout << "  layered/         nodes.csv, edges.csv, graph_summary.json\n";
        std::cout << "  random_sparse/   nodes.csv, edges.csv, graph_summary.json\n";
        std::cout << "  manifest.json\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "export_graph_foundation error: " << ex.what() << '\n';
        return 1;
    }
}
