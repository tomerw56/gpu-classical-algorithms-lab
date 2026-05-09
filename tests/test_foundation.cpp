#include "common/benchmark_config.hpp"
#include "foundation/foundation_smoke.hpp"

#include <iostream>

int main()
{
    algobench::BenchmarkConfig config;
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;

    const auto results = algobench::foundation::run_foundation_smoke_cpu(config);
    if (results.empty())
    {
        std::cerr << "no results returned\n";
        return 1;
    }

    if (!results.front().correct)
    {
        std::cerr << "foundation smoke CPU benchmark failed validation\n";
        return 2;
    }

    return 0;
}
