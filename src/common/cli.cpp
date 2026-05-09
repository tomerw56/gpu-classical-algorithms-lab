#include "common/cli.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace algobench
{
namespace
{

std::string require_value(int& i, int argc, char** argv, const std::string& option)
{
    if (i + 1 >= argc)
    {
        throw std::runtime_error("missing value for " + option);
    }
    ++i;
    return argv[i];
}

int parse_positive_int(const std::string& text, const std::string& option)
{
    const int value = std::atoi(text.c_str());
    if (value <= 0)
    {
        throw std::runtime_error(option + " must be positive");
    }
    return value;
}

} // namespace

CliOptions parse_cli(int argc, char** argv)
{
    CliOptions options;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            options.help = true;
        }
        else if (arg == "--list")
        {
            options.list = true;
        }
        else if (arg == "--benchmark")
        {
            options.config.benchmark = require_value(i, argc, argv, arg);
        }
        else if (arg == "--preset")
        {
            options.config.preset = require_value(i, argc, argv, arg);
        }
        else if (arg == "--repeat")
        {
            options.config.repeat = parse_positive_int(require_value(i, argc, argv, arg), arg);
        }
        else if (arg == "--warmup")
        {
            const int value = std::atoi(require_value(i, argc, argv, arg).c_str());
            if (value < 0)
            {
                throw std::runtime_error("--warmup must be non-negative");
            }
            options.config.warmup = value;
        }
        else if (arg == "--output")
        {
            options.config.output_path = require_value(i, argc, argv, arg);
        }
        else if (arg == "--no-gpu")
        {
            options.config.include_gpu = false;
        }
        else if (arg == "--no-validate")
        {
            options.config.validate = false;
        }
        else if (arg == "--verbose")
        {
            options.config.verbose = true;
        }
        else if (arg == "--set")
        {
            const std::string kv = require_value(i, argc, argv, arg);
            const auto pos = kv.find('=');
            if (pos == std::string::npos)
            {
                throw std::runtime_error("--set expects key=value");
            }
            options.config.params[kv.substr(0, pos)] = kv.substr(pos + 1);
        }
        else
        {
            throw std::runtime_error("unknown option: " + arg);
        }
    }

    return options;
}

void print_help(const char* executable_name)
{
    std::cout
        << "Usage:\n"
        << "  " << executable_name << " --list\n"
        << "  " << executable_name << " --benchmark <name|all> [options]\n\n"
        << "Options:\n"
        << "  --benchmark NAME     Benchmark to run. Default: all\n"
        << "  --preset NAME        tiny, small, medium, large. Default: small\n"
        << "  --repeat N           Number of measured repeats. Default: 5\n"
        << "  --warmup N           Number of warmup runs. Default: 1\n"
        << "  --output PATH        Append JSONL results to file\n"
        << "  --no-gpu             Skip GPU variants even if CUDA is built\n"
        << "  --no-validate        Skip validation when supported\n"
        << "  --set key=value      Benchmark-specific parameter override\n"
        << "  --verbose            Print extra details\n"
        << "  --help               Show this help\n";
}

} // namespace algobench
