#include "polynomial/polynomial_batch.hpp"

#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace
{

double direct_evaluate(double x, const algobench::polynomial::Coefficients& coefficients)
{
    double sum = 0.0;
    double power = 1.0;
    for (double coefficient : coefficients)
    {
        sum += coefficient * power;
        power *= x;
    }
    return sum;
}

double metadata_double(const algobench::BenchmarkResult& result, const std::string& key)
{
    const auto it = result.metadata.find(key);
    TEST_CHECK(it != result.metadata.end());
    if (it == result.metadata.end())
    {
        return 0.0;
    }
    return std::strtod(it->second.c_str(), nullptr);
}

} // namespace

int main()
{
    using namespace algobench::polynomial;

    TEST_CHECK_EQ(kPolynomialCoefficientCount, static_cast<std::size_t>(16));
    TEST_CHECK_EQ(values_for_preset("tiny"), 1024);
    TEST_CHECK_EQ(values_for_preset("small"), 250'000);
    TEST_CHECK_EQ(values_for_preset("medium"), 1'000'000);
    TEST_CHECK_EQ(values_for_preset("large"), 5'000'000);
    TEST_CHECK_THROWS((void)values_for_preset("invalid"));

    const auto coefficients = default_coefficients();
    TEST_CHECK_EQ(coefficients.size(), static_cast<std::size_t>(16));
    TEST_CHECK_NEAR(coefficients[0], 1.0, 1.0e-15);
    TEST_CHECK_NEAR(coefficients[1], -5.0e-6, 1.0e-18);
    TEST_CHECK_NEAR(coefficients[2], 3.3333333333333335e-11, 1.0e-24);

    TEST_CHECK_NEAR(x_for_index(0), 100.0, 1.0e-12);
    TEST_CHECK_NEAR(x_for_index(kPolynomialXCycle - 1), 409600.0, 1.0e-12);
    TEST_CHECK_NEAR(x_for_index(kPolynomialXCycle), 100.0, 1.0e-12);

    const double x = 1200.0;
    const double horner = evaluate_horner(x, coefficients);
    const double direct = direct_evaluate(x, coefficients);
    TEST_CHECK_NEAR(horner, direct, std::max(1.0e-9, std::abs(direct) * 1.0e-11));

    algobench::BenchmarkConfig config;
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;

    const auto once = run_polynomial_batch_cpu(config);
    TEST_CHECK_EQ(once.size(), static_cast<std::size_t>(1));
    const auto& cpu_once = once.front();
    TEST_CHECK_EQ(cpu_once.benchmark, std::string("polynomial_batch"));
    TEST_CHECK_EQ(cpu_once.variant, std::string("cpu"));
    TEST_CHECK_EQ(cpu_once.preset, std::string("tiny"));
    TEST_CHECK_EQ(cpu_once.input_size.at("values"), 1024);
    TEST_CHECK_EQ(cpu_once.input_size.at("coefficients"), static_cast<std::int64_t>(16));
    TEST_CHECK(cpu_once.correct);
    TEST_CHECK(cpu_once.total_ms >= 0.0);
    TEST_CHECK(cpu_once.metadata.count("checksum") == 1);
    TEST_CHECK(cpu_once.metadata.count("reference_checksum") == 1);
    TEST_CHECK(cpu_once.metadata.count("max_abs_error") == 1);
    TEST_CHECK(cpu_once.metadata.count("validation") == 1);

    const double checksum_once = metadata_double(cpu_once, "checksum");
    const double reference_once = metadata_double(cpu_once, "reference_checksum");
    TEST_CHECK_NEAR(checksum_once, reference_once, std::max(1.0e-6, std::abs(reference_once) * 1.0e-10));

    config.repeat = 3;
    const auto repeated = run_polynomial_batch_cpu(config);
    TEST_CHECK_EQ(repeated.size(), static_cast<std::size_t>(1));
    const double checksum_repeated = metadata_double(repeated.front(), "checksum");
    const double reference_repeated = metadata_double(repeated.front(), "reference_checksum");

    // Repeat controls timed executions only. It must not multiply the final checksum.
    TEST_CHECK_NEAR(checksum_repeated, checksum_once, std::max(1.0e-6, std::abs(checksum_once) * 1.0e-10));
    TEST_CHECK_NEAR(reference_repeated, reference_once, std::max(1.0e-6, std::abs(reference_once) * 1.0e-10));

    return algobench::test::finish();
}
