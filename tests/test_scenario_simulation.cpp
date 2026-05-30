#include "scenario/scenario_simulation.hpp"

#include <cassert>
#include <iostream>

using namespace algobench;
using namespace algobench::scenario;

int main()
{
    const ScenarioShape shape{8, 4, 64};
    const auto problem = make_scenario_problem(shape);
    assert(static_cast<int>(problem.tasks.size()) == shape.task_count);
    assert(static_cast<int>(problem.resources.size()) == shape.resource_count);
    assert(static_cast<long long>(problem.scenarios.size()) == shape.scenario_count);

    const auto evals = evaluate_scenarios_cpu_reference(problem);
    const auto validation = validate_scenario_evaluations(evals, evals);
    assert(validation.correct);

    const auto aggregate = aggregate_scenario_evaluations(evals);
    assert(aggregate.feasible_scenarios + aggregate.infeasible_scenarios == shape.scenario_count);
    assert(aggregate.worst_score >= aggregate.best_score);

    BenchmarkConfig config{};
    config.benchmark = "scenario_simulation";
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;
    config.params["tasks"] = "8";
    config.params["resources"] = "4";
    config.params["scenarios"] = "64";
    auto results = run_scenario_simulation_cpu(config);
    assert(results.size() == 1);
    assert(results[0].benchmark == "scenario_simulation");
    assert(results[0].correct);
    assert(results[0].metadata.count("robustness_score") == 1);

    std::cout << "test_scenario_simulation passed\n";
    return 0;
}
