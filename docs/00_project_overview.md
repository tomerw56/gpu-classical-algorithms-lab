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

The current codebase contains the shared benchmark infrastructure, the `foundation_smoke` benchmark, three real Phase 2 algorithm benchmarks (`polynomial_batch`, `cost_matrix`, and `spatial_events`), the Phase 3.1 CSR graph foundation, and the Phase 3.2 `graph_bfs` traversal benchmark.

It also contains:

- `cuda_probe` for CUDA runtime diagnostics.
- `execute_all_tests.bat` for running the current test/benchmark suite with one command.
- `execute_graph_bfs_all_sweeps_and_analyze.bat` as the single BFS sweep/analyze/plot runner. It produces both the layered scale study and the chain/grid/layered/random shape × scale study.
- `export_cost_matrix` and `plot_cost_matrix.py` for inspecting cost-matrix inputs/results.
- `export_spatial_events` and `plot_spatial_events.py` for inspecting spatial-event geometry and result matrices.
- `export_graph_foundation` and `plot_graph_foundation.py` for inspecting chain, grid, layered, and random sparse CSR demo graphs.
- `CsrGraph`, deterministic graph generators, validation, degree statistics, and CPU/GPU BFS traversal over those graph shapes.

## Current implemented benchmarks

`polynomial_batch` evaluates a degree-15 polynomial with 16 coefficients over many `x` values spaced by `100`. It has both CPU and CUDA implementations, tolerance-based validation, JSONL result metadata, and dedicated tests.

`cost_matrix` evaluates a branch-heavy task/resource matrix with compatibility checks, dispatch-radius rejection, lateness rejection, load penalties, urgency penalties, and a small zone-proxy penalty. It has both CPU and CUDA implementations, element-wise validation, JSONL result metadata, and dedicated tests.

`spatial_events` evaluates deterministic moving track segments against circular zones, classifying `none`, `enter`, `exit`, `stay_inside`, and `cross_through` cases. It has both CPU and CUDA implementations, event-code and score validation, JSONL result metadata, dedicated tests, and an exporter/plotter pair for visual inspection.

`graph_foundation` establishes the CSR representation and deterministic chain, grid, layered, and sparse graph generators. Phase 3.1.1 adds an exporter/plotter pair so those graph shapes can be inspected. Phase 3.2 adds `graph_bfs`, a CPU queue BFS versus CUDA frontier BFS traversal benchmark over those same shapes. Phase 3.2.1 documents why this first transparent GPU BFS can lose to the CPU baseline and adds a shape-comparison runner.

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
- `docs/graph_bfs.md`
- `docs/phase_03_graph_bfs.md`
- `docs/phase_03_graph_bfs_analysis.md`
- `docs/phase_03_graph_bfs_shape_scaling.md`
- `docs/phase_03_graph_bfs_frontier_anatomy.md`
- `docs/phase_03_graph_bfs_single_runner.md`

## Test foundation

The test suite covers the registry, CLI, JSON writer, random utilities, device info, foundation smoke benchmark semantics, polynomial benchmark semantics, cost-matrix benchmark semantics, spatial-event benchmark semantics, CSR graph foundation semantics, and BFS traversal semantics.

## Current graph-BFS follow-up

Phase 3.2.2 adds `execute_graph_bfs_all_sweeps_and_analyze.bat` and `scripts/plot_graph_bfs_scaling.py` so the layered BFS case can be measured from tiny graphs through a very large graph and plotted as both runtime and CPU/GPU speedup curves.

Phase 3.2.3 adds `execute_graph_bfs_all_sweeps_and_analyze.bat` and `scripts/plot_graph_bfs_shape_scaling.py` so chain, grid, layered, and random graph families can be swept across size ranges and compared on the same CPU/GPU speedup chart.

Phase 3.2.4 adds BFS frontier-anatomy metadata and plots. `graph_bfs` now reports depth, max frontier size, mean frontier size, reached edge visits, and average edge work per BFS level. The shape-scaling plotter uses those fields to explain why the current GPU BFS remains bad on chain/grid graphs but becomes strong on sufficiently large low-diameter random graphs.

## Graph BFS diagnostics flow

Phase 3.2.6 adds `execute_graph_bfs_all_sweeps_and_analyze.bat` and `scripts/analyze_graph_bfs_jsonl.py`. Use this flow whenever a BFS scaling plot looks empty or suspicious. It verifies that the JSONL contains matched CPU/GPU rows and reports whether frontier metrics are exact or fallback estimates.


## Canonical BFS runner

Use a single batch file for all BFS sweep, analysis, and plot generation:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

This script is self-contained. It runs the layered-only sweep, the chain/grid/layered/random shape × scale sweep, the JSONL analyzer, and both plotters. Older split `execute_graph_bfs*.bat` wrappers were retired to avoid duplicated logic and batch-label errors.
