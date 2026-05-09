cmake_minimum_required(VERSION 3.24)

project(gpu_classical_algorithms_lab
    VERSION 0.1.0
    DESCRIPTION "Classical algorithms CPU/GPU benchmark lab"
    LANGUAGES CXX)

option(ENABLE_CUDA "Build CUDA GPU benchmarks when a CUDA compiler is available" OFF)
option(REQUIRE_CUDA "Fail configure if ENABLE_CUDA=ON but CUDA is missing" ON)

option(BUILD_TESTS "Build smoke tests" ON)
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckLanguage)
set(GPUALGOBENCH_HAS_CUDA OFF)

if(ENABLE_CUDA)
    check_language(CUDA)

    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        set(GPUALGOBENCH_HAS_CUDA ON)
        message(STATUS "CUDA compiler found: ${CMAKE_CUDA_COMPILER}")
    else()
        if(REQUIRE_CUDA)
            message(FATAL_ERROR
                "ENABLE_CUDA=ON, but no CUDA compiler was found.\n"
                "Install the CUDA Toolkit or pass:\n"
                "  -DCMAKE_CUDA_COMPILER=/path/to/nvcc\n"
                "On Windows this is often:\n"
                "  -DCMAKE_CUDA_COMPILER=\"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.x/bin/nvcc.exe\"")
        else()
            message(WARNING "ENABLE_CUDA=ON, but no CUDA compiler was found. Building CPU-only.")
        endif()
    endif()
endif()

add_library(gpu_algobench_common STATIC
       src/common/benchmark_registry.cpp
    src/common/benchmark_timer.cpp
    src/common/cli.cpp
    src/common/device_info.cpp
    src/common/json_writer.cpp
    src/common/random_utils.cpp
    src/foundation/foundation_smoke_cpu.cpp
)

target_include_directories(gpu_algobench_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(GPUALGOBENCH_HAS_CUDA)
    target_include_directories(gpu_algobench_common
        PRIVATE
            ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES}
    )
endif()

target_compile_features(gpu_algobench_common PUBLIC cxx_std_17)

if(GPUALGOBENCH_HAS_CUDA)
    target_sources(gpu_algobench_common PRIVATE
        src/foundation/foundation_smoke_gpu.cu)
    target_compile_definitions(gpu_algobench_common PUBLIC GPUALGOBENCH_ENABLE_CUDA=1)
    set_target_properties(gpu_algobench_common PROPERTIES
        CUDA_STANDARD 17
        CUDA_STANDARD_REQUIRED ON)
else()
    target_compile_definitions(gpu_algobench_common PUBLIC GPUALGOBENCH_ENABLE_CUDA=0)
endif()

# Important when a target contains both .cpp and .cu files:
# warning flags such as /W4, /permissive-, -Wall, and -Wextra are C++ compiler
# options, not CUDA compiler options. Keep them scoped to CXX so they are not
# passed directly to nvcc.
if(MSVC)
    target_compile_options(gpu_algobench_common PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->)

    if(ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(gpu_algobench_common PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/WX>)
    endif()
else()
    target_compile_options(gpu_algobench_common PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>)

    if(ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(gpu_algobench_common PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
    endif()
endif()

add_executable(gpu_algobench apps/gpu_algobench.cpp)
target_link_libraries(gpu_algobench PRIVATE gpu_algobench_common)

add_executable(list_benchmarks apps/list_benchmarks.cpp)
target_link_libraries(list_benchmarks PRIVATE gpu_algobench_common)

add_executable(validate_all apps/validate_all.cpp)
target_link_libraries(validate_all PRIVATE gpu_algobench_common)

if(BUILD_TESTS)
    enable_testing()
    add_executable(test_foundation tests/test_foundation.cpp)
    target_link_libraries(test_foundation PRIVATE gpu_algobench_common)
    add_test(NAME test_foundation COMMAND test_foundation)
endif()
