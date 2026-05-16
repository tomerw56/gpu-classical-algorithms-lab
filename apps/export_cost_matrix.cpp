#include "cost_matrix/cost_matrix.hpp"

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
    std::filesystem::path output_dir = "results/cost_matrix_visualization";
    std::int64_t task_count = -1;
    std::int64_t resource_count = -1;
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
        }
        else if (arg == "--tasks")
        {
            options.task_count = parse_positive_int64(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--resources")
        {
            options.resource_count = parse_positive_int64(require_value(i, argc, argv, arg), arg);
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
        << "  --preset NAME        tiny, small, medium, large. Default: tiny\n"
        << "  --tasks N            Override the preset task count\n"
        << "  --resources N        Override the preset resource count\n"
        << "  --output-dir PATH    Directory for CSV and metadata export\n"
        << "  --help               Show this help\n\n"
        << "Examples:\n"
        << "  " << executable_name << " --preset tiny --output-dir results\\cost_matrix_tiny\n"
        << "  " << executable_name << " --tasks 96 --resources 128 --output-dir results\\cost_matrix_96x128\n";
}

algobench::cost_matrix::CostMatrixShape resolve_shape(const Options& options)
{
    auto shape = algobench::cost_matrix::shape_for_preset(options.preset);

    if (options.task_count > 0)
    {
        shape.task_count = options.task_count;
    }
    if (options.resource_count > 0)
    {
        shape.resource_count = options.resource_count;
    }

    return shape;
}

void evaluate_matrix(std::vector<float>& costs,
                     std::vector<std::uint8_t>& feasible,
                     const std::vector<algobench::cost_matrix::Task>& tasks,
                     const std::vector<algobench::cost_matrix::Resource>& resources)
{
    const auto resource_count = static_cast<std::int64_t>(resources.size());

    for (std::int64_t task_index = 0; task_index < static_cast<std::int64_t>(tasks.size()); ++task_index)
    {
        for (std::int64_t resource_index = 0; resource_index < resource_count; ++resource_index)
        {
            const auto flat_index = task_index * resource_count + resource_index;
            const auto evaluation = algobench::cost_matrix::evaluate_pair(
                tasks[static_cast<std::size_t>(task_index)],
                resources[static_cast<std::size_t>(resource_index)]);

            costs[static_cast<std::size_t>(flat_index)] = evaluation.cost;
            feasible[static_cast<std::size_t>(flat_index)] = evaluation.feasible;
        }
    }
}

void write_cost_matrix_csv(const std::filesystem::path& path,
                           const std::vector<float>& costs,
                           const algobench::cost_matrix::CostMatrixShape& shape)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open cost matrix export: " + path.string());
    }

    file << std::setprecision(9);
    for (std::int64_t task_index = 0; task_index < shape.task_count; ++task_index)
    {
        for (std::int64_t resource_index = 0; resource_index < shape.resource_count; ++resource_index)
        {
            if (resource_index > 0)
            {
                file << ',';
            }

            const auto flat_index = task_index * shape.resource_count + resource_index;
            file << costs[static_cast<std::size_t>(flat_index)];
        }
        file << '\n';
    }
}

void write_feasible_matrix_csv(const std::filesystem::path& path,
                               const std::vector<std::uint8_t>& feasible,
                               const algobench::cost_matrix::CostMatrixShape& shape)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open feasibility matrix export: " + path.string());
    }

    for (std::int64_t task_index = 0; task_index < shape.task_count; ++task_index)
    {
        for (std::int64_t resource_index = 0; resource_index < shape.resource_count; ++resource_index)
        {
            if (resource_index > 0)
            {
                file << ',';
            }

            const auto flat_index = task_index * shape.resource_count + resource_index;
            file << static_cast<int>(feasible[static_cast<std::size_t>(flat_index)]);
        }
        file << '\n';
    }
}

void write_tasks_csv(const std::filesystem::path& path,
                     const std::vector<algobench::cost_matrix::Task>& tasks)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open task export: " + path.string());
    }

    file << "task_index,x,y,deadline,priority,required_skill,urgent\n";
    file << std::setprecision(9);
    for (std::size_t i = 0; i < tasks.size(); ++i)
    {
        const auto& task = tasks[i];
        file << i << ','
             << task.x << ','
             << task.y << ','
             << task.deadline << ','
             << task.priority << ','
             << task.required_skill << ','
             << static_cast<int>(task.urgent)
             << '\n';
    }
}

void write_resources_csv(const std::filesystem::path& path,
                         const std::vector<algobench::cost_matrix::Resource>& resources)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open resource export: " + path.string());
    }

    file << "resource_index,x,y,load,speed,available_at,skill_mask\n";
    file << std::setprecision(9);
    for (std::size_t i = 0; i < resources.size(); ++i)
    {
        const auto& resource = resources[i];
        file << i << ','
             << resource.x << ','
             << resource.y << ','
             << resource.load << ','
             << resource.speed << ','
             << resource.available_at << ','
             << resource.skill_mask
             << '\n';
    }
}

void write_metadata_json(const std::filesystem::path& path,
                         const Options& options,
                         const algobench::cost_matrix::CostMatrixShape& shape,
                         std::int64_t feasible_count)
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("failed to open metadata export: " + path.string());
    }

    file << "{\n"
         << "  \"preset\": \"" << options.preset << "\",\n"
         << "  \"task_count\": " << shape.task_count << ",\n"
         << "  \"resource_count\": " << shape.resource_count << ",\n"
         << "  \"cell_count\": " << shape.cell_count() << ",\n"
         << "  \"feasible_count\": " << feasible_count << ",\n"
         << "  \"infeasible_cost\": " << algobench::cost_matrix::kInfeasibleCost << ",\n"
         << "  \"layout\": \"row-major: flat_index = task_index * resource_count + resource_index\"\n"
         << "}\n";
}

std::int64_t count_feasible(const std::vector<std::uint8_t>& feasible)
{
    std::int64_t count = 0;
    for (const auto value : feasible)
    {
        if (value != 0)
        {
            ++count;
        }
    }
    return count;
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
        const auto tasks = algobench::cost_matrix::make_tasks(shape.task_count);
        const auto resources = algobench::cost_matrix::make_resources(shape.resource_count);

        std::vector<float> costs(static_cast<std::size_t>(shape.cell_count()), algobench::cost_matrix::kInfeasibleCost);
        std::vector<std::uint8_t> feasible(static_cast<std::size_t>(shape.cell_count()), 0);
        evaluate_matrix(costs, feasible, tasks, resources);

        const auto feasible_count = count_feasible(feasible);
        std::filesystem::create_directories(options.output_dir);

        write_cost_matrix_csv(options.output_dir / "costs.csv", costs, shape);
        write_feasible_matrix_csv(options.output_dir / "feasible.csv", feasible, shape);
        write_tasks_csv(options.output_dir / "tasks.csv", tasks);
        write_resources_csv(options.output_dir / "resources.csv", resources);
        write_metadata_json(options.output_dir / "metadata.json", options, shape, feasible_count);

        std::cout << "Exported cost-matrix visualization data to: " << options.output_dir.string() << '\n';
        std::cout << "  tasks:     " << shape.task_count << '\n';
        std::cout << "  resources: " << shape.resource_count << '\n';
        std::cout << "  cells:     " << shape.cell_count() << '\n';
        std::cout << "  feasible:  " << feasible_count << '\n';
        std::cout << "  files:     costs.csv, feasible.csv, tasks.csv, resources.csv, metadata.json\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << '\n';
        print_help(argv[0]);
        return 1;
    }
}
