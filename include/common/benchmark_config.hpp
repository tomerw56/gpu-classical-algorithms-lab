#pragma once

#include <map>
#include <string>

namespace algobench
{

struct BenchmarkConfig
{
    std::string benchmark = "all";
    std::string preset = "small";
    std::string output_path;

    int repeat = 5;
    int warmup = 1;

    bool include_gpu = true;
    bool validate = true;
    bool verbose = false;

    std::map<std::string, std::string> params;
};

} // namespace algobench
