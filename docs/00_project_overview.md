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

The current codebase contains the shared benchmark infrastructure, the `foundation_smoke` benchmark, three real Phase 2 algorithm benchmarks (`polynomial_batch`, `cost_matrix`, and `spatial_events`), and the Phase 3.1 CSR graph foundation used by upcoming traversal benchmarks.

It also contains:

- `cuda_probe` for CUDA runtime diagnostics.
- `execute_all_tests.bat` for running the current test/benchmark suite with one command.
- `export_cost_matrix` and `plot_cost_matrix.py` for inspecting cost-matrix inputs/results.
- `export_spatial_events` and `plot_spatial_events.py` for inspecting spatial-event geometry and result matrices.
- `export_graph_foundation` and `plot_graph_foundation.py` for inspecting chain, grid, layered, and random sparse CSR demo graphs.
- `CsrGraph`, deterministic graph generators, validation, and degree statistics for graph BFS/connectivity phases.

## Current implemented benchmarks

`polynomial_batch` evaluates a degree-15 polynomial with 16 coefficients over many `x` values spaced by `100`. It has both CPU and CUDA implementations, tolerance-based validation, JSONL result metadata, and dedicated tests.

`cost_matrix` evaluates a branch-heavy task/resource matrix with compatibility checks, dispatch-radius rejection, lateness rejection, load penalties, urgency penalties, and a small zone-proxy penalty. It has both CPU and CUDA implementations, element-wise validation, JSONL result metadata, and dedicated tests.

`spatial_events` evaluates deterministic moving track segments against circular zones, classifying `none`, `enter`, `exit`, `stay_inside`, and `cross_through` cases. It has both CPU and CUDA implementations, event-code and score validation, JSONL result metadata, dedicated tests, and an exporter/plotter pair for visual inspection.

`graph_foundation` is not a benchmark yet. It establishes the CSR representation and deterministic chain, grid, layered, and sparse graph generators that the upcoming BFS and connectivity benchmarks will share. Phase 3.1.1 adds an exporter/plotter pair so those graph shapes can be inspected before traversal timing begins.

See:

- `docs/polynomial_batch.md`
- `docs/phase_02_polynomial_batch.md`
- `docs/cost_matrix.md`
- `docs/phase_02_cost_matrix.md`
- `docs/spatial_events.md`
- `docs/phase_02_spatial_events.md`
- `docs/graph_foundation.md`
- `docs/phase_03_graph_foundation.md`
- `docs/phase_03_graph_visualization.md`

## Test foundation

The test suite covers the registry, CLI, JSON writer, random utilities, device info, foundation smoke benchmark semantics, polynomial benchmark semantics, cost-matrix benchmark semantics, spatial-event benchmark semantics, and CSR graph foundation semantics.
