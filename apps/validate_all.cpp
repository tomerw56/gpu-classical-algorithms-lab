#include "common/benchmark_config.hpp"
#include "common/benchmark_registry.hpp"
#include "common/json_writer.hpp"

#include <iostream>

int main()
{
    auto registry = algobench::make_default_registry();

    algobench::BenchmarkConfig config;
    config.benchmark = "all";
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;
    config.include_gpu = true;
    config.validate = true;

    const auto results = registry.run("all", config);
    algobench::print_result_table(std::cout, results);

    for (const auto& result : results)
    {
        if (!result.correct)
        {
            std::cerr << "validation failed for " << result.benchmark << " / " << result.variant << "\n";
            return 2;
        }
    }

    return 0;
}
