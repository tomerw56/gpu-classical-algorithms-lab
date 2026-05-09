# Phase 1 — Foundation

Phase 1 provides the benchmark skeleton.

## Delivered components

- `gpu_algobench`: main CLI runner.
- `list_benchmarks`: small helper that lists benchmarks.
- `validate_all`: runs tiny validation-oriented benchmark settings.
- `BenchmarkRegistry`: maps benchmark names to executable functions.
- `BenchmarkConfig`: shared command-line configuration.
- `BenchmarkResult`: shared output record.
- `JsonWriter`: writes JSONL output.
- `CpuTimer`: simple steady-clock timer.
- `cuda_check.cuh`: CUDA error-checking macro for later `.cu` files.
- `foundation_smoke`: tiny infrastructure test benchmark.

## Current benchmark

`foundation_smoke` performs a deterministic vector transform and checksum.

It is not meant to prove GPU performance. It proves the benchmark runner, result format, and build system work.

## Next phase

Phase 2 should add the first real algorithm demos:

1. Polynomial batch evaluation.
2. Complex cost matrix generation.
3. Spatial event detection.
