#include "common/benchmark_timer.hpp"

namespace algobench
{

void CpuTimer::start()
{
    start_time_ = clock::now();
}

double CpuTimer::stop_ms()
{
    const auto end_time = clock::now();
    const auto elapsed = std::chrono::duration<double, std::milli>(end_time - start_time_);
    return elapsed.count();
}

} // namespace algobench
