#include "combinations/combination_finder.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace algobench::combinations
{
namespace
{
std::int32_t get_int_param(const BenchmarkConfig& config, const std::string& key, std::int32_t fallback)
{
    const auto it = config.params.find(key);
    if (it == config.params.end())
    {
        return fallback;
    }
    return std::stoi(it->second);
}

std::int64_t checksum_update(std::int64_t checksum, std::int64_t rank, const CombinationEvaluation& eval)
{
    const std::int64_t mask_part = static_cast<std::int64_t>(eval.violation_mask) * 1'000'003LL;
    const std::int64_t score_part = static_cast<std::int64_t>(eval.score) * 97LL;
    return checksum + ((rank + 1) * 13LL) + mask_part + score_part;
}

void accumulate_evaluation(CombinationAggregate& aggregate, std::int64_t rank, const CombinationEvaluation& eval)
{
    if (eval.violation_mask == 0u)
    {
        ++aggregate.valid_count;
        if (eval.score > aggregate.best_score || (eval.score == aggregate.best_score && rank < aggregate.best_rank))
        {
            aggregate.best_score = eval.score;
            aggregate.best_rank = rank;
        }
    }
    else
    {
        ++aggregate.invalid_count;
    }

    if ((eval.violation_mask & kCombinationViolationBudget) != 0u)
    {
        ++aggregate.budget_violations;
    }
    if ((eval.violation_mask & kCombinationViolationRisk) != 0u)
    {
        ++aggregate.risk_violations;
    }
    if ((eval.violation_mask & kCombinationViolationCoverage) != 0u)
    {
        ++aggregate.coverage_violations;
    }
    if ((eval.violation_mask & kCombinationViolationSpread) != 0u)
    {
        ++aggregate.spread_violations;
    }

    aggregate.checksum = checksum_update(aggregate.checksum, rank, eval);
}
} // namespace

CombinationShape combination_shape_for_config(const BenchmarkConfig& config)
{
    CombinationShape shape;
    const auto& preset = config.preset;

    if (preset == "tiny")
    {
        shape.item_count = 64;
        shape.choose_k = 3;
    }
    else if (preset == "small")
    {
        shape.item_count = 192;
        shape.choose_k = 3;
    }
    else if (preset == "medium")
    {
        shape.item_count = 384;
        shape.choose_k = 3;
    }
    else if (preset == "large")
    {
        shape.item_count = 768;
        shape.choose_k = 3;
    }
    else
    {
        // The sweep runner uses --set sweep_label=... together with explicit n/k.
        shape.item_count = 128;
        shape.choose_k = 3;
    }

    shape.item_count = get_int_param(config, "items", shape.item_count);
    shape.choose_k = get_int_param(config, "k", shape.choose_k);

    if (shape.choose_k < 2 || shape.choose_k > 4)
    {
        throw std::runtime_error("combination_finder currently supports k=2, k=3, or k=4");
    }
    if (shape.item_count < shape.choose_k)
    {
        throw std::runtime_error("combination_finder item_count must be >= k");
    }

    shape.combination_count = binomial_small(shape.item_count, shape.choose_k);
    return shape;
}

std::string combination_label_for_config(const BenchmarkConfig& config)
{
    const auto it = config.params.find("sweep_label");
    if (it != config.params.end())
    {
        return it->second;
    }
    return config.preset;
}

CombinationProblem make_combination_problem(const CombinationShape& shape)
{
    CombinationProblem problem;
    problem.shape = shape;
    problem.items.resize(static_cast<std::size_t>(shape.item_count));

    for (std::int32_t i = 0; i < shape.item_count; ++i)
    {
        Item item;
        item.value = 20 + ((i * 37 + 11) % 180);
        item.cost = 5 + ((i * 19 + 7) % 80);
        item.risk = 1 + ((i * 23 + 3) % 60);
        item.x = (i * 29 + 17) % 1000;
        item.y = (i * 43 + 31) % 1000;
        item.category_mask = 1u << static_cast<std::uint32_t>((i * 5 + 2) % 6);
        if ((i % 11) == 0)
        {
            item.category_mask |= 1u << static_cast<std::uint32_t>((i + 3) % 6);
        }
        if ((i % 17) == 0)
        {
            item.category_mask |= 1u << static_cast<std::uint32_t>((i + 5) % 6);
        }
        problem.items[static_cast<std::size_t>(i)] = item;
    }

    const std::int32_t k = shape.choose_k;
    problem.rules.budget_limit = 55 * k;
    problem.rules.risk_limit = 38 * k;
    problem.rules.required_category_mask = 0b000111u;
    problem.rules.max_spread_squared = 420 * 420;
    return problem;
}

CombinationAggregate evaluate_combinations_cpu_reference(const CombinationProblem& problem)
{
    CombinationAggregate aggregate;
    const auto& shape = problem.shape;
    std::int32_t indices[4] = {0, 0, 0, 0};
    std::int64_t rank = 0;

    if (shape.choose_k == 2)
    {
        for (std::int32_t a = 0; a < shape.item_count - 1; ++a)
        {
            for (std::int32_t b = a + 1; b < shape.item_count; ++b)
            {
                indices[0] = a;
                indices[1] = b;
                const auto eval = evaluate_combination_indices(problem.items.data(), indices, 2, problem.rules);
                accumulate_evaluation(aggregate, rank, eval);
                ++rank;
            }
        }
    }
    else if (shape.choose_k == 3)
    {
        for (std::int32_t a = 0; a < shape.item_count - 2; ++a)
        {
            for (std::int32_t b = a + 1; b < shape.item_count - 1; ++b)
            {
                for (std::int32_t c = b + 1; c < shape.item_count; ++c)
                {
                    indices[0] = a;
                    indices[1] = b;
                    indices[2] = c;
                    const auto eval = evaluate_combination_indices(problem.items.data(), indices, 3, problem.rules);
                    accumulate_evaluation(aggregate, rank, eval);
                    ++rank;
                }
            }
        }
    }
    else if (shape.choose_k == 4)
    {
        for (std::int32_t a = 0; a < shape.item_count - 3; ++a)
        {
            for (std::int32_t b = a + 1; b < shape.item_count - 2; ++b)
            {
                for (std::int32_t c = b + 1; c < shape.item_count - 1; ++c)
                {
                    for (std::int32_t d = c + 1; d < shape.item_count; ++d)
                    {
                        indices[0] = a;
                        indices[1] = b;
                        indices[2] = c;
                        indices[3] = d;
                        const auto eval = evaluate_combination_indices(problem.items.data(), indices, 4, problem.rules);
                        accumulate_evaluation(aggregate, rank, eval);
                        ++rank;
                    }
                }
            }
        }
    }
    else
    {
        throw std::runtime_error("unsupported k in evaluate_combinations_cpu_reference");
    }

    if (aggregate.best_rank < 0)
    {
        aggregate.best_score = 0;
    }
    return aggregate;
}

bool compare_combination_aggregates(const CombinationAggregate& actual, const CombinationAggregate& reference)
{
    return actual.valid_count == reference.valid_count &&
           actual.invalid_count == reference.invalid_count &&
           actual.budget_violations == reference.budget_violations &&
           actual.risk_violations == reference.risk_violations &&
           actual.coverage_violations == reference.coverage_violations &&
           actual.spread_violations == reference.spread_violations &&
           actual.checksum == reference.checksum &&
           actual.best_score == reference.best_score;
}

void fill_combination_metadata(BenchmarkResult& result,
                               const CombinationAggregate& actual,
                               const CombinationAggregate& reference)
{
    result.metadata["valid_count"] = std::to_string(actual.valid_count);
    result.metadata["reference_valid_count"] = std::to_string(reference.valid_count);
    result.metadata["invalid_count"] = std::to_string(actual.invalid_count);
    result.metadata["budget_violations"] = std::to_string(actual.budget_violations);
    result.metadata["risk_violations"] = std::to_string(actual.risk_violations);
    result.metadata["coverage_violations"] = std::to_string(actual.coverage_violations);
    result.metadata["spread_violations"] = std::to_string(actual.spread_violations);
    result.metadata["checksum"] = std::to_string(actual.checksum);
    result.metadata["reference_checksum"] = std::to_string(reference.checksum);
    result.metadata["best_score"] = std::to_string(actual.best_score);
    result.metadata["reference_best_score"] = std::to_string(reference.best_score);
    result.metadata["best_rank"] = std::to_string(actual.best_rank);
}

} // namespace algobench::combinations
