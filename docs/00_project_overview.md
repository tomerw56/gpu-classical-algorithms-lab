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

The current codebase contains the shared benchmark infrastructure, the `foundation_smoke` benchmark, three real Phase 2 algorithm benchmarks (`polynomial_batch`, `cost_matrix`, and `spatial_events`), the Phase 3.1 CSR graph foundation, the Phase 3.2 `graph_bfs` traversal benchmark, and the Phase 3.3 `graph_connected_components` benchmark.

It also contains:

- `cuda_probe` for CUDA runtime diagnostics.
- `execute_all_tests.bat` for running the current test/benchmark suite with one command.
- `execute_graph_bfs_all_sweeps_and_analyze.bat` as the single BFS sweep/analyze/plot runner. It produces both the layered scale study and the chain/grid/layered/random shape × scale study.
- `export_cost_matrix` and `plot_cost_matrix.py` for inspecting cost-matrix inputs/results.
- `export_spatial_events` and `plot_spatial_events.py` for inspecting spatial-event geometry and result matrices.
- `export_graph_foundation` and `plot_graph_foundation.py` for inspecting chain, grid, layered, and random sparse CSR demo graphs.
- `export_graph_components` and `plot_graph_components.py` for inspecting connected-component labels and component sizes.
- `CsrGraph`, deterministic graph generators, validation, degree statistics, CPU/GPU BFS traversal, and CPU/GPU connected-components labeling over CSR graphs.

## Current implemented benchmarks

`polynomial_batch` evaluates a degree-15 polynomial with 16 coefficients over many `x` values spaced by `100`. It has both CPU and CUDA implementations, tolerance-based validation, JSONL result metadata, and dedicated tests.

`cost_matrix` evaluates a branch-heavy task/resource matrix with compatibility checks, dispatch-radius rejection, lateness rejection, load penalties, urgency penalties, and a small zone-proxy penalty. It has both CPU and CUDA implementations, element-wise validation, JSONL result metadata, and dedicated tests.

`spatial_events` evaluates deterministic moving track segments against circular zones, classifying `none`, `enter`, `exit`, `stay_inside`, and `cross_through` cases. It has both CPU and CUDA implementations, event-code and score validation, JSONL result metadata, dedicated tests, and an exporter/plotter pair for visual inspection.

`graph_foundation` establishes the CSR representation and deterministic chain, grid, layered, and sparse graph generators. Phase 3.1.1 adds an exporter/plotter pair so those graph shapes can be inspected. Phase 3.2 adds `graph_bfs`, a CPU queue BFS versus CUDA frontier BFS traversal benchmark over those same shapes. Phase 3.2.1 documents why this first transparent GPU BFS can lose to the CPU baseline and adds a shape-comparison runner. Phase 3.3 adds `graph_connected_components`, a CPU Union-Find versus CUDA label-propagation benchmark over disconnected graph families.

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
- `docs/graph_connected_components.md`
- `docs/phase_03_graph_connected_components.md`

## Test foundation

The test suite covers the registry, CLI, JSON writer, random utilities, device info, foundation smoke benchmark semantics, polynomial benchmark semantics, cost-matrix benchmark semantics, spatial-event benchmark semantics, CSR graph foundation semantics, BFS traversal semantics, and connected-components semantics.

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


## Connected-components scaling runner

Use `execute_graph_connected_components_all_sweeps_and_analyze.bat` to run the connected-components shape × scale study. It writes one JSONL file, an analysis report, and timing/speedup/convergence plots. See `docs/phase_03_graph_connected_components_scaling.md`.


## Appendices

- `appendix_graph_findings.md` - accumulated conclusions from the BFS and connected-components graph studies.

- `phase_03_graph_connected_components_non_naive.md` - adds the gpu-non-naive connected-components variant and explains expected tradeoffs.

- `phase_03_graph_connected_components_variant_naming.md` - explains why the executable variant `gpu` means `gpu-naive` in connected-components analysis.


## Phase 3.4: weighted graph relaxation

Adds `graph_weighted_relaxation`: CPU Dijkstra versus CUDA iterative edge relaxation. This phase demonstrates that weighted shortest-path problems are not automatically GPU-friendly; the GPU baseline exposes parallel edge work, but may do many repeated global relaxation passes.

- `phase_03_graph_weighted_relaxation_scaling.md` - weighted shortest-path sweep/analyze/plot flow.

- `phase_03_graph_weighted_relaxation_frontier.md` - active-frontier GPU shortest-path relaxation variant and interpretation.


See also: `phase_03_graph_weighted_relaxation_frontier_retrospective.md`.


## Weighted-relaxation backend policy

See `phase_03_graph_weighted_relaxation_backend_policy.md` for the current conclusion on CPU Dijkstra vs global GPU relaxation vs active-frontier GPU relaxation, including why the frontier method improved but still usually loses.


## Weighted relaxation delta-stepping experiment

The weighted-relaxation phase now includes a fourth backend row:

```text
gpu-delta-stepping
```

This is a bucketed active-relaxation experiment inspired by delta stepping. It is compared against CPU Dijkstra, the original global GPU edge scan, and the active frontier GPU variant. The analysis scripts and backend-recommendation flow now include delta-stepping columns and plots.


## Delta-stepping scheduler fix

See `docs/phase_03_graph_weighted_relaxation_delta_scheduler_fix.md` for the fix that prevents the `gpu-delta-stepping` variant from stopping early on high-diameter chain/grid-style cases. The result table was also widened so `gpu-delta-stepping` is displayed cleanly.

- `phase_03_graph_weighted_relaxation_delta_tuning.md` - delta-width tuning for the weighted-relaxation GPU delta variant.

## Weighted SSSP final attempt: `gpu-delta-light-heavy`

The weighted-relaxation chapter includes one final GPU attempt named
`gpu-delta-light-heavy`. It is a delta-stepping-style experiment that separates
light edges (`weight <= delta`) from heavy edges (`weight > delta`). It closes a
bucket with light edges first, then relaxes heavy edges outward.

This variant is intentionally treated as the final educational attempt for the
weighted shortest-path section. If it does not beat CPU Dijkstra or the simpler
`gpu` global scan, the documented conclusion is that weighted SSSP requires a
more serious GPU graph algorithm or a mature graph library rather than a small
frontier/bucket tweak.


## Final weighted-relaxation conclusion

See `docs/phase_03_graph_weighted_relaxation_final_conclusions.md` for the final conclusion of the weighted-shortest-path experiment. The canonical runner includes a very-very-large random stress point controlled by:

```bat
set "INCLUDE_VERY_VERY_LARGE_RANDOM=1"
set "VERY_VERY_LARGE_RANDOM_NODES=1048576"
set "VERY_VERY_LARGE_RANDOM_REPEAT=1"
```

The final expected policy is: CPU Dijkstra for chain/grid/layered and small random graphs; global GPU scan for sufficiently large random graphs. The frontier and delta variants remain useful educational counterexamples.


## Phase 4.1 - Constraint network

Adds `constraint_network`, a CPU/GPU benchmark for validating many candidate assignments against several constraints. See `phase_04_constraint_network.md`.


- `docs/phase_04_constraint_network_validation_fix.md` - explains the Phase 4.1 GPU validation tolerance fix for the constraint-network benchmark.


## Constraint-network diagnostics

Phase 4.1 includes a richer diagnostic plotting script:

```bat
python scripts\plot_constraint_network_diagnostics.py ^
  --jsonl results\constraint_network_scale_sweep.jsonl ^
  --output-dir results\constraint_network_diagnostics ^
  --show
```

The canonical runner also invokes this script automatically. It visualizes valid/invalid ratios, violation reasons, GPU transfer/kernel/output-copy breakdown, and CPU/GPU penalty validation error.

## Constraint-network visualization

The constraint-network phase includes both performance plots and problem-definition plots. The problem-definition exporter visualizes which task/resource pairs are compatible, which candidates are valid, and why candidates are rejected.

See `docs/phase_04_constraint_network_problem_visualization.md`.
