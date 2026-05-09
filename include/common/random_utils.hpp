#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace algobench
{

std::mt19937 make_rng(std::uint32_t seed = 123456789u);
std::vector<double> random_double_vector(std::size_t size, double min_value, double max_value, std::uint32_t seed = 123456789u);
std::vector<int> random_int_vector(std::size_t size, int min_value, int max_value, std::uint32_t seed = 123456789u);

} // namespace algobench
