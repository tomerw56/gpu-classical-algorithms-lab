# Phase 1.3 — Infrastructure Tests

Phase 1.3 expands the CTest suite around the benchmark infrastructure.

The tests are intentionally lightweight and dependency-free. They use a small local assertion helper in `tests/test_support.hpp` rather than a full testing framework. This keeps the CUDA/MSVC/Ninja workflow simple while the project foundation is still changing.

## Test executables

CTest currently runs these executables:

| Test | Purpose |
|---|---|
| `test_foundation` | Checks `foundation_smoke` presets, CPU result metadata, correctness, and repeat/checksum policy. |
| `test_polynomial` | Checks polynomial presets, 16-coefficient layout, stride-100 x policy, Horner correctness, benchmark metadata, and repeat/checksum policy. |
| `test_cost_matrix` | Checks cost-matrix presets, deterministic task/resource generation, forced feasible/infeasible pairs, result metadata, and repeat/checksum policy. |
| `test_registry` | Checks default benchmark registration, `all`, named benchmark execution, and unknown benchmark errors. |
| `test_cli` | Checks command-line parsing, flags, repeated `--set`, and invalid argument handling. |
| `test_json_writer` | Checks JSONL serialization, escaping, append/truncate behavior, and table printing. |
| `test_random_utils` | Checks deterministic seeded random vectors and bounds. |
| `test_device_info` | Checks CPU device string and CUDA runtime-info behavior in both CPU-only and CUDA builds. |

## Running tests

From the build directory:

```bat
ctest --output-on-failure
```

From the repository root, use the wrapper:

```bat
execute_all_tests.bat
```

The wrapper now prints the discovered CTest tests first:

```bat
ctest --test-dir "<build-dir>" -N
```

Then it runs them with verbose output:

```bat
ctest --test-dir "<build-dir>" --output-on-failure --verbose
```

## Correctness policy covered by tests

The foundation, polynomial, and cost-matrix tests verify that `--repeat` is a timing setting only. The reported checksum must describe the final single output vector, not the sum of all repeated executions.

This matters because CPU and GPU benchmark rows must describe the same logical computation. Different checksum semantics would make them different benchmarks.

## Future testing direction

When the real algorithm benchmarks are added, each one should include:

1. Tiny deterministic correctness cases.
2. CPU/GPU equivalence checks.
3. Bad-input tests.
4. Preset-size tests.
5. Benchmark-result metadata tests.

For now, the local assertion helper is enough. Later, we can move to Catch2 or GoogleTest if the test suite grows significantly.


## Exporter smoke coverage

Phase 2.2 adds a small CTest entry named `export_cost_matrix_smoke`. It runs the CSV exporter on a `4 x 5` matrix and verifies through process success that the inspectable export path remains wired into the build.
