#include "polynomial/polynomial_batch.hpp"

#include "common/benchmark_timer.hpp"
#include "common/device_info.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace algobench::polynomial
{

Coefficients default_coefficients()
{
    Coefficients coefficients{};

    // Coefficients are scaled so that the benchmark can use x values spaced by
    // 100 while keeping the degree-15 polynomial numerically well behaved.
    // Conceptually, the polynomial is a compact alternating series in
    // (x * 1e-5), expressed as an ordinary polynomial in x.
    double scale_power = 1.0;
    for (std::size_t degree = 0; degree < coefficients.size(); ++degree)
    {
        const double sign = (degree % 2 == 0) ? 1.0 : -1.0;
        const double base = sign / static_cast<double>(degree + 1);
        coefficients[degree] = base * scale_power;
        scale_power *= kPolynomialCoefficientScale;
    }

    return coefficients;
}

std::int64_t values_for_preset(const std::string& preset)
{
    if (preset == "tiny")
    {
        return 1024;
    }
    if (preset == "small")
    {
        return 250'000;
    }
    if (preset == "medium")
    {
        return 1'000'000;
    }
    if (preset == "large")
    {
        return 5'000'000;
    }

    throw std::runtime_error("unknown preset for polynomial_batch: " + preset);
}

double x_for_index(std::int64_t index)
{
    const std::int64_t cycle_index = (index % kPolynomialXCycle) + 1;
    return static_cast<double>(cycle_index) * kPolynomialXStep;
}

double evaluate_horner(double x, const Coefficients& coefficients)
{
    double result = coefficients.back();

    for (std::int64_t degree = static_cast<std::int64_t>(coefficients.size()) - 2; degree >= 0; --degree)
    {
        result = result * x + coefficients[static_cast<std::size_t>(degree)];
    }

    return result;
}

namespace
{

void evaluate_into(std::vector<double>& output, const Coefficients& coefficients)
{
    for (std::int64_t i = 0; i < static_cast<std::int64_t>(output.size()); ++i)
    {
        output[static_cast<std::size_t>(i)] = evaluate_horner(x_for_index(i), coefficients);
    }
}

struct ValidationSummary
{
    double checksum = 0.0;
    double reference_checksum = 0.0;
    double max_abs_error = 0.0;
    double max_rel_error = 0.0;
    bool correct = true;
};

ValidationSummary validate_output(const std::vector<double>& output, const Coefficients& coefficients)
{
    ValidationSummary summary;

    for (std::int64_t i = 0; i < static_cast<std::int64_t>(output.size()); ++i)
    {
        const double actual = output[static_cast<std::size_t>(i)];
        const double reference = evaluate_horner(x_for_index(i), coefficients);
        const double abs_error = std::abs(actual - reference);
        const double rel_denominator = std::max(1.0, std::abs(reference));
        const double rel_error = abs_error / rel_denominator;
        const double tolerance = std::max(1.0e-9, std::abs(reference) * 1.0e-11);

        summary.checksum += actual;
        summary.reference_checksum += reference;
        summary.max_abs_error = std::max(summary.max_abs_error, abs_error);
        summary.max_rel_error = std::max(summary.max_rel_error, rel_error);

        if (!std::isfinite(actual) || abs_error > tolerance)
        {
            summary.correct = false;
        }
    }

    return summary;
}

void fill_common_metadata(BenchmarkResult& result, const ValidationSummary& validation)
{
    result.metadata["coefficient_count"] = std::to_string(kPolynomialCoefficientCount);
    result.metadata["x_step"] = std::to_string(kPolynomialXStep);
    result.metadata["x_cycle"] = std::to_string(kPolynomialXCycle);
    result.metadata["checksum"] = std::to_string(validation.checksum);
    result.metadata["reference_checksum"] = std::to_string(validation.reference_checksum);
    result.metadata["max_abs_error"] = std::to_string(validation.max_abs_error);
    result.metadata["max_rel_error"] = std::to_string(validation.max_rel_error);
    result.metadata["validation"] = "element-wise Horner comparison over final output vector";
    result.metadata["x_policy"] = "stride-100 domain cycled every 4096 values to keep degree-15 arithmetic bounded";
}

} // namespace

std::vector<BenchmarkResult> run_polynomial_batch_cpu(const BenchmarkConfig& config)
{
    const std::int64_t n = values_for_preset(config.preset);
    const auto coefficients = default_coefficients();
    std::vector<double> output(static_cast<std::size_t>(n));

    for (int i = 0; i < config.warmup; ++i)
    {
        evaluate_into(output, coefficients);
    }

    CpuTimer timer;
    timer.start();

    for (int r = 0; r < config.repeat; ++r)
    {
        evaluate_into(output, coefficients);
    }

    const double elapsed_ms = timer.stop_ms();
    const auto validation = validate_output(output, coefficients);

    BenchmarkResult result;
    result.benchmark = "polynomial_batch";
    result.variant = "cpu";
    result.preset = config.preset;
    result.repeat = config.repeat;
    result.warmup = config.warmup;
    result.input_size["values"] = n;
    result.input_size["coefficients"] = static_cast<std::int64_t>(kPolynomialCoefficientCount);
    result.total_ms = elapsed_ms;
    result.correct = validation.correct;
    result.device = cpu_device_name();
    result.notes = "degree-15 Horner evaluation over many stride-100 x values";
    fill_common_metadata(result, validation);

    return {result};
}

} // namespace algobench::polynomial
