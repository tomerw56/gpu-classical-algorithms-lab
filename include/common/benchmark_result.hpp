#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace algobench
{

struct BenchmarkResult
{
    std::string benchmark;
    std::string variant;
    std::string preset;

    int repeat = 1;
    int warmup = 0;

    std::map<std::string, std::int64_t> input_size;

    double total_ms = 0.0;
    double h2d_ms = 0.0;
    double kernel_ms = 0.0;
    double d2h_ms = 0.0;

    bool correct = false;

    std::string device;
    std::string notes;

    std::map<std::string, std::string> metadata;
};

} // namespace algobench
