#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"

#include <functional>
#include <string>
#include <vector>

namespace algobench
{

struct BenchmarkInfo
{
    std::string name;
    std::string description;
    std::vector<std::string> presets;
};

using BenchmarkFunction = std::function<std::vector<BenchmarkResult>(const BenchmarkConfig&)>;

class BenchmarkRegistry
{
public:
    void add(BenchmarkInfo info, BenchmarkFunction function);

    std::vector<BenchmarkInfo> list() const;
    std::vector<BenchmarkResult> run(const std::string& name, const BenchmarkConfig& config) const;

private:
    struct Entry
    {
        BenchmarkInfo info;
        BenchmarkFunction function;
    };

    std::vector<Entry> entries_;
};

BenchmarkRegistry make_default_registry();

} // namespace algobench
