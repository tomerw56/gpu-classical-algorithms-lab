#pragma once

#include "common/benchmark_config.hpp"
#include "common/benchmark_result.hpp"

#include <cstdint>
#include <string>
#include <vector>

#if defined(__CUDACC__)
#define ALGOBENCH_HD __host__ __device__ inline
#else
#define ALGOBENCH_HD inline
#endif

namespace algobench::combinations
{

constexpr std::uint32_t kCombinationViolationBudget = 1u << 0;
constexpr std::uint32_t kCombinationViolationRisk = 1u << 1;
constexpr std::uint32_t kCombinationViolationCoverage = 1u << 2;
constexpr std::uint32_t kCombinationViolationSpread = 1u << 3;

struct CombinationShape
{
    std::int32_t item_count = 0;
    std::int32_t choose_k = 3;
    std::int64_t combination_count = 0;
};

struct Item
{
    std::int32_t value = 0;
    std::int32_t cost = 0;
    std::int32_t risk = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::uint32_t category_mask = 0;
};

struct CombinationRules
{
    std::int32_t budget_limit = 0;
    std::int32_t risk_limit = 0;
    std::uint32_t required_category_mask = 0;
    std::int32_t max_spread_squared = 0;
};

struct CombinationProblem
{
    CombinationShape shape;
    CombinationRules rules;
    std::vector<Item> items;
};

struct CombinationAggregate
{
    std::int64_t valid_count = 0;
    std::int64_t invalid_count = 0;
    std::int64_t budget_violations = 0;
    std::int64_t risk_violations = 0;
    std::int64_t coverage_violations = 0;
    std::int64_t spread_violations = 0;
    std::int64_t checksum = 0;
    std::int32_t best_score = -2147483647;
    std::int64_t best_rank = -1;
};

struct CombinationEvaluation
{
    std::uint32_t violation_mask = 0;
    std::int32_t score = 0;
};

ALGOBENCH_HD std::int32_t popcount8(std::uint32_t mask)
{
    std::int32_t count = 0;
    for (std::int32_t i = 0; i < 8; ++i)
    {
        count += static_cast<std::int32_t>((mask >> i) & 1u);
    }
    return count;
}

ALGOBENCH_HD std::int64_t binomial_small(std::int32_t n, std::int32_t k)
{
    if (k < 0 || n < 0 || k > n)
    {
        return 0;
    }
    if (k > n - k)
    {
        k = n - k;
    }

    std::int64_t result = 1;
    for (std::int32_t i = 1; i <= k; ++i)
    {
        result = (result * static_cast<std::int64_t>(n - k + i)) / i;
    }
    return result;
}

ALGOBENCH_HD void unrank_combination_lex(std::int32_t n,
                                          std::int32_t k,
                                          std::int64_t rank,
                                          std::int32_t* out_indices)
{
    std::int32_t start = 0;
    for (std::int32_t pos = 0; pos < k; ++pos)
    {
        for (std::int32_t candidate = start; candidate <= n - (k - pos); ++candidate)
        {
            const std::int64_t count_with_candidate = binomial_small(n - candidate - 1, k - pos - 1);
            if (rank < count_with_candidate)
            {
                out_indices[pos] = candidate;
                start = candidate + 1;
                break;
            }
            rank -= count_with_candidate;
        }
    }
}

ALGOBENCH_HD std::int32_t squared_distance_i32(const Item& a, const Item& b)
{
    const std::int32_t dx = a.x - b.x;
    const std::int32_t dy = a.y - b.y;
    return dx * dx + dy * dy;
}

ALGOBENCH_HD CombinationEvaluation evaluate_combination_indices(const Item* items,
                                                                const std::int32_t* indices,
                                                                std::int32_t k,
                                                                const CombinationRules& rules)
{
    std::int32_t total_value = 0;
    std::int32_t total_cost = 0;
    std::int32_t total_risk = 0;
    std::uint32_t coverage = 0;
    std::int32_t max_distance2 = 0;

    for (std::int32_t i = 0; i < k; ++i)
    {
        const Item& item = items[indices[i]];
        total_value += item.value;
        total_cost += item.cost;
        total_risk += item.risk;
        coverage |= item.category_mask;
    }

    for (std::int32_t i = 0; i < k; ++i)
    {
        for (std::int32_t j = i + 1; j < k; ++j)
        {
            const std::int32_t distance2 = squared_distance_i32(items[indices[i]], items[indices[j]]);
            if (distance2 > max_distance2)
            {
                max_distance2 = distance2;
            }
        }
    }

    std::uint32_t mask = 0;
    if (total_cost > rules.budget_limit)
    {
        mask |= kCombinationViolationBudget;
    }
    if (total_risk > rules.risk_limit)
    {
        mask |= kCombinationViolationRisk;
    }
    if ((coverage & rules.required_category_mask) != rules.required_category_mask)
    {
        mask |= kCombinationViolationCoverage;
    }
    if (max_distance2 > rules.max_spread_squared)
    {
        mask |= kCombinationViolationSpread;
    }

    const std::int32_t diversity_bonus = popcount8(coverage) * 25;
    const std::int32_t compactness_bonus = max_distance2 <= rules.max_spread_squared ? 50 : 0;
    const std::int32_t score = total_value * 100 - total_cost * 3 - total_risk * 7 + diversity_bonus + compactness_bonus;

    return CombinationEvaluation{mask, score};
}

CombinationShape combination_shape_for_config(const BenchmarkConfig& config);
std::string combination_label_for_config(const BenchmarkConfig& config);
CombinationProblem make_combination_problem(const CombinationShape& shape);
CombinationAggregate evaluate_combinations_cpu_reference(const CombinationProblem& problem);
bool compare_combination_aggregates(const CombinationAggregate& actual, const CombinationAggregate& reference);
void fill_combination_metadata(BenchmarkResult& result,
                               const CombinationAggregate& actual,
                               const CombinationAggregate& reference);

std::vector<BenchmarkResult> run_combination_finder_cpu(const BenchmarkConfig& config);
std::vector<BenchmarkResult> run_combination_finder_gpu(const BenchmarkConfig& config);

} // namespace algobench::combinations

#undef ALGOBENCH_HD
