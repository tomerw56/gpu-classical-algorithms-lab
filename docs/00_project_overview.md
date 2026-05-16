# Project Overview

This codebase is a teaching and benchmarking lab for classical algorithms on CPU and GPU.

The project intentionally avoids machine-learning examples. The focus is on workloads such as:

- Polynomial batch calculations.
- Complex cost matrix generation.
- Assignment preprocessing.
- Graph BFS and reachability.
- Connected components.
- Weighted graph relaxation.
- Constraint-network validation.
- Combination search.
- Spatial event detection.
- Local-search optimization candidate scoring.
- Scenario simulation.

The important question is not:

> Is the GPU faster?

The better question is:

> For which input sizes and algorithm shapes does GPU acceleration pay off end-to-end?

## Design rule

Every benchmark should eventually have:

- A CPU baseline.
- A GPU implementation.
- A deterministic input generator.
- A correctness comparison.
- Multiple input sizes.
- Repeated timing.
- JSONL output.
- A short Markdown explanation of strengths and pitfalls.

## Current foundation

The current codebase contains the shared benchmark infrastructure, the `foundation_smoke` benchmark, and the first real algorithm benchmark: `polynomial_batch`.

It also contains:

- `cuda_probe` for CUDA runtime diagnostics.
- `execute_all_tests.bat` for running the current test/benchmark suite with one command.

## Current implemented benchmark

`polynomial_batch` evaluates a degree-15 polynomial with 16 coefficients over many `x` values spaced by `100`. It has both CPU and CUDA implementations, tolerance-based validation, JSONL result metadata, and dedicated tests.

See `docs/polynomial_batch.md` and `docs/phase_02_polynomial_batch.md`.

## Test foundation

The test suite covers the registry, CLI, JSON writer, random utilities, device info, foundation smoke benchmark semantics, and polynomial benchmark semantics.
