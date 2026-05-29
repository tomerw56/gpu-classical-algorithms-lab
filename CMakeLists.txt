cmake_minimum_required(VERSION 3.24)

project(gpu_classical_algorithms_lab
    VERSION 0.3.4
    DESCRIPTION "Classical algorithms CPU/GPU benchmark lab"
    LANGUAGES CXX
)

# ============================================================
# Options
# ============================================================

option(ENABLE_CUDA "Build CUDA GPU benchmarks" ON)
option(BUILD_TESTS "Build tests" ON)
option(ENABLE_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# If ON, requesting CUDA but failing to find it is a hard error.
# This is useful while developing the GPU-enabled build, because it prevents
# accidental CPU-only builds when CUDA was expected.
option(REQUIRE_CUDA "Fail configure if ENABLE_CUDA=ON but CUDA is unavailable" ON)

# ============================================================
# C++ standard
# ============================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Optional CUDA language enablement
# ============================================================

include(CheckLanguage)

set(GPUALGOBENCH_HAS_CUDA OFF)

if(ENABLE_CUDA)
    check_language(CUDA)

    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)

        set(CMAKE_CUDA_STANDARD 17)
        set(CMAKE_CUDA_STANDARD_REQUIRED ON)
        set(CMAKE_CUDA_EXTENSIONS OFF)

        # Important:
        # This makes CUDAToolkit_ROOT meaningful and gives us CUDA::cudart
        # and CUDAToolkit_INCLUDE_DIRS.
        find_package(CUDAToolkit REQUIRED)

        set(GPUALGOBENCH_HAS_CUDA ON)

        message(STATUS "CUDA compiler found: ${CMAKE_CUDA_COMPILER}")
        message(STATUS "CUDA compiler version: ${CMAKE_CUDA_COMPILER_VERSION}")
        message(STATUS "CUDA toolkit include dirs: ${CUDAToolkit_INCLUDE_DIRS}")

        if(DEFINED CMAKE_CUDA_HOST_COMPILER)
            message(STATUS "CUDA host compiler: ${CMAKE_CUDA_HOST_COMPILER}")
        endif()

        # If the user did not provide an architecture, choose a reasonable
        # default for the current RTX 1000 Ada setup.
        #
        # Override from the command line when needed:
        #   -DCMAKE_CUDA_ARCHITECTURES=89
        if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
            set(CMAKE_CUDA_ARCHITECTURES 89)
            message(STATUS "CMAKE_CUDA_ARCHITECTURES not set; defaulting to 89")
        endif()
    else()
        if(REQUIRE_CUDA)
            message(FATAL_ERROR
                "ENABLE_CUDA=ON, but no CUDA compiler was found.\n"
                "For Ninja on Windows, run from a vcvars64.bat environment and pass:\n"
                "  -DCMAKE_CUDA_COMPILER=<CUDA_ROOT>/bin/nvcc.exe\n"
                "  -DCMAKE_CUDA_HOST_COMPILER=cl.exe\n"
            )
        else()
            message(WARNING
                "ENABLE_CUDA=ON, but no CUDA compiler was found. Building CPU-only."
            )
        endif()
    endif()
endif()

# ============================================================
# Common library
# ============================================================

set(GPUALGOBENCH_COMMON_SOURCES
    src/common/benchmark_registry.cpp
    src/common/benchmark_timer.cpp
    src/common/cli.cpp
    src/common/device_info.cpp
    src/common/json_writer.cpp
    src/common/random_utils.cpp

    src/foundation/foundation_smoke_cpu.cpp
    src/polynomial/polynomial_batch_cpu.cpp
    src/cost_matrix/cost_matrix_common.cpp
    src/cost_matrix/cost_matrix_cpu.cpp
    src/spatial_events/spatial_events_common.cpp
    src/spatial_events/spatial_events_cpu.cpp
    src/graph/graph_foundation.cpp
    src/graph/graph_bfs_common.cpp
    src/graph/graph_bfs_cpu.cpp
    src/graph/graph_connected_components_common.cpp
    src/graph/graph_connected_components_cpu.cpp
    src/graph/graph_weighted_relaxation_common.cpp
    src/graph/graph_weighted_relaxation_cpu.cpp
    src/constraints/constraint_network_common.cpp
    src/constraints/constraint_network_cpu.cpp
)

if(GPUALGOBENCH_HAS_CUDA)
    list(APPEND GPUALGOBENCH_COMMON_SOURCES
        src/foundation/foundation_smoke_gpu.cu
        src/polynomial/polynomial_batch_gpu.cu
        src/cost_matrix/cost_matrix_gpu.cu
        src/spatial_events/spatial_events_gpu.cu
        src/graph/graph_bfs_gpu.cu
        src/graph/graph_connected_components_gpu.cu
        src/graph/graph_weighted_relaxation_gpu.cu
        src/constraints/constraint_network_gpu.cu
    )
endif()

add_library(gpu_algobench_common STATIC
    ${GPUALGOBENCH_COMMON_SOURCES}
)

target_include_directories(gpu_algobench_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gpu_algobench_common PUBLIC cxx_std_17)

if(GPUALGOBENCH_HAS_CUDA)
    # Important fix:
    # device_info.cpp is compiled as normal C++ by cl.exe, but it includes
    # cuda_runtime.h when CUDA is enabled. Therefore the CUDA include directory
    # must also be visible to C++ sources, not only to .cu sources.
    target_include_directories(gpu_algobench_common
        PRIVATE
            ${CUDAToolkit_INCLUDE_DIRS}
    )

    target_link_libraries(gpu_algobench_common
        PRIVATE
            CUDA::cudart
    )

    target_compile_definitions(gpu_algobench_common
        PUBLIC
            GPUALGOBENCH_ENABLE_CUDA=1
    )

    set_target_properties(gpu_algobench_common PROPERTIES
        CUDA_STANDARD 17
        CUDA_STANDARD_REQUIRED ON
        CUDA_EXTENSIONS OFF
        CUDA_RUNTIME_LIBRARY Shared
    )
else()
    target_compile_definitions(gpu_algobench_common
        PUBLIC
            GPUALGOBENCH_ENABLE_CUDA=0
    )
endif()

# ============================================================
# Compiler warnings
# ============================================================

# Important:
# gpu_algobench_common may contain both .cpp and .cu files.
# Do not pass MSVC flags like /W4 or /permissive- directly to nvcc.
# Scope warning flags by language.
if(MSVC)
    target_compile_options(gpu_algobench_common PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
    )

    if(ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(gpu_algobench_common PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/WX>
        )
    endif()
else()
    target_compile_options(gpu_algobench_common PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
    )

    if(ENABLE_WARNINGS_AS_ERRORS)
        target_compile_options(gpu_algobench_common PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Werror>
        )
    endif()
endif()

# ============================================================
# Applications
# ============================================================

add_executable(gpu_algobench
    apps/gpu_algobench.cpp
)

target_link_libraries(gpu_algobench
    PRIVATE
        gpu_algobench_common
)

add_executable(list_benchmarks
    apps/list_benchmarks.cpp
)

target_link_libraries(list_benchmarks
    PRIVATE
        gpu_algobench_common
)

add_executable(validate_all
    apps/validate_all.cpp
)

target_link_libraries(validate_all
    PRIVATE
        gpu_algobench_common
)

add_executable(cuda_probe
    apps/cuda_probe.cpp
)

target_link_libraries(cuda_probe
    PRIVATE
        gpu_algobench_common
)

add_executable(export_cost_matrix
    apps/export_cost_matrix.cpp
)

target_link_libraries(export_cost_matrix
    PRIVATE
        gpu_algobench_common
)

add_executable(export_spatial_events
    apps/export_spatial_events.cpp
)

target_link_libraries(export_spatial_events
    PRIVATE
        gpu_algobench_common
)

add_executable(export_graph_foundation
    apps/export_graph_foundation.cpp
)

target_link_libraries(export_graph_foundation
    PRIVATE
        gpu_algobench_common
)

add_executable(export_graph_components
    apps/export_graph_components.cpp
)

target_link_libraries(export_graph_components
    PRIVATE
        gpu_algobench_common
)

add_executable(export_constraint_network
    apps/export_constraint_network.cpp
)

target_link_libraries(export_constraint_network
    PRIVATE
        gpu_algobench_common
)

# ============================================================
# Tests
# ============================================================

if(BUILD_TESTS)
    enable_testing()

    function(add_algobench_test test_name source_file)
        add_executable(${test_name}
            ${source_file}
        )

        target_include_directories(${test_name}
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/tests
        )

        target_link_libraries(${test_name}
            PRIVATE
                gpu_algobench_common
        )

        add_test(
            NAME ${test_name}
            COMMAND ${test_name}
        )
    endfunction()

    add_algobench_test(test_foundation tests/test_foundation.cpp)
    add_algobench_test(test_polynomial tests/test_polynomial.cpp)
    add_algobench_test(test_cost_matrix tests/test_cost_matrix.cpp)
    add_algobench_test(test_spatial_events tests/test_spatial_events.cpp)
    add_algobench_test(test_graph_foundation tests/test_graph_foundation.cpp)
    add_algobench_test(test_graph_bfs tests/test_graph_bfs.cpp)
    add_algobench_test(test_graph_connected_components tests/test_graph_connected_components.cpp)
    add_algobench_test(test_graph_weighted_relaxation tests/test_graph_weighted_relaxation.cpp)
    add_algobench_test(test_constraint_network tests/test_constraint_network.cpp)

    add_test(
        NAME export_cost_matrix_smoke
        COMMAND export_cost_matrix
            --tasks 4
            --resources 5
            --output-dir ${CMAKE_CURRENT_BINARY_DIR}/export_cost_matrix_smoke
    )

    add_test(
        NAME export_spatial_events_smoke
        COMMAND export_spatial_events
            --tracks 5
            --zones 4
            --output-dir ${CMAKE_CURRENT_BINARY_DIR}/export_spatial_events_smoke
    )

    add_test(
        NAME export_graph_foundation_smoke
        COMMAND export_graph_foundation
            --chain-nodes 6
            --grid-width 3
            --grid-height 2
            --layers 3
            --nodes-per-layer 4
            --fanout 2
            --random-nodes 8
            --random-out-degree 2
            --output-dir ${CMAKE_CURRENT_BINARY_DIR}/export_graph_foundation_smoke
    )

    add_test(
        NAME export_graph_components_smoke
        COMMAND export_graph_components
            --graph random_clusters
            --components 3
            --component-size 12
            --random-out-degree 2
            --output-dir ${CMAKE_CURRENT_BINARY_DIR}/export_graph_components_smoke
    )

    add_test(
        NAME export_constraint_network_smoke
        COMMAND export_constraint_network
            --tasks 6
            --resources 4
            --candidates 24
            --output-dir ${CMAKE_CURRENT_BINARY_DIR}/export_constraint_network_smoke
    )

    add_algobench_test(test_registry tests/test_registry.cpp)
    add_algobench_test(test_cli tests/test_cli.cpp)
    add_algobench_test(test_json_writer tests/test_json_writer.cpp)
    add_algobench_test(test_random_utils tests/test_random_utils.cpp)
    add_algobench_test(test_device_info tests/test_device_info.cpp)
endif()

# ============================================================
# Convenience scripts
# ============================================================

# Keep the canonical script at the repository root, but also copy it to the
# build directory so users can run it from either place.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/execute_all_tests.bat
    ${CMAKE_CURRENT_BINARY_DIR}/execute_all_tests.bat
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/execute_graph_bfs_all_sweeps_and_analyze.bat
    ${CMAKE_CURRENT_BINARY_DIR}/execute_graph_bfs_all_sweeps_and_analyze.bat
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/execute_graph_connected_components_all_sweeps_and_analyze.bat
    ${CMAKE_CURRENT_BINARY_DIR}/execute_graph_connected_components_all_sweeps_and_analyze.bat
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
    ${CMAKE_CURRENT_BINARY_DIR}/execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/execute_constraint_network_all_sweeps_and_analyze.bat
    ${CMAKE_CURRENT_BINARY_DIR}/execute_constraint_network_all_sweeps_and_analyze.bat
    COPYONLY
)

# ============================================================
# Summary
# ============================================================

message(STATUS "")
message(STATUS "gpu_classical_algorithms_lab configuration summary")
message(STATUS "------------------------------------------------")
message(STATUS "C++ compiler:          ${CMAKE_CXX_COMPILER}")
message(STATUS "Build type:            ${CMAKE_BUILD_TYPE}")
message(STATUS "ENABLE_CUDA:           ${ENABLE_CUDA}")
message(STATUS "GPUALGOBENCH_HAS_CUDA: ${GPUALGOBENCH_HAS_CUDA}")

if(GPUALGOBENCH_HAS_CUDA)
    message(STATUS "CUDA compiler:         ${CMAKE_CUDA_COMPILER}")
    message(STATUS "CUDA version:          ${CMAKE_CUDA_COMPILER_VERSION}")
    message(STATUS "CUDA architectures:    ${CMAKE_CUDA_ARCHITECTURES}")
endif()

message(STATUS "BUILD_TESTS:           ${BUILD_TESTS}")
message(STATUS "")
