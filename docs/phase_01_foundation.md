# Phase 1 — Foundation

Phase 1 provides the benchmark skeleton.

## Delivered components

- `gpu_algobench`: main CLI runner.
- `list_benchmarks`: small helper that lists benchmarks.
- `validate_all`: runs tiny validation-oriented benchmark settings.
- `cuda_probe`: prints CUDA runtime/device diagnostic information.
- `execute_all_tests.bat`: runs the current test/benchmark suite from one command.
- `BenchmarkRegistry`: maps benchmark names to executable functions.
- `BenchmarkConfig`: shared command-line configuration.
- `BenchmarkResult`: shared output record.
- `JsonWriter`: writes JSONL output.
- `CpuTimer`: simple steady-clock timer.
- `cuda_check.cuh`: CUDA error-checking macro for later `.cu` files.
- `foundation_smoke`: tiny infrastructure test benchmark.
- Infrastructure CTest suite: registry, CLI, JSONL writer, random utilities, device info, and foundation semantics.

## Current benchmark

`foundation_smoke` performs a deterministic vector transform and validates a checksum.

The checksum has a strict policy: it represents the final single output vector, not an accumulated value across `--repeat`. Repeats are for timing only. This keeps CPU and GPU variants comparable: they run the same logical transform and report the same validation meaning.

It is not meant to prove GPU performance. It proves the benchmark runner, result format, correctness policy, and build system work.

## Current verification command

After building, run:

```bat
execute_all_tests.bat
```

The script lists the benchmarks, runs CUDA diagnostics, prints discovered CTest tests, runs CTest, runs `validate_all`, and runs all registered benchmarks with the preset/repeat settings defined at the top of the script.

## Next phase

Phase 2 should add the first real algorithm demos:

1. Polynomial batch evaluation.
2. Complex cost matrix generation.
3. Spatial event detection.

## Phase 1.3 test coverage

Phase 1.3 adds multiple CTest executables instead of relying on a single smoke test. This makes failures easier to localize and prepares the repo for adding real algorithm benchmarks. See `phase_01_tests.md`.
