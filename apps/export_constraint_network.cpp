#include "constraints/constraint_network.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

struct Options
{
    std::int64_t tasks = 32;
    std::int64_t resources = 16;
    std::int64_t candidates = 512;
    fs::path output_dir = fs::path("results") / "constraint_network_problem_demo";
};

std::int64_t parse_i64(const std::string& value, std::int64_t fallback)
{
    try
    {
        const auto parsed = std::stoll(value);
        return parsed > 0 ? parsed : fallback;
    }
    catch (...)
    {
        return fallback;
    }
}

Options parse_args(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const auto next = [&]() -> std::string {
            if (i + 1 >= argc)
            {
                return {};
            }
            ++i;
            return argv[i];
        };

        if (arg == "--tasks")
        {
            options.tasks = parse_i64(next(), options.tasks);
        }
        else if (arg == "--resources")
        {
            options.resources = parse_i64(next(), options.resources);
        }
        else if (arg == "--candidates")
        {
            options.candidates = parse_i64(next(), options.candidates);
        }
        else if (arg == "--output-dir")
        {
            options.output_dir = next();
        }
        else if (arg == "--help" || arg == "-h")
        {
            std::cout
                << "export_constraint_network\n\n"
                << "Exports a small generated constraint-network problem for visualization.\n\n"
                << "Options:\n"
                << "  --tasks N          Number of tasks, default 32\n"
                << "  --resources N      Number of resources, default 16\n"
                << "  --candidates N     Number of candidate assignments, default 512\n"
                << "  --output-dir DIR   Output directory, default results/constraint_network_problem_demo\n";
            std::exit(0);
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << "\n";
            std::exit(2);
        }
    }
    return options;
}

std::string skill_names(std::uint32_t mask)
{
    std::ostringstream os;
    bool first = true;
    for (int i = 0; i < 8; ++i)
    {
        if ((mask & (1u << static_cast<std::uint32_t>(i))) != 0u)
        {
            if (!first)
            {
                os << "|";
            }
            os << "S" << i;
            first = false;
        }
    }
    if (first)
    {
        os << "none";
    }
    return os.str();
}

std::string csv_escape(const std::string& value)
{
    if (value.find_first_of(",\"\n\r") == std::string::npos)
    {
        return value;
    }
    std::string out = "\"";
    for (const char ch : value)
    {
        if (ch == '"')
        {
            out += "\"\"";
        }
        else
        {
            out += ch;
        }
    }
    out += "\"";
    return out;
}

bool time_windows_overlap(const algobench::constraints::Task& task,
                          const algobench::constraints::Resource& resource)
{
    return std::max(task.earliest_start, resource.available_start) <=
           std::min(task.latest_end, resource.available_end);
}

struct PairCompatibility
{
    bool skill_ok = false;
    bool capacity_ok = false;
    bool time_overlap_ok = false;
    bool distance_ok = false;
    bool zone_ok = false;
    bool risk_ok = false;
    bool pair_ok = false;
};

PairCompatibility pair_compatibility(const algobench::constraints::Task& task,
                                     const algobench::constraints::Resource& resource)
{
    PairCompatibility out;
    out.skill_ok = (resource.skill_mask & task.required_skill_mask) == task.required_skill_mask;
    out.capacity_ok = resource.capacity >= task.required_capacity;
    out.time_overlap_ok = time_windows_overlap(task, resource);
    const float distance2 = algobench::constraints::squared_distance(task.x, task.y, resource.home_x, resource.home_y);
    out.distance_ok = distance2 <= resource.max_distance * resource.max_distance;
    const std::uint32_t zone_bit = 1u << static_cast<std::uint32_t>(task.zone_id & 31);
    out.zone_ok = (resource.forbidden_zone_mask & zone_bit) == 0u;
    out.risk_ok = task.risk <= resource.risk_tolerance;
    out.pair_ok = out.skill_ok && out.capacity_ok && out.time_overlap_ok && out.distance_ok && out.zone_ok && out.risk_ok;
    return out;
}

void write_tasks(const fs::path& path, const algobench::constraints::ConstraintProblem& problem)
{
    std::ofstream out(path);
    out << "task_id,x,y,required_skill_mask,required_skills,required_capacity,earliest_start,latest_end,zone_id,risk\n";
    out << std::fixed << std::setprecision(4);
    for (std::size_t i = 0; i < problem.tasks.size(); ++i)
    {
        const auto& t = problem.tasks[i];
        out << i << ',' << t.x << ',' << t.y << ',' << t.required_skill_mask << ','
            << csv_escape(skill_names(t.required_skill_mask)) << ',' << t.required_capacity << ','
            << t.earliest_start << ',' << t.latest_end << ',' << t.zone_id << ',' << t.risk << '\n';
    }
}

void write_resources(const fs::path& path, const algobench::constraints::ConstraintProblem& problem)
{
    std::ofstream out(path);
    out << "resource_id,home_x,home_y,skill_mask,skills,capacity,available_start,available_end,forbidden_zone_mask,max_distance,risk_tolerance\n";
    out << std::fixed << std::setprecision(4);
    for (std::size_t i = 0; i < problem.resources.size(); ++i)
    {
        const auto& r = problem.resources[i];
        out << i << ',' << r.home_x << ',' << r.home_y << ',' << r.skill_mask << ','
            << csv_escape(skill_names(r.skill_mask)) << ',' << r.capacity << ','
            << r.available_start << ',' << r.available_end << ',' << r.forbidden_zone_mask << ','
            << r.max_distance << ',' << r.risk_tolerance << '\n';
    }
}

void write_pair_compatibility(const fs::path& path, const algobench::constraints::ConstraintProblem& problem)
{
    std::ofstream out(path);
    out << "task_id,resource_id,skill_ok,capacity_ok,time_overlap_ok,distance_ok,zone_ok,risk_ok,pair_ok\n";
    for (std::size_t ti = 0; ti < problem.tasks.size(); ++ti)
    {
        for (std::size_t ri = 0; ri < problem.resources.size(); ++ri)
        {
            const auto compat = pair_compatibility(problem.tasks[ti], problem.resources[ri]);
            out << ti << ',' << ri << ','
                << (compat.skill_ok ? 1 : 0) << ','
                << (compat.capacity_ok ? 1 : 0) << ','
                << (compat.time_overlap_ok ? 1 : 0) << ','
                << (compat.distance_ok ? 1 : 0) << ','
                << (compat.zone_ok ? 1 : 0) << ','
                << (compat.risk_ok ? 1 : 0) << ','
                << (compat.pair_ok ? 1 : 0) << '\n';
        }
    }
}

void write_candidate_evaluations(const fs::path& path, const algobench::constraints::ConstraintProblem& problem)
{
    const auto evaluations = algobench::constraints::evaluate_candidates_cpu_reference(problem);
    std::ofstream out(path);
    out << "candidate_id,task_id,resource_id,start_time,duration,end_time,valid,violation_count,violation_mask,penalty,"
        << "skill_violation,capacity_violation,task_window_violation,resource_window_violation,distance_violation,forbidden_zone_violation,risk_violation\n";
    out << std::fixed << std::setprecision(6);
    for (std::size_t i = 0; i < problem.candidates.size(); ++i)
    {
        const auto& c = problem.candidates[i];
        const auto& e = evaluations[i];
        const std::uint32_t mask = e.violation_mask;
        out << i << ',' << c.task_index << ',' << c.resource_index << ',' << c.start_time << ','
            << c.duration << ',' << (c.start_time + c.duration) << ',' << static_cast<int>(e.valid) << ','
            << e.violation_count << ',' << e.violation_mask << ',' << e.penalty << ','
            << ((mask & algobench::constraints::kViolationSkill) ? 1 : 0) << ','
            << ((mask & algobench::constraints::kViolationCapacity) ? 1 : 0) << ','
            << ((mask & algobench::constraints::kViolationTaskWindow) ? 1 : 0) << ','
            << ((mask & algobench::constraints::kViolationResourceWindow) ? 1 : 0) << ','
            << ((mask & algobench::constraints::kViolationDistance) ? 1 : 0) << ','
            << ((mask & algobench::constraints::kViolationForbiddenZone) ? 1 : 0) << ','
            << ((mask & algobench::constraints::kViolationRisk) ? 1 : 0) << '\n';
    }
}

void write_matrix_csv(const fs::path& path,
                      const algobench::constraints::ConstraintProblem& problem,
                      const char* column_name,
                      bool (*getter)(const PairCompatibility&))
{
    std::ofstream out(path);
    out << "task_id";
    for (std::size_t ri = 0; ri < problem.resources.size(); ++ri)
    {
        out << ",resource_" << ri;
    }
    out << '\n';
    for (std::size_t ti = 0; ti < problem.tasks.size(); ++ti)
    {
        out << ti;
        for (std::size_t ri = 0; ri < problem.resources.size(); ++ri)
        {
            const auto compat = pair_compatibility(problem.tasks[ti], problem.resources[ri]);
            out << ',' << (getter(compat) ? 1 : 0);
        }
        out << '\n';
    }
    (void)column_name;
}

void write_metadata(const fs::path& path, const algobench::constraints::ConstraintProblem& problem)
{
    std::int64_t pair_ok_count = 0;
    for (const auto& task : problem.tasks)
    {
        for (const auto& resource : problem.resources)
        {
            pair_ok_count += pair_compatibility(task, resource).pair_ok ? 1 : 0;
        }
    }
    const auto evaluations = algobench::constraints::evaluate_candidates_cpu_reference(problem);
    std::int64_t valid_candidates = 0;
    for (const auto& e : evaluations)
    {
        valid_candidates += e.valid ? 1 : 0;
    }

    std::ofstream out(path);
    out << "{\n";
    out << "  \"tasks\": " << problem.tasks.size() << ",\n";
    out << "  \"resources\": " << problem.resources.size() << ",\n";
    out << "  \"candidates\": " << problem.candidates.size() << ",\n";
    out << "  \"pair_ok_count\": " << pair_ok_count << ",\n";
    out << "  \"pair_count\": " << (problem.tasks.size() * problem.resources.size()) << ",\n";
    out << "  \"valid_candidate_count\": " << valid_candidates << ",\n";
    out << "  \"candidate_count\": " << evaluations.size() << ",\n";
    out << "  \"description\": \"Synthetic constraint-network definition: task/resource compatibility plus concrete candidate assignments.\"\n";
    out << "}\n";
}

} // namespace

int main(int argc, char** argv)
{
    const auto options = parse_args(argc, argv);
    fs::create_directories(options.output_dir);

    algobench::constraints::ConstraintShape shape;
    shape.task_count = options.tasks;
    shape.resource_count = options.resources;
    shape.candidate_count = options.candidates;
    const auto problem = algobench::constraints::make_constraint_problem(shape);

    write_tasks(options.output_dir / "tasks.csv", problem);
    write_resources(options.output_dir / "resources.csv", problem);
    write_pair_compatibility(options.output_dir / "pair_compatibility.csv", problem);
    write_candidate_evaluations(options.output_dir / "candidate_evaluations.csv", problem);
    write_matrix_csv(options.output_dir / "skill_compatibility_matrix.csv", problem, "skill_ok", [](const PairCompatibility& c) { return c.skill_ok; });
    write_matrix_csv(options.output_dir / "pair_feasibility_matrix.csv", problem, "pair_ok", [](const PairCompatibility& c) { return c.pair_ok; });
    write_metadata(options.output_dir / "metadata.json", problem);

    std::cout << "Exported constraint-network problem definition to: " << options.output_dir << '\n';
    std::cout << "  tasks.csv\n";
    std::cout << "  resources.csv\n";
    std::cout << "  pair_compatibility.csv\n";
    std::cout << "  candidate_evaluations.csv\n";
    std::cout << "  skill_compatibility_matrix.csv\n";
    std::cout << "  pair_feasibility_matrix.csv\n";
    std::cout << "  metadata.json\n";
    return 0;
}
