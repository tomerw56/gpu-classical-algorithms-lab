#include "common/benchmark_registry.hpp"

#include <iostream>

int main()
{
    const auto registry = algobench::make_default_registry();
    for (const auto& info : registry.list())
    {
        std::cout << info.name << " - " << info.description << "\n";
    }
    return 0;
}
