#include "common/cli.hpp"

#include "test_support.hpp"

#include <string>
#include <vector>

namespace
{

algobench::CliOptions parse(std::vector<std::string> args)
{
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args)
    {
        argv.push_back(arg.data());
    }

    return algobench::parse_cli(static_cast<int>(argv.size()), argv.data());
}

} // namespace

int main()
{
    {
        const auto options = parse({"gpu_algobench", "--help"});
        TEST_CHECK(options.help);
    }

    {
        const auto options = parse({"gpu_algobench", "--list"});
        TEST_CHECK(options.list);
    }

    {
        const auto options = parse({
            "gpu_algobench",
            "--benchmark", "foundation_smoke",
            "--preset", "tiny",
            "--repeat", "7",
            "--warmup", "2",
            "--output", "results/out.jsonl",
            "--no-gpu",
            "--no-validate",
            "--verbose",
            "--set", "nodes=100",
            "--set", "edges=200"});

        TEST_CHECK_EQ(options.config.benchmark, std::string("foundation_smoke"));
        TEST_CHECK_EQ(options.config.preset, std::string("tiny"));
        TEST_CHECK_EQ(options.config.repeat, 7);
        TEST_CHECK_EQ(options.config.warmup, 2);
        TEST_CHECK_EQ(options.config.output_path, std::string("results/out.jsonl"));
        TEST_CHECK(!options.config.include_gpu);
        TEST_CHECK(!options.config.validate);
        TEST_CHECK(options.config.verbose);
        TEST_CHECK_EQ(options.config.params.at("nodes"), std::string("100"));
        TEST_CHECK_EQ(options.config.params.at("edges"), std::string("200"));
    }

    TEST_CHECK_THROWS((void)parse({"gpu_algobench", "--benchmark"}));
    TEST_CHECK_THROWS((void)parse({"gpu_algobench", "--repeat", "0"}));
    TEST_CHECK_THROWS((void)parse({"gpu_algobench", "--repeat", "-1"}));
    TEST_CHECK_THROWS((void)parse({"gpu_algobench", "--warmup", "-1"}));
    TEST_CHECK_THROWS((void)parse({"gpu_algobench", "--set", "missing_equals"}));
    TEST_CHECK_THROWS((void)parse({"gpu_algobench", "--unknown"}));

    return algobench::test::finish();
}
