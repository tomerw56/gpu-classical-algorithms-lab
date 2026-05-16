# Phase 2: Polynomial Batch Benchmark

Phase 2 begins the actual algorithm demonstrations. The first implemented benchmark is:

```text
polynomial_batch
```

It evaluates a degree-15 polynomial with 16 coefficients over many `x` values spaced by `100`.

## Why this benchmark first?

It is a clean bridge from Phase 1 infrastructure into real GPU work:

- the CPU baseline is obvious;
- the GPU mapping is obvious;
- correctness can be checked rigorously;
- the workload is arithmetic-heavy enough to show a meaningful GPU effect;
- the benchmark naturally teaches the difference between kernel time and end-to-end time.

## Files added

```text
include/polynomial/polynomial_batch.hpp
src/polynomial/polynomial_batch_cpu.cpp
src/polynomial/polynomial_batch_gpu.cu
tests/test_polynomial.cpp
docs/polynomial_batch.md
```

## Integration points updated

```text
CMakeLists.txt
src/common/benchmark_registry.cpp
README.md
docs/00_project_overview.md
docs/01_benchmarking_methodology.md
docs/03_results_format.md
docs/phase_01_tests.md
```

The shared executables automatically pick up the new benchmark:

```text
gpu_algobench --list
validate_all
execute_all_tests.bat
scripts/run_all_benchmarks.py
```

## Validation

The new CTest executable:

```text
test_polynomial
```

checks:

- preset sizes;
- the 16-coefficient layout;
- the stride-100 x-domain cycle;
- Horner evaluation against a direct polynomial sum for a representative point;
- CPU benchmark metadata and correctness;
- repeat semantics, ensuring reported checksums do not scale with `--repeat`.
