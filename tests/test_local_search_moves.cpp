#include "local_search/local_search_moves.hpp"
#include "test_support.hpp"

#include <cmath>

using namespace algobench;

int main()
{
    local_search::LocalSearchShape shape{16, 24, 128};
    const auto problem = local_search::make_local_search_problem(shape);
    TEST_CHECK_EQ(static_cast<int>(problem.tasks.size()), 16);
    TEST_CHECK_EQ(static_cast<int>(problem.resources.size()), 24);
    TEST_CHECK_EQ(static_cast<int>(problem.moves.size()), 128);

    const auto evaluations = local_search::evaluate_moves_cpu_reference(problem);
    TEST_CHECK_EQ(static_cast<int>(evaluations.size()), 128);
    const auto aggregate = local_search::aggregate_move_evaluations(evaluations);
    TEST_CHECK_EQ(aggregate.valid_moves + aggregate.invalid_moves, 128);
    TEST_CHECK(aggregate.delta_checksum != 0.0);

    const auto validation = local_search::validate_move_evaluations(evaluations, evaluations);
    TEST_CHECK(validation.correct);
    TEST_CHECK_EQ(validation.valid_mismatches, 0);
    TEST_CHECK_EQ(validation.improving_mismatches, 0);

    BenchmarkConfig config{};
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;
    config.params["tasks"] = "16";
    config.params["resources"] = "24";
    config.params["moves"] = "128";
    config.params["sweep_label"] = "test_local_search";
    const auto results = local_search::run_local_search_moves_cpu(config);
    TEST_CHECK_EQ(static_cast<int>(results.size()), 1);
    TEST_CHECK_EQ(results[0].benchmark, std::string("local_search_moves"));
    TEST_CHECK_EQ(results[0].preset, std::string("test_local_search"));
    TEST_CHECK(results[0].correct);
    return 0;
}
