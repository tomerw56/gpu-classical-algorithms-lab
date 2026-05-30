#include "local_search/local_search_moves.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace algobench;

namespace
{
std::string arg_value(int argc, char** argv, const std::string& name, const std::string& fallback)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (argv[i] == name)
        {
            return argv[i + 1];
        }
    }
    return fallback;
}

int int_arg(int argc, char** argv, const std::string& name, int fallback)
{
    return std::stoi(arg_value(argc, argv, name, std::to_string(fallback)));
}

long long int64_arg(int argc, char** argv, const std::string& name, long long fallback)
{
    return std::stoll(arg_value(argc, argv, name, std::to_string(fallback)));
}
}

int main(int argc, char** argv)
{
    try
    {
        const auto output_dir = std::filesystem::path(arg_value(argc, argv, "--output-dir", "results/local_search_moves_demo"));
        const local_search::LocalSearchShape shape{
            int_arg(argc, argv, "--tasks", 32),
            int_arg(argc, argv, "--resources", 24),
            int64_arg(argc, argv, "--moves", 512)};
        std::filesystem::create_directories(output_dir);
        const auto problem = local_search::make_local_search_problem(shape);
        const auto evaluations = local_search::evaluate_moves_cpu_reference(problem);
        const auto aggregate = local_search::aggregate_move_evaluations(evaluations);

        {
            std::ofstream f(output_dir / "tasks.csv");
            f << "task_id,x,y,required_skill_mask,demand,priority,risk,current_resource\n";
            for (std::int32_t i = 0; i < shape.task_count; ++i)
            {
                const auto& t = problem.tasks[static_cast<std::size_t>(i)];
                f << i << ',' << t.x << ',' << t.y << ',' << t.required_skill_mask << ',' << t.demand << ','
                  << t.priority << ',' << t.risk << ',' << problem.current_assignment[static_cast<std::size_t>(i)] << "\n";
            }
        }
        {
            std::ofstream f(output_dir / "resources.csv");
            f << "resource_id,x,y,skill_mask,capacity,max_distance,risk_tolerance,load\n";
            for (std::int32_t i = 0; i < shape.resource_count; ++i)
            {
                const auto& r = problem.resources[static_cast<std::size_t>(i)];
                f << i << ',' << r.x << ',' << r.y << ',' << r.skill_mask << ',' << r.capacity << ','
                  << r.max_distance << ',' << r.risk_tolerance << ',' << r.load << "\n";
            }
        }
        {
            std::ofstream f(output_dir / "candidate_moves.csv");
            f << "move_id,task_id,old_resource,new_resource,valid,improving,violation_mask,old_cost,new_cost,delta\n";
            for (std::size_t i = 0; i < problem.moves.size(); ++i)
            {
                const auto& move = problem.moves[i];
                const auto& eval = evaluations[i];
                const auto old_resource = problem.current_assignment[static_cast<std::size_t>(move.task_index)];
                f << i << ',' << move.task_index << ',' << old_resource << ',' << move.new_resource_index << ','
                  << static_cast<int>(eval.valid) << ',' << static_cast<int>(eval.improving) << ',' << eval.violation_mask << ','
                  << eval.old_cost << ',' << eval.new_cost << ',' << eval.delta << "\n";
            }
        }
        {
            std::ofstream f(output_dir / "metadata.json");
            f << "{\n";
            f << "  \"tasks\": " << shape.task_count << ",\n";
            f << "  \"resources\": " << shape.resource_count << ",\n";
            f << "  \"moves\": " << shape.move_count << ",\n";
            f << "  \"valid_moves\": " << aggregate.valid_moves << ",\n";
            f << "  \"improving_moves\": " << aggregate.improving_moves << ",\n";
            f << "  \"best_move_index\": " << aggregate.best_move_index << ",\n";
            f << "  \"best_delta\": " << aggregate.best_delta << "\n";
            f << "}\n";
        }
        std::cout << "Exported local-search move problem to: " << output_dir << "\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "export_local_search_moves failed: " << ex.what() << "\n";
        return 1;
    }
}
