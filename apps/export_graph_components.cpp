#include "graph/graph_connected_components.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

constexpr double kPi = 3.141592653589793238462643383279502884;

struct Options
{
    std::filesystem::path output_dir = "results/graph_components_demo";
    std::string graph_kind = "random_clusters";
    std::string preset = "tiny";
    std::int32_t components = 6;
    std::int32_t component_size = 24;
    std::int32_t grid_width = 6;
    std::int32_t grid_height = 5;
    std::int32_t random_out_degree = 3;
    std::uint32_t seed = 0xC0FFEEu;
    bool help = false;
};

struct NodeLayout
{
    std::int32_t node_id = 0;
    double x = 0.0;
    double y = 0.0;
    std::int32_t component_label = 0;
    std::int32_t component_index = 0;
    std::int64_t component_size = 0;
    std::int64_t out_degree = 0;
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
        else if (arg == "--graph")
        {
            options.graph_kind = require_value(i, argc, argv, arg);
        }
        else if (arg == "--preset")
        {
            options.preset = require_value(i, argc, argv, arg);
        }
        else if (arg == "--components")
        {
            options.components = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--component-size")
        {
            options.component_size = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--grid-width")
        {
            options.grid_width = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--grid-height")
        {
            options.grid_height = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--random-out-degree")
        {
            options.random_out_degree = parse_positive_int32(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--seed")
        {
            options.seed = parse_uint32(require_value(i, argc, argv, arg), arg);
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
        << "Export a small connected-components graph for plotting.\n\n"
        << "Usage:\n"
        << "  " << executable_name << " [options]\n\n"
        << "Options:\n"
        << "  --output-dir DIR             Output directory, default results\\graph_components_demo\n"
        << "  --graph KIND                 chains, grid_components, random_clusters, or mixed\n"
        << "  --preset NAME                tiny/small/medium/large base preset, default tiny\n"
        << "  --components N               Component count for visualization\n"
        << "  --component-size N           Nodes per non-grid component\n"
        << "  --grid-width N               Width per grid component\n"
        << "  --grid-height N              Height per grid component\n"
        << "  --random-out-degree N        Extra random links per node inside random clusters\n"
        << "  --seed N                     Random-cluster seed\n"
        << "  --help                       Show this help\n\n"
        << "Examples:\n"
        << "  " << executable_name << " --graph random_clusters --components 6 --component-size 24 --output-dir results\\graph_components_demo\n"
        << "  " << executable_name << " --graph grid_components --components 4 --grid-width 6 --grid-height 5 --output-dir results\\graph_components_grid\n";
}

algobench::BenchmarkConfig make_config(const Options& options)
{
    algobench::BenchmarkConfig config;
    config.preset = options.preset;
    config.repeat = 1;
    config.warmup = 0;
    config.include_gpu = false;
    config.params["graph"] = options.graph_kind;
    config.params["components"] = std::to_string(options.components);
    config.params["component_size"] = std::to_string(options.component_size);
    config.params["grid_width"] = std::to_string(options.grid_width);
    config.params["grid_height"] = std::to_string(options.grid_height);
    config.params["random_out_degree"] = std::to_string(options.random_out_degree);
    config.params["seed"] = std::to_string(options.seed);
    return config;
}

std::map<std::int32_t, std::int32_t> component_index_by_label(const std::vector<std::int32_t>& labels)
{
    std::map<std::int32_t, std::int32_t> index_by_label;
    for (const auto label : labels)
    {
        if (index_by_label.find(label) == index_by_label.end())
        {
            index_by_label[label] = static_cast<std::int32_t>(index_by_label.size());
        }
    }
    return index_by_label;
}

std::map<std::int32_t, std::int64_t> component_sizes(const std::vector<std::int32_t>& labels)
{
    std::map<std::int32_t, std::int64_t> sizes;
    for (const auto label : labels)
    {
        ++sizes[label];
    }
    return sizes;
}

std::vector<NodeLayout> make_layout(const algobench::graph_cc::GraphCcShape& shape,
                                    const algobench::graph::CsrGraph& graph,
                                    const std::vector<std::int32_t>& labels)
{
    const auto index_by_label = component_index_by_label(labels);
    const auto sizes = component_sizes(labels);
    std::vector<NodeLayout> nodes;
    nodes.reserve(static_cast<std::size_t>(graph.node_count));

    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const auto label = labels[static_cast<std::size_t>(node)];
        const auto component_index = index_by_label.at(label);
        const auto component_size = sizes.at(label);
        double x = 0.0;
        double y = 0.0;

        if (shape.graph_kind == "grid_components" || shape.graph_kind == "grids")
        {
            const auto local = node - label;
            x = static_cast<double>(component_index * (shape.grid_width + 3) + (local % shape.grid_width));
            y = -static_cast<double>(local / shape.grid_width);
        }
        else if (shape.graph_kind == "chains" || shape.graph_kind == "chain_components")
        {
            const auto local = node - label;
            x = static_cast<double>(local);
            y = -static_cast<double>(component_index * 2);
        }
        else
        {
            const auto local = static_cast<std::int32_t>(node - label);
            const double radius = std::max(1.2, std::sqrt(static_cast<double>(component_size)) * 0.42);
            const double angle = (2.0 * kPi * static_cast<double>(local)) / static_cast<double>(component_size);
            x = static_cast<double>(component_index) * (radius * 3.2 + 3.0) + radius * std::cos(angle);
            y = radius * std::sin(angle);
        }

        nodes.push_back({node, x, y, label, component_index, component_size, graph.out_degree(node)});
    }
    return nodes;
}

void write_nodes_csv(const std::filesystem::path& path, const std::vector<NodeLayout>& nodes)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open " + path.string());
    }
    file << "node_id,x,y,component_label,component_index,component_size,out_degree\n";
    file << std::fixed << std::setprecision(6);
    for (const auto& node : nodes)
    {
        file << node.node_id << ',' << node.x << ',' << node.y << ',' << node.component_label << ','
             << node.component_index << ',' << node.component_size << ',' << node.out_degree << '\n';
    }
}

void write_edges_csv(const std::filesystem::path& path, const algobench::graph::CsrGraph& graph)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open " + path.string());
    }
    file << "source,target\n";
    for (std::int32_t node = 0; node < graph.node_count; ++node)
    {
        const auto begin = graph.row_offsets[static_cast<std::size_t>(node)];
        const auto end = graph.row_offsets[static_cast<std::size_t>(node + 1)];
        for (auto edge = begin; edge < end; ++edge)
        {
            file << node << ',' << graph.column_indices[static_cast<std::size_t>(edge)] << '\n';
        }
    }
}

void write_components_csv(const std::filesystem::path& path, const std::map<std::int32_t, std::int64_t>& sizes)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open " + path.string());
    }
    file << "component_label,component_index,size\n";
    std::int32_t index = 0;
    for (const auto& [label, size] : sizes)
    {
        file << label << ',' << index++ << ',' << size << '\n';
    }
}

void write_metadata_json(const std::filesystem::path& path,
                         const Options& options,
                         const algobench::graph_cc::GraphCcShape& shape,
                         const algobench::graph::CsrGraph& graph,
                         const algobench::graph_cc::ConnectedComponentsSummary& summary)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open " + path.string());
    }
    file << "{\n"
         << "  \"graph_kind\": \"" << shape.graph_kind << "\",\n"
         << "  \"shape\": \"" << algobench::graph_cc::describe_cc_shape(shape) << "\",\n"
         << "  \"node_count\": " << graph.node_count << ",\n"
         << "  \"directed_adjacency_entry_count\": " << graph.edge_count() << ",\n"
         << "  \"component_count\": " << summary.component_count << ",\n"
         << "  \"largest_component_size\": " << summary.largest_component_size << ",\n"
         << "  \"checksum\": " << summary.checksum << ",\n"
         << "  \"output_dir\": \"" << options.output_dir.generic_string() << "\"\n"
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

        const auto config = make_config(options);
        const auto shape = algobench::graph_cc::cc_shape_for_config(config);
        const auto graph = algobench::graph_cc::make_cc_graph_for_shape(shape);
        const auto labels = algobench::graph_cc::connected_components_cpu_reference(graph);
        const auto summary = algobench::graph_cc::validate_component_labels(graph, labels);
        const auto nodes = make_layout(shape, graph, labels);
        const auto sizes = component_sizes(labels);

        std::filesystem::create_directories(options.output_dir);
        write_nodes_csv(options.output_dir / "nodes.csv", nodes);
        write_edges_csv(options.output_dir / "edges.csv", graph);
        write_components_csv(options.output_dir / "components.csv", sizes);
        write_metadata_json(options.output_dir / "metadata.json", options, shape, graph, summary);

        std::cout << "Exported connected-components graph to " << options.output_dir << '\n'
                  << "  nodes:      " << graph.node_count << '\n'
                  << "  edges:      " << graph.edge_count() << '\n'
                  << "  components: " << summary.component_count << '\n';
    }
    catch (const std::exception& ex)
    {
        std::cerr << "export_graph_components failed: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
