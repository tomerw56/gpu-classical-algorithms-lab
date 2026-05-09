#pragma once

#include "common/benchmark_config.hpp"

#include <string>
#include <vector>

namespace algobench
{

struct CliOptions
{
    BenchmarkConfig config;
    bool list = false;
    bool help = false;
};

CliOptions parse_cli(int argc, char** argv);
void print_help(const char* executable_name);

} // namespace algobench
