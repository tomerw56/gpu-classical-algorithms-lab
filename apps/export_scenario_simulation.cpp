#include "scenario/scenario_simulation.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace algobench::scenario;

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

long long int64_arg(int argc, char** argv, const std::string& key, long long fallback)
{
    return std::stoll(arg_value(argc, argv, key, std::to_string(fallback)));
}

void write_tasks(const ScenarioProblem& problem, const std::filesystem::path& path)
{
    std::ofstream file(path);
    file << "task_id,x,y,demand,base_cost,priority,risk,latest_finish,assigned_resource\n";
    for (std::size_t i = 0; i < problem.tasks.size(); ++i)
    {
        const auto& t = problem.tasks[i];
        file << i << ',' << t.x << ',' << t.y << ',' << t.demand << ',' << t.base_cost << ','
             << t.priority << ',' << t.risk << ',' << t.latest_finish << ',' << problem.assignment[i] << '\n';
    }
}

void write_resources(const ScenarioProblem& problem, const std::filesystem::path& path)
{
    std::ofstream file(path);
    file << "resource_id,x,y,capacity,speed,reliability,risk_tolerance,fixed_cost\n";
    for (std::size_t i = 0; i < problem.resources.size(); ++i)
    {
        const auto& r = problem.resources[i];
        file << i << ',' << r.x << ',' << r.y << ',' << r.capacity << ',' << r.speed << ','
             << r.reliability << ',' << r.risk_tolerance << ',' << r.fixed_cost << '\n';
    }
}

void write_scenarios(const ScenarioProblem& problem, const std::vector<ScenarioEvaluation>& evals, const std::filesystem::path& path)
{
    std::ofstream file(path);
    file << "scenario_id,failed_resource,demand_spike_task,delay_multiplier,cost_multiplier,risk_multiplier,weather_penalty,feasible,violation_mask,violation_count,failed_tasks,capacity_violations,risk_violations,delay_violations,score\n";
    for (std::size_t i = 0; i < problem.scenarios.size(); ++i)
    {
        const auto& s = problem.scenarios[i];
        const auto& e = evals[i];
        file << i << ',' << s.failed_resource << ',' << s.demand_spike_task << ',' << s.delay_multiplier << ','
             << s.cost_multiplier << ',' << s.risk_multiplier << ',' << s.weather_penalty << ','
             << static_cast<int>(e.feasible) << ',' << e.violation_mask << ',' << e.violation_count << ','
             << e.failed_task_count << ',' << e.capacity_violation_count << ',' << e.risk_violation_count << ','
             << e.delay_violation_count << ',' << e.score << '\n';
    }
}

void write_metadata(const ScenarioProblem& problem, const ScenarioAggregate& aggregate, const std::filesystem::path& path)
{
    std::ofstream file(path);
    file << "{\n";
    file << "  \"task_count\": " << problem.shape.task_count << ",\n";
    file << "  \"resource_count\": " << problem.shape.resource_count << ",\n";
    file << "  \"scenario_count\": " << problem.shape.scenario_count << ",\n";
    file << "  \"feasible_scenarios\": " << aggregate.feasible_scenarios << ",\n";
    file << "  \"infeasible_scenarios\": " << aggregate.infeasible_scenarios << ",\n";
    file << "  \"mean_score\": " << aggregate.mean_score << ",\n";
    file << "  \"best_score\": " << aggregate.best_score << ",\n";
    file << "  \"worst_score\": " << aggregate.worst_score << ",\n";
    file << "  \"robustness_score\": " << aggregate.robustness_score << "\n";
    file << "}\n";
}
}

int main(int argc, char** argv)
{
    try
    {
        const int tasks = int_arg(argc, argv, "--tasks", 32);
        const int resources = int_arg(argc, argv, "--resources", 16);
        const long long scenarios = int64_arg(argc, argv, "--scenarios", 1024);
        const std::filesystem::path output_dir = arg_value(argc, argv, "--output-dir", "results/scenario_simulation_problem_demo");

        std::filesystem::create_directories(output_dir);
        const ScenarioShape shape{tasks, resources, scenarios};
        const auto problem = make_scenario_problem(shape);
        const auto evals = evaluate_scenarios_cpu_reference(problem);
        const auto aggregate = aggregate_scenario_evaluations(evals);

        write_tasks(problem, output_dir / "tasks.csv");
        write_resources(problem, output_dir / "resources.csv");
        write_scenarios(problem, evals, output_dir / "scenarios.csv");
        write_metadata(problem, aggregate, output_dir / "metadata.json");

        std::cout << "Exported scenario simulation problem to " << output_dir << "\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "ERROR: " << ex.what() << "\n";
        return 1;
    }
}
