#include "combinations/combination_finder.hpp"
#include "test_support.hpp"

using namespace algobench;
using namespace algobench::combinations;

int main()
{
    TEST_CHECK_EQ(binomial_small(5, 2), 10);
    TEST_CHECK_EQ(binomial_small(6, 3), 20);

    std::int32_t indices[4] = {0, 0, 0, 0};
    unrank_combination_lex(5, 3, 0, indices);
    TEST_CHECK_EQ(indices[0], 0);
    TEST_CHECK_EQ(indices[1], 1);
    TEST_CHECK_EQ(indices[2], 2);

    unrank_combination_lex(5, 3, 9, indices);
    TEST_CHECK_EQ(indices[0], 2);
    TEST_CHECK_EQ(indices[1], 3);
    TEST_CHECK_EQ(indices[2], 4);

    BenchmarkConfig config;
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;
    config.params["items"] = "16";
    config.params["k"] = "3";

    const auto shape = combination_shape_for_config(config);
    TEST_CHECK_EQ(shape.item_count, 16);
    TEST_CHECK_EQ(shape.choose_k, 3);
    TEST_CHECK_EQ(shape.combination_count, 560);

    const auto problem = make_combination_problem(shape);
    const auto aggregate = evaluate_combinations_cpu_reference(problem);
    TEST_CHECK_EQ(aggregate.valid_count + aggregate.invalid_count, shape.combination_count);
    TEST_CHECK(aggregate.checksum != 0);

    const auto results = run_combination_finder_cpu(config);
    TEST_CHECK_EQ(results.size(), static_cast<std::size_t>(1));
    TEST_CHECK_EQ(results[0].benchmark, std::string("combination_finder"));
    TEST_CHECK_EQ(results[0].variant, std::string("cpu"));
    TEST_CHECK(results[0].correct);
    TEST_CHECK_EQ(results[0].input_size.at("combinations"), shape.combination_count);

    return 0;
}
