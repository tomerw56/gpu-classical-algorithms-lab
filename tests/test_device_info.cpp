#include "common/device_info.hpp"

#include "test_support.hpp"

#include <string>

int main()
{
    const std::string cpu_name = algobench::cpu_device_name();
    TEST_CHECK(cpu_name.find("CPU threads=") != std::string::npos);

    const auto info = algobench::cuda_runtime_info();
    const std::string status = algobench::cuda_runtime_status();
    TEST_CHECK(!status.empty());

#if GPUALGOBENCH_ENABLE_CUDA
    TEST_CHECK(info.compiled_with_cuda);
    TEST_CHECK(info.runtime_version >= 0);
    TEST_CHECK(info.driver_version >= 0);
#else
    TEST_CHECK(!info.compiled_with_cuda);
    TEST_CHECK(!info.available);
#endif

    const std::string device_name = algobench::cuda_device_name();
    TEST_CHECK(!device_name.empty());

    return algobench::test::finish();
}
