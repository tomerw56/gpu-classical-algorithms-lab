#pragma once

#include <chrono>

namespace algobench
{

class CpuTimer
{
public:
    using clock = std::chrono::steady_clock;

    void start();
    double stop_ms();

private:
    clock::time_point start_time_{};
};

} // namespace algobench
