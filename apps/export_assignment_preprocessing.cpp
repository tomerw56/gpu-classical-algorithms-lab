#include "assignment/assignment_preprocessing.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;
using namespace algobench::assignment;

namespace
{
std::string arg_value(int argc, char** argv, const std::string& key, const std::string& fallback)
{
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (argv[i] == key)
        {
            return argv[i + 1];
        }
    }
    return fallback;
}

int int_arg(int argc, char** argv, const std::string& key, int fallback)
{
    return std::stoi(arg_value(argc, argv, key, std::to_string(fallback)));
}

void write_tasks(const fs::path& path, const AssignmentProblem& problem)
{
    std::ofstream out(path);
    out << "task_id,x,y,required_skill_mask,demand,earliest,latest,duration,zone_id,priority,risk\n";
    for (std::size_t i = 0; i < problem.tasks.size(); ++i)
    {
        const auto& t = problem.tasks[i];
        out << i << ',' << t.x << ',' << t.y << ',' << t.required_skill_mask << ',' << t.demand << ','
            << t.earliest << ',' << t.latest << ',' << t.duration << ',' << t.zone_id << ',' << t.priority << ',' << t.risk << '\n';
    }
}

void write_resources(const fs::path& path, const AssignmentProblem& problem)
{
    std::ofstream out(path);
    out << "resource_id,x,y,skill_mask,capacity,available_start,available_end,forbidden_zone_mask,max_distance,risk_tolerance,load\n";
    for (std::size_t i = 0; i < problem.resources.size(); ++i)
    {
        const auto& r = problem.resources[i];
        out << i << ',' << r.x << ',' << r.y << ',' << r.skill_mask << ',' << r.capacity << ','
            << r.available_start << ',' << r.available_end << ',' << r.forbidden_zone_mask << ','
            << r.max_distance << ',' << r.risk_tolerance << ',' << r.load << '\n';
    }
}

void write_pair_matrix(const fs::path& feasibility, const fs::path& costs, const AssignmentProblem& problem)
{
    std::ofstream feasible_out(feasibility);
    std::ofstream cost_out(costs);
    feasible_out << "task_id";
    cost_out << "task_id";
    for (std::int32_t r = 0; r < problem.shape.resource_count; ++r)
    {
        feasible_out << ",r" << r;
        cost_out << ",r" << r;
    }
    feasible_out << '\n';
    cost_out << '\n';

    for (std::int32_t t = 0; t < problem.shape.task_count; ++t)
    {
        feasible_out << t;
        cost_out << t;
        for (std::int32_t r = 0; r < problem.shape.resource_count; ++r)
        {
            const auto eval = evaluate_assignment_pair(problem.tasks[static_cast<std::size_t>(t)],
                                                       problem.resources[static_cast<std::size_t>(r)]);
            feasible_out << ',' << static_cast<int>(eval.feasible);
            cost_out << ',' << (eval.feasible ? eval.cost : -1.0f);
        }
        feasible_out << '\n';
        cost_out << '\n';
    }
}

void write_topk(const fs::path& path, const AssignmentProblem& problem, const std::vector<TopKEntry>& topk)
{
    std::ofstream out(path);
    out << "task_id,rank,resource_id,cost\n";
    for (std::int32_t t = 0; t < problem.shape.task_count; ++t)
    {
        for (std::int32_t k = 0; k < problem.shape.top_k; ++k)
        {
            const auto& entry = topk[static_cast<std::size_t>(t * problem.shape.top_k + k)];
            out << t << ',' << k << ',' << entry.resource_index << ',' << entry.cost << '\n';
        }
    }
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        AssignmentShape shape{};
        shape.task_count = int_arg(argc, argv, "--tasks", 32);
        shape.resource_count = int_arg(argc, argv, "--resources", 24);
        shape.top_k = int_arg(argc, argv, "--top-k", 4);
        const fs::path output_dir = arg_value(argc, argv, "--output-dir", "results/assignment_preprocessing_demo");
        fs::create_directories(output_dir);

        const auto problem = make_assignment_problem(shape);
        const auto topk = compute_assignment_topk_cpu_reference(problem);
        const auto aggregate = aggregate_assignment_solution(problem, topk);

        write_tasks(output_dir / "tasks.csv", problem);
        write_resources(output_dir / "resources.csv", problem);
        write_pair_matrix(output_dir / "pair_feasibility_matrix.csv", output_dir / "pair_cost_matrix.csv", problem);
        write_topk(output_dir / "topk_candidates.csv", problem, topk);

        std::ofstream meta(output_dir / "metadata.json");
        meta << "{\n"
             << "  \"tasks\": " << shape.task_count << ",\n"
             << "  \"resources\": " << shape.resource_count << ",\n"
             << "  \"top_k\": " << shape.top_k << ",\n"
             << "  \"feasible_pair_count\": " << aggregate.feasible_pair_count << ",\n"
             << "  \"selected_candidate_count\": " << aggregate.selected_candidate_count << "\n"
             << "}\n";

        std::cout << "Exported assignment preprocessing problem to: " << output_dir.string() << '\n';
        return 0;
    }
    catch (const std::exception& exc)
    {
        std::cerr << "export_assignment_preprocessing failed: " << exc.what() << '\n';
        return 1;
    }
}
