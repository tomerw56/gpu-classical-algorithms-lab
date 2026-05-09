#include "common/random_utils.hpp"

namespace algobench
{

std::mt19937 make_rng(std::uint32_t seed)
{
    return std::mt19937(seed);
}

std::vector<double> random_double_vector(std::size_t size, double min_value, double max_value, std::uint32_t seed)
{
    auto rng = make_rng(seed);
    std::uniform_real_distribution<double> dist(min_value, max_value);

    std::vector<double> values(size);
    for (auto& value : values)
    {
        value = dist(rng);
    }
    return values;
}

std::vector<int> random_int_vector(std::size_t size, int min_value, int max_value, std::uint32_t seed)
{
    auto rng = make_rng(seed);
    std::uniform_int_distribution<int> dist(min_value, max_value);

    std::vector<int> values(size);
    for (auto& value : values)
    {
        value = dist(rng);
    }
    return values;
}

} // namespace algobench
