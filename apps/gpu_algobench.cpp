#include "common/benchmark_registry.hpp"
#include "common/cli.hpp"
#include "common/json_writer.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv)
{
    try
    {
        const auto options = algobench::parse_cli(argc, argv);
        const auto registry = algobench::make_default_registry();

        if (options.help)
        {
            algobench::print_help(argv[0]);
            return 0;
        }

        if (options.list)
        {
            for (const auto& info : registry.list())
            {
                std::cout << info.name << "\n  " << info.description << "\n  presets: ";
                for (std::size_t i = 0; i < info.presets.size(); ++i)
                {
                    if (i > 0)
                    {
                        std::cout << ", ";
                    }
                    std::cout << info.presets[i];
                }
                std::cout << "\n\n";
            }
            return 0;
        }

        const auto results = registry.run(options.config.benchmark, options.config);
        algobench::print_result_table(std::cout, results);
        algobench::write_jsonl(options.config.output_path, results, true);

        for (const auto& result : results)
        {
            if (!result.correct && options.config.validate)
            {
                return 2;
            }
        }

        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "error: " << ex.what() << "\n";
        algobench::print_help(argv[0]);
        return 1;
    }
}
