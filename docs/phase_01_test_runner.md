# Phase 1.1 — Execute-All Test Runner

Phase 1.1 adds a single Windows batch entry point for running the current executable tests and benchmark smoke run.

The goal is to make local verification repeatable before adding real algorithm demos.

## Script

Root-level script:

```text
execute_all_tests.bat
```

CMake also copies the script into the build directory:

```text
build-cuda-ninja/execute_all_tests.bat
```

The repository-root copy is the canonical one.

## Fixed variables

The top of the script contains the settings you are expected to change:

```bat
set "BUILD_DIR=build-cuda-ninja"
set "BENCHMARK_NAME=all"
set "BENCHMARK_PRESET=small"
set "BENCHMARK_REPEAT=5"
set "BENCHMARK_WARMUP=1"
set "RESULTS_DIR=results"
set "RESULTS_FILE=execute_all_tests.jsonl"
set "RUN_GPU=1"
set "OVERWRITE_RESULTS=1"
```

This lets us change the input size/preset and repeat count in one place.

## What it runs

The script prints every command before executing it. It also prints the discovered CTest tests before running them.

Current sequence:

1. `list_benchmarks`
2. `cuda_probe`, if present
3. `ctest --test-dir <build-dir> -N` to print discovered tests, then `ctest --test-dir <build-dir> --output-on-failure --verbose`
4. `validate_all`
5. `gpu_algobench --benchmark all --preset <preset> --repeat <repeat> --warmup <warmup>`

## Output

Benchmark results are written as JSON Lines:

```text
results/execute_all_tests.jsonl
```

The default behavior is to delete the old result file before running:

```bat
set "OVERWRITE_RESULTS=1"
```

Set it to `0` to append results across runs.

## Why this matters

As the project grows, this script becomes the quick sanity command:

```bat
execute_all_tests.bat
```

Before adding a new benchmark, the script should pass.
After adding a new benchmark, the script should include it automatically when `BENCHMARK_NAME=all`.

## Pitfalls

- The script assumes the project was built first.
- By default it expects the Ninja build directory to be `build-cuda-ninja`.
- Directory variables in the script are normalized without a trailing backslash. This avoids Windows command-line parsing issues such as `ctest --test-dir "path\" --output-on-failure` being interpreted as one malformed path argument.
- If the binary was built with CUDA but the runtime cannot access the GPU, the benchmark may fail validation.
- For CPU-only runs, set `RUN_GPU=0` or build with `-DENABLE_CUDA=OFF`.

## Current CTest coverage

The CTest suite currently includes:

- `test_foundation`
- `test_registry`
- `test_cli`
- `test_json_writer`
- `test_random_utils`
- `test_device_info`

These are documented in `docs/phase_01_tests.md`.
