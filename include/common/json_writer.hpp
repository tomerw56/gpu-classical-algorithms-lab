#pragma once

#include "common/benchmark_result.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace algobench
{

std::string to_json_line(const BenchmarkResult& result);
void write_jsonl(const std::string& path, const std::vector<BenchmarkResult>& results, bool append = true);
void print_result_table(std::ostream& os, const std::vector<BenchmarkResult>& results);

} // namespace algobench
