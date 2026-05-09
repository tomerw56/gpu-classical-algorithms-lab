#pragma once

#include <string>

namespace algobench
{

std::string cpu_device_name();
std::string cuda_device_name();
bool cuda_runtime_available();

} // namespace algobench
