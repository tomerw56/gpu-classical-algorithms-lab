#include "foundation/foundation_smoke.hpp"

#include "test_support.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

namespace
{

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
    TEST_CHECK_EQ(algobench::foundation::values_for_preset("tiny"), 1024);
    TEST_CHECK_EQ(algobench::foundation::values_for_preset("small"), 1'000'000);
    TEST_CHECK_EQ(algobench::foundation::values_for_preset("medium"), 5'000'000);
    TEST_CHECK_EQ(algobench::foundation::values_for_preset("large"), 20'000'000);
    TEST_CHECK_THROWS((void)algobench::foundation::values_for_preset("invalid"));

    algobench::BenchmarkConfig config;
    config.preset = "tiny";
    config.repeat = 1;
    config.warmup = 0;

    const auto once = algobench::foundation::run_foundation_smoke_cpu(config);
    TEST_CHECK_EQ(once.size(), static_cast<std::size_t>(1));

    const auto& cpu_once = once.front();
    TEST_CHECK_EQ(cpu_once.benchmark, std::string("foundation_smoke"));
    TEST_CHECK_EQ(cpu_once.variant, std::string("cpu"));
    TEST_CHECK_EQ(cpu_once.preset, std::string("tiny"));
    TEST_CHECK_EQ(cpu_once.repeat, 1);
    TEST_CHECK_EQ(cpu_once.warmup, 0);
    TEST_CHECK_EQ(cpu_once.input_size.at("values"), 1024);
    TEST_CHECK(cpu_once.correct);
    TEST_CHECK(cpu_once.total_ms >= 0.0);
    TEST_CHECK(cpu_once.metadata.count("checksum") == 1);
    TEST_CHECK(cpu_once.metadata.count("reference") == 1);
    TEST_CHECK(cpu_once.metadata.count("checksum_policy") == 1);

    const double checksum_once = metadata_double(cpu_once, "checksum");
    const double reference_once = metadata_double(cpu_once, "reference");
    TEST_CHECK_NEAR(checksum_once, reference_once, std::max(1e-6, std::abs(reference_once) * 1e-10));

    config.repeat = 3;
    const auto repeated = algobench::foundation::run_foundation_smoke_cpu(config);
    TEST_CHECK_EQ(repeated.size(), static_cast<std::size_t>(1));

    const double checksum_repeated = metadata_double(repeated.front(), "checksum");
    const double reference_repeated = metadata_double(repeated.front(), "reference");

    // repeat is a timing control. It must not multiply the reported checksum.
    TEST_CHECK_NEAR(checksum_repeated, checksum_once, std::max(1e-6, std::abs(checksum_once) * 1e-10));
    TEST_CHECK_NEAR(reference_repeated, reference_once, std::max(1e-6, std::abs(reference_once) * 1e-10));

    return algobench::test::finish();
}
