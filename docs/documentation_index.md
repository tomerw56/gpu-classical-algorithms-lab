# Documentation index

This repository is a benchmark-and-teaching lab for classical algorithms on CPU versus CUDA GPU. The documentation is now organized around **how a reader should enter the project**, not around the order in which the files were created.

## 1. Start here

| File | Purpose |
|---|---|
| `README.md` | Build/run quickstart and top-level project status. |
| `docs/00_project_overview.md` | Repository structure, benchmark philosophy, and phase map. |
| `docs/documentation_index.md` | This file; the ordered documentation map. |
| `docs/literature_and_problem_definitions.md` | Source-backed problem definitions and reading list for all featured benchmark families. |
| `docs/01_benchmarking_methodology.md` | How CPU/GPU timings, repeats, JSONL, and correctness checks are interpreted. |
| `docs/02_gpu_pitfalls.md` | Common failure modes: transfer overhead, launch overhead, atomics, divergence, irregular graph work. |
| `docs/03_results_format.md` | JSONL schema and metadata fields used by analyzers/plotters. |

## 2. Build, test, and infrastructure

| File | Purpose |
|---|---|
| `docs/phase_01_foundation.md` | Initial benchmark framework and smoke benchmark. |
| `docs/phase_01_cuda_runtime_diagnostics.md` | CUDA runtime/device diagnostics and common Windows/CUDA pitfalls. |
| `docs/phase_01_test_runner.md` | General `execute_all_tests.bat` workflow. |
| `docs/phase_01_tests.md` | Current CTest coverage and exporter smoke tests. |

Primary build command on Tomer's Windows CUDA setup:

```bat
configure_ninja_cuda128.bat
```

General test command:

```bat
execute_all_tests.bat
```

## 3. Phase 2: dense data-parallel benchmarks

These are the cleanest positive GPU examples because the work is mostly independent and regular.

| Benchmark | Main docs | Visualization/export docs |
|---|---|---|
| `polynomial_batch` | `docs/polynomial_batch.md`, `docs/phase_02_polynomial_batch.md` | none needed; numeric scaling benchmark |
| `cost_matrix` | `docs/cost_matrix.md`, `docs/phase_02_cost_matrix.md` | `export_cost_matrix`, `scripts/plot_cost_matrix.py` described in `docs/cost_matrix.md` |
| `spatial_events` | `docs/spatial_events.md`, `docs/phase_02_spatial_events.md` | `export_spatial_events`, `scripts/plot_spatial_events.py` described in `docs/spatial_events.md` |

## 4. Phase 3: graph benchmarks and graph lessons

The graph phase is the main "GPU is not magic" section. It compares graph anatomy, frontier width, convergence rounds, and CPU baseline strength.

### Graph foundation

| File | Purpose |
|---|---|
| `docs/graph_foundation.md` | CSR representation and deterministic graph generators. |
| `docs/phase_03_graph_foundation.md` | Phase summary for CSR foundation. |
| `docs/phase_03_graph_visualization.md` | Graph exporter and visual plots for chain/grid/layered/random shapes. |

### BFS / reachability

| File | Purpose |
|---|---|
| `docs/graph_bfs.md` | CPU queue BFS vs CUDA frontier BFS. |
| `docs/phase_03_graph_bfs.md` | Benchmark implementation notes. |
| `docs/phase_03_graph_bfs_analysis.md` | Why naive GPU BFS loses on many graph shapes. |
| `docs/phase_03_graph_bfs_scaling.md` | Layered-graph scaling sweep. |
| `docs/phase_03_graph_bfs_shape_scaling.md` | Shape × scale sweep: chain/grid/layered/random. |
| `docs/phase_03_graph_bfs_frontier_anatomy.md` | Frontier depth/width and useful work per synchronization point. |
| `docs/phase_03_graph_bfs_jsonl_diagnostics.md` | JSONL analyzer for suspicious or stale BFS plots. |
| `docs/phase_03_graph_bfs_single_runner.md` | The single canonical BFS sweep/analyze/plot runner. |

Canonical command:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

### Connected components

| File | Purpose |
|---|---|
| `docs/graph_connected_components.md` | CPU Union-Find, naive GPU label propagation, and non-naive GPU variant. |
| `docs/phase_03_graph_connected_components.md` | Phase summary and implementation notes. |
| `docs/phase_03_graph_connected_components_scaling.md` | Shape × scale sweep, plots, and crossover discussion. |
| `docs/phase_03_graph_connected_components_non_naive.md` | `gpu-non-naive` root-hooking + pointer-jumping experiment. |
| `docs/phase_03_graph_connected_components_variant_naming.md` | Clarifies `gpu` = naive and `gpu-non-naive` = improved variant. |

Canonical command:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

### Weighted graph relaxation / shortest paths

| File | Purpose |
|---|---|
| `docs/graph_weighted_relaxation.md` | CPU Dijkstra versus multiple GPU relaxation attempts. |
| `docs/phase_03_graph_weighted_relaxation.md` | Initial weighted relaxation benchmark. |
| `docs/phase_03_graph_weighted_relaxation_scaling.md` | Shape × scale weighted sweep. |
| `docs/phase_03_graph_weighted_relaxation_backend_policy.md` | Backend recommendation policy. |
| `docs/phase_03_graph_weighted_relaxation_frontier.md` | Active-frontier GPU attempt. |
| `docs/phase_03_graph_weighted_relaxation_frontier_retrospective.md` | Why first frontier attempt was worse. |
| `docs/phase_03_graph_weighted_relaxation_delta_stepping.md` | Simplified delta-stepping attempt. |
| `docs/phase_03_graph_weighted_relaxation_delta_scheduler_fix.md` | Correctness/scheduler fix for delta variant. |
| `docs/phase_03_graph_weighted_relaxation_delta_tuning.md` | Delta-width tuning results. |
| `docs/phase_03_graph_weighted_relaxation_light_heavy_delta.md` | Final light/heavy delta attempt. |
| `docs/phase_03_graph_weighted_relaxation_final_conclusions.md` | Final weighted-shortest-path conclusion. |

Canonical command:

```bat
execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
```

### Graph conclusion appendix

| File | Purpose |
|---|---|
| `docs/appendix_graph_findings.md` | High-level conclusions from BFS, connected components, and weighted shortest paths. |

## 5. Phase 4: optimization-support workloads

These benchmarks are closer to practical algorithm departments: candidate scoring, constraint validation, and combinatorial enumeration.

### Constraint network / candidate validation

| File | Purpose |
|---|---|
| `docs/constraint_network.md` | Problem definition and CPU/GPU candidate validation. |
| `docs/phase_04_constraint_network.md` | Benchmark implementation and interpretation. |
| `docs/phase_04_constraint_network_validation_fix.md` | Floating-point validation tolerance note. |
| `docs/phase_04_constraint_network_diagnostics.md` | Diagnostic plots: validity ratio, violation reasons, timing breakdown. |
| `docs/phase_04_constraint_network_problem_visualization.md` | Problem-definition exporter and plots: task-resource feasibility matrices. |

Canonical command:

```bat
execute_constraint_network_all_sweeps_and_analyze.bat
```

### Combination finder / candidate enumeration

| File | Purpose |
|---|---|
| `docs/phase_04_combination_finder.md` | CPU/GPU benchmark for evaluating k-combinations. |
| `docs/phase_04_combination_problem_definitions.md` | Source-backed combination/exhaustive-search definitions. |
| `docs/literature_and_problem_definitions.md` | Broader source-backed reading list including this benchmark. |

Canonical command:

```bat
execute_combination_finder_all_sweeps_and_analyze.bat
```

## 6. Current live-demo commands by topic

| Topic | Command |
|---|---|
| Build CUDA/Ninja | `configure_ninja_cuda128.bat` |
| General smoke/tests | `execute_all_tests.bat` |
| BFS all sweeps | `execute_graph_bfs_all_sweeps_and_analyze.bat` |
| Connected components all sweeps | `execute_graph_connected_components_all_sweeps_and_analyze.bat` |
| Weighted relaxation all sweeps | `execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat` |
| Constraint network all sweeps | `execute_constraint_network_all_sweeps_and_analyze.bat` |
| Combination finder all sweeps | `execute_combination_finder_all_sweeps_and_analyze.bat` |

## 7. Recommended reading order for lecture preparation

1. `docs/literature_and_problem_definitions.md`
2. `docs/01_benchmarking_methodology.md`
3. `docs/02_gpu_pitfalls.md`
4. `docs/polynomial_batch.md`
5. `docs/cost_matrix.md`
6. `docs/spatial_events.md`
7. `docs/graph_bfs.md`
8. `docs/appendix_graph_findings.md`
9. `docs/constraint_network.md`
10. `docs/phase_04_combination_finder.md`

This order tells the story cleanly: simple dense parallel wins, graph counterexamples, improved graph variants, and then practical optimization-support workloads.
