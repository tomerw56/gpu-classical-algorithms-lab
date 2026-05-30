# Documentation index

This repository is a benchmark-and-teaching lab for classical algorithms on CPU versus CUDA GPU. The index is organized for a reader preparing a lecture or reviewing the project, not by the order in which the files were created.

## 1. Start here

| File | Purpose |
|---|---|
| `../README.md` | Build/run quickstart and top-level project status. |
| `docs/00_project_overview.md` | Repository structure, benchmark philosophy, and phase map. |
| `docs/documentation_index.md` | This file; the ordered documentation map. |
| `docs/literature_and_problem_definitions.md` | Source-backed problem definitions and reading list for all benchmark families. |
| `docs/01_benchmarking_methodology.md` | How CPU/GPU timings, repeats, JSONL, and correctness checks are interpreted. |
| `docs/02_gpu_pitfalls.md` | Transfer overhead, launch overhead, atomics, divergence, irregular graph work. |
| `docs/03_results_format.md` | JSONL schema and metadata fields used by analyzers/plotters. |

## 2. Build, test, and infrastructure

| File | Purpose |
|---|---|
| `docs/phase_01_foundation.md` | Initial benchmark framework and smoke benchmark. |
| `docs/phase_01_cuda_runtime_diagnostics.md` | CUDA runtime/device diagnostics and Windows/CUDA notes. |
| `docs/phase_01_test_runner.md` | General `execute_all_tests.bat` workflow. |
| `docs/phase_01_tests.md` | Current CTest coverage and exporter smoke tests. |

Primary build command on the Windows CUDA setup:

```bat
configure_ninja_cuda128.bat
```

General validation command:

```bat
execute_all_tests.bat
```

## 3. Phase 2: dense data-parallel benchmarks

These are the cleanest positive GPU examples because the work is mostly independent and regular.

| Benchmark | Main docs | Visualization/export docs |
|---|---|---|
| `polynomial_batch` | `docs/polynomial_batch.md`, `docs/phase_02_polynomial_batch.md` | Numeric scaling benchmark; no separate exporter needed. |
| `cost_matrix` | `docs/cost_matrix.md`, `docs/phase_02_cost_matrix.md` | `export_cost_matrix`, `scripts/plot_cost_matrix.py`. |
| `spatial_events` | `docs/spatial_events.md`, `docs/phase_02_spatial_events.md` | `export_spatial_events`, `scripts/plot_spatial_events.py`. |

## 4. Phase 3: graph benchmarks and graph lessons

The graph phase is the main "GPU is not magic" section. It compares graph anatomy, frontier width, convergence rounds, and CPU baseline strength.

### Graph foundation

| File | Purpose |
|---|---|
| `docs/graph_foundation.md` | CSR representation and deterministic graph generators. |
| `docs/phase_03_graph_foundation.md` | Phase summary for CSR foundation. |
| `docs/phase_03_graph_visualization.md` | Graph exporter and plots for chain/grid/layered/random shapes. |

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
| `docs/graph_connected_components.md` | CPU Union-Find vs GPU label-propagation variants. |
| `docs/phase_03_graph_connected_components.md` | Phase summary and graph-family discussion. |
| `docs/phase_03_graph_connected_components_scaling.md` | Shape × scale runner, analysis, and plots. |
| `docs/phase_03_graph_connected_components_non_naive.md` | Non-naive GPU variant: root hooking + pointer jumping. |
| `docs/phase_03_graph_connected_components_variant_naming.md` | Clarifies `gpu` as naive and `gpu-non-naive` as the improved variant. |

Canonical command:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

### Weighted graph relaxation / shortest paths

| File | Purpose |
|---|---|
| `docs/graph_weighted_relaxation.md` | CPU Dijkstra vs several GPU relaxation attempts. |
| `docs/phase_03_graph_weighted_relaxation.md` | Phase implementation summary. |
| `docs/phase_03_graph_weighted_relaxation_scaling.md` | Shape × scale sweep and plots. |
| `docs/phase_03_graph_weighted_relaxation_backend_policy.md` | Backend recommendation logic. |
| `docs/phase_03_graph_weighted_relaxation_frontier.md` | Active-frontier GPU attempt. |
| `docs/phase_03_graph_weighted_relaxation_frontier_retrospective.md` | Why the first frontier implementation got worse. |
| `docs/phase_03_graph_weighted_relaxation_delta_stepping.md` | Delta-stepping inspired attempt. |
| `docs/phase_03_graph_weighted_relaxation_delta_tuning.md` | Delta-width tuning. |
| `docs/phase_03_graph_weighted_relaxation_light_heavy_delta.md` | Final light/heavy delta attempt. |
| `docs/phase_03_graph_weighted_relaxation_final_conclusions.md` | Final weighted-shortest-path conclusion. |

Canonical command:

```bat
execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
```

Graph conclusion appendix:

```text
docs/appendix_graph_findings.md
```

## 5. Phase 4: optimization-support workloads

These phases are the practical hybrid CPU/GPU story: GPU scores, filters, validates, or simulates many independent candidates while CPU/solver logic remains in control.

Overview:

```text
docs/phase_04_overview.md
```

### Constraint network / candidate validation

| File | Purpose |
|---|---|
| `docs/constraint_network.md` | Problem definition and CPU/GPU candidate validation. |
| `docs/phase_04_constraint_network.md` | Benchmark implementation and interpretation. |
| `docs/phase_04_constraint_network_validation_fix.md` | Floating-point validation tolerance note. |
| `docs/phase_04_constraint_network_diagnostics.md` | Diagnostic plots: validity ratio, violation reasons, timing breakdown. |
| `docs/phase_04_constraint_network_problem_visualization.md` | Problem-definition exporter and task-resource feasibility matrices. |

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

### Assignment preprocessing

| File | Purpose |
|---|---|
| `docs/assignment_preprocessing.md` | Task/resource feasibility, cost scoring, and per-task top-K reduction. |
| `docs/phase_04_assignment_preprocessing.md` | Phase narrative, benchmark interpretation, canonical runner, and output files. |
| `docs/literature_and_problem_definitions.md` | Source-backed assignment/Hungarian/top-K definitions. |

Canonical command:

```bat
execute_assignment_preprocessing_all_sweeps_and_analyze.bat
```

Plotting-only command:

```bat
execute_assignment_preprocessing_plots.bat
```

### Local-search move evaluation

| File | Purpose |
|---|---|
| `docs/local_search_moves.md` | Benchmark definition and CPU/GPU mapping. |
| `docs/phase_04_local_search_moves.md` | Runner, outputs, and interpretation. |
| `docs/phase_04_local_search_moves_speedup_plateau.md` | Why speedup rises and then stabilizes at throughput plateau. |

Canonical command:

```bat
execute_local_search_moves_all_sweeps_and_analyze.bat
```

Plotting-only command:

```bat
execute_local_search_moves_plots.bat
```

### Scenario simulation / robust planning

| File | Purpose |
|---|---|
| `docs/scenario_simulation.md` | Problem definition and CPU/GPU mapping. |
| `docs/phase_04_scenario_simulation.md` | Runner, outputs, and interpretation. |
| `docs/phase_04_scenario_simulation_sweep_trim.md` | Explains why the default sweep stops at `sc_4m`. |
| `docs/phase_04_scenario_simulation_feasibility_calibration.md` | Correctness vs feasibility semantics. |
| `docs/phase_04_scenario_simulation_feasibility_calibration_v2.md` | Second calibration pass and mixed feasible/infeasible results. |

Canonical command:

```bat
execute_scenario_simulation_all_sweeps_and_analyze.bat
```

Plotting-only command:

```bat
execute_scenario_simulation_plots.bat
```

## 6. Final lecture/report package

Use these files when presenting or preparing the final internal lecture:

| File | Purpose |
|---|---|
| `docs/lecture_packaging.md` | Lecture flow, narrative, and recommended demo sequence. |
| `docs/live_demo_script.md` | What to run and what to say during the demo. |
| `docs/lecture_command_cheatsheet.md` | Compact command list for live usage. |
| `docs/final_benchmark_conclusions.md` | Final conclusions across all workload families. |
| `docs/gpu_decision_guide.md` | CPU vs GPU vs hybrid decision checklist. |
| `docs/final_report_outline.md` | Outline for a written internal report. |
| `docs/lecture_demo_runner_fix.md` | Batch-runner fix for the curated lecture demo. |
| `docs/documentation_review_notes.md` | Notes from the final documentation cleanup pass. |

Canonical short demo command:

```bat
execute_lecture_demo_core.bat
```

## 7. Current live-demo commands by topic

| Topic | Command |
|---|---|
| Build CUDA/Ninja | `configure_ninja_cuda128.bat` |
| General smoke/tests | `execute_all_tests.bat` |
| Curated lecture demo | `execute_lecture_demo_core.bat` |
| BFS all sweeps | `execute_graph_bfs_all_sweeps_and_analyze.bat` |
| Connected components all sweeps | `execute_graph_connected_components_all_sweeps_and_analyze.bat` |
| Weighted relaxation all sweeps | `execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat` |
| Constraint network all sweeps | `execute_constraint_network_all_sweeps_and_analyze.bat` |
| Combination finder all sweeps | `execute_combination_finder_all_sweeps_and_analyze.bat` |
| Assignment preprocessing all sweeps | `execute_assignment_preprocessing_all_sweeps_and_analyze.bat` |
| Assignment preprocessing plots only | `execute_assignment_preprocessing_plots.bat` |
| Local-search all sweeps | `execute_local_search_moves_all_sweeps_and_analyze.bat` |
| Local-search plots only | `execute_local_search_moves_plots.bat` |
| Scenario simulation all sweeps | `execute_scenario_simulation_all_sweeps_and_analyze.bat` |
| Scenario simulation plots only | `execute_scenario_simulation_plots.bat` |

## 8. Recommended reading paths

### Fast lecture prep

1. `docs/lecture_packaging.md`
2. `docs/live_demo_script.md`
3. `docs/final_benchmark_conclusions.md`
4. `docs/gpu_decision_guide.md`
5. `docs/lecture_command_cheatsheet.md`

### Technical review

1. `docs/literature_and_problem_definitions.md`
2. `docs/01_benchmarking_methodology.md`
3. `docs/02_gpu_pitfalls.md`
4. `docs/03_results_format.md`
5. Benchmark-specific docs for the phase under review.

### Full story

1. Dense data-parallel wins: polynomial, cost matrix, spatial events.
2. Graph counterexamples and nuance: BFS, connected components, weighted shortest paths.
3. Optimization-support workloads: constraint validation, combinations, assignment preprocessing, local search, scenario simulation.
4. Final decision guide and conclusions.
