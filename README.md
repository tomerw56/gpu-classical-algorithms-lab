# GPU Classical Algorithms Lab

A C++/CUDA benchmark playground for testing where GPU acceleration helps classical algorithmic workloads: graph theory, constraint checking, cost matrices, spatial events, combinations, local search, and scenario simulation.

This repository is currently in **Phase 3.4: weighted graph relaxation**, after the Phase 1 infrastructure and the three Phase 2 CPU/GPU pairwise/data-parallel benchmarks.

The current codebase includes:

- CMake build with optional CUDA.
- Ninja-focused Windows CUDA workflow.
- CPU-only build path.
- Shared benchmark registry.
- Shared benchmark result format.
- JSON Lines output for later plotting.
- Basic CLI runner.
- CPU timer and CUDA timing hooks.
- Deterministic random utilities.
- A small `foundation_smoke` benchmark to verify the infrastructure.
- `cuda_probe` diagnostic executable.
- `execute_all_tests.bat` to run the current test/benchmark suite from one command.
- `execute_graph_bfs_all_sweeps_and_analyze.bat`, the single canonical BFS sweep/analyze/plot runner. It runs both the layered scale study and the chain/grid/layered/random shape × scale study.
- `graph_connected_components`: CPU Union-Find versus CUDA iterative label propagation over disconnected CSR graph families.
- `execute_graph_connected_components_all_sweeps_and_analyze.bat`, the single canonical connected-components sweep/analyze/plot runner. It runs chains, grids, random clusters, and mixed components across increasing sizes.
- `export_graph_components`: inspectable CSV exporter used by the Python connected-components plotting utility.

### Graph BFS sweep/analyze/plot flow

For all BFS scaling work, use the single canonical runner:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

It generates both JSONL files:

```text
results\graph_bfs_layered_scale_sweep.jsonl
results\graph_bfs_shape_scale_sweep.jsonl
```

Then it analyzes both files and writes both plot sets. The shape × scale file is the one that includes all graph anatomies:

```text
chain, grid, layered, random
```

If a BFS anatomy plot looks empty or suspicious, rerun the same script. It runs the analyzer automatically and writes reports under:

```text
results\graph_bfs_layered_scale_analysis\
results\graph_bfs_shape_scale_analysis\
```

Older split BFS batch wrappers were intentionally retired so there is only one batch command to remember.

- Frontier-anatomy BFS metadata and plots: depth, max frontier size, mean edge work per BFS level, and speedup versus frontier work.
- CTest coverage for the registry, CLI, JSONL writer, random utilities, device info, foundation benchmark semantics, polynomial, cost-matrix, spatial-event, graph-foundation, graph-BFS, and graph-connected-components semantics.
- `polynomial_batch`: degree-15 polynomial evaluation over many stride-100 `x` values, with CPU and CUDA implementations.
- `cost_matrix`: branch-heavy task/resource feasibility and cost generation, with CPU and CUDA implementations.
- `spatial_events`: track-segment versus circular-zone event detection, with CPU and CUDA implementations.
- `export_cost_matrix`: inspectable CSV exporter used by the Python matrix plotting utility.
- `export_spatial_events`: inspectable CSV exporter used by the Python spatial-event plotting utility.
- `export_graph_foundation`: inspectable multi-graph CSR exporter used by the Python graph-foundation plotting utility.
- Graph foundation utilities: deterministic CSR construction plus chain, grid, layered, and sparse graph generators.
- `graph_bfs`: CPU queue BFS versus CUDA frontier BFS over the generated CSR graph shapes.
- `graph_connected_components`: CPU Union-Find versus CUDA label-propagation connected components.
- `graph_weighted_relaxation`: CPU Dijkstra versus CUDA iterative edge relaxation for weighted shortest paths.


#### Note on BFS anatomy plots and older JSONL files

`plot_graph_bfs_shape_scaling.py` supports both new and older BFS sweep files. Newer runs contain exact frontier-anatomy metadata such as `mean_frontier_edge_visits`. Older runs may not. In that case, the plotter derives fallback estimates from `max_distance`, `reached_count`, and `mean_out_degree` so the “useful work per synchronization point” plot is still meaningful. Exact max-frontier plots are skipped when the required exact data is unavailable.

## Build

### CPU-only

```bash
cmake -S . -B build-cpu -G Ninja -DENABLE_CUDA=OFF
cmake --build build-cpu
```

### CUDA-enabled on Windows with Ninja, MSVC, and CUDA 12.8

Use the included wrapper:

```bat
configure_ninja_cuda128.bat
```

Or manually open a Visual Studio x64 developer environment and run:

```bat
cmake -S . -B build-cuda-ninja -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DENABLE_CUDA=ON ^
  -DCMAKE_CXX_COMPILER=cl.exe ^
  -DCMAKE_CUDA_COMPILER="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8\bin\nvcc.exe" ^
  -DCMAKE_CUDA_HOST_COMPILER=cl.exe ^
  -DCMAKE_CUDA_ARCHITECTURES=89 ^
  -DCUDAToolkit_ROOT="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8"

cmake --build build-cuda-ninja
```

Notes:

- Use Ninja for the current Windows CUDA flow.
- Use `cl.exe` as the C++ compiler and CUDA host compiler.
- Make sure `cl`, `link`, `rc`, `mt`, and `ninja` are visible in the environment.
- If your GPU is not Ada / `sm_89`, change `-DCMAKE_CUDA_ARCHITECTURES` accordingly.

## Run

List benchmarks:

```bat
build-cuda-ninja\gpu_algobench.exe --list
```

Run the foundation smoke benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark foundation_smoke --preset small --repeat 5
```

Run the new BFS benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1
```

The first GPU BFS is intentionally educational rather than state-of-the-art. On the development laptop, the default layered case was slower on GPU for both `small` and `large`, which is discussed in `docs/phase_03_graph_bfs_analysis.md`.

Compare graph shapes manually:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1 --set graph=chain
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1 --set graph=grid
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1 --set graph=layered
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1 --set graph=random
```

Run all BFS scale/anatomy experiments, analysis, and plots using the single BFS runner:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

The script writes:

```text
results\graph_bfs_layered_scale_sweep.jsonl
results\graph_bfs_shape_scale_sweep.jsonl
results\graph_bfs_layered_scale_analysis\
results\graph_bfs_shape_scale_analysis\
results\graph_bfs_layered_scale_plots\
results\graph_bfs_shape_scale_plots\
```

The shape-scale JSONL and plots include:

```text
chain
grid
layered
random
```

Important switches are at the top of `execute_graph_bfs_all_sweeps_and_analyze.bat`:

```bat
set "RUN_LAYERED_SWEEP=1"
set "RUN_SHAPE_SCALE_SWEEP=1"
set "INCLUDE_VERY_LARGE=1"
set "INCLUDE_HEAVY_CASES=0"
```

`INCLUDE_HEAVY_CASES=0` still runs every graph family. It only skips the largest stress cases. Set it to `1` when you want the full heavy chain/grid/layered/random sweep.


Run the connected-components benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1
```

Compare connected-components graph families:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=chains
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=grid_components
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=random_clusters
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=mixed
```

Run all connected-components scale/anatomy experiments, analysis, and plots using the single connected-components runner:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

The script writes:

```text
results\graph_connected_components_shape_scale_sweep.jsonl
results\graph_connected_components_shape_scale_analysis\
results\graph_connected_components_shape_scale_plots\
```

The sweep includes:

```text
chains
grid_components
random_clusters
mixed
```

Important switches are at the top of `execute_graph_connected_components_all_sweeps_and_analyze.bat`:

```bat
set "CC_REPEAT=3"
set "CC_WARMUP=1"
set "INCLUDE_HEAVY_CASES=0"
```

`INCLUDE_HEAVY_CASES=0` still runs every graph family. It only skips the largest stress points. Set it to `1` when you want the full heavy sweep.

Export and plot a small connected-components graph:

```bat
build-cuda-ninja\export_graph_components.exe --graph random_clusters --components 6 --component-size 24 --output-dir results\graph_components_demo
python scripts\plot_graph_components.py --input-dir results\graph_components_demo --show
```

Run the polynomial benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark polynomial_batch --preset small --repeat 5 --warmup 1
```

Run the cost-matrix benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark cost_matrix --preset small --repeat 5 --warmup 1
```

Run the spatial-event benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark spatial_events --preset small --repeat 5 --warmup 1
```

Graph Phase 3.1 is a data foundation rather than a timing benchmark. Its CSR structures and deterministic generators are exercised through CTest:

```bat
ctest --test-dir build-cuda-ninja -R test_graph_foundation --output-on-failure --verbose
```

Export a small graph-foundation visualization bundle:

```bat
build-cuda-ninja\export_graph_foundation.exe --output-dir results\graph_foundation_demo
```

Plot the chain, grid, layered, and random sparse demo graphs plus the out-degree histogram:

```bat
python scripts\plot_graph_foundation.py --input-dir results\graph_foundation_demo --show
```

Export a small cost matrix for visual inspection:

```bat
build-cuda-ninja\export_cost_matrix.exe --preset tiny --output-dir results\cost_matrix_tiny
```

Plot the exported matrix as a cost heatmap, feasibility mask, and 3D surface:

```bat
python scripts\plot_cost_matrix.py --input-dir results\cost_matrix_tiny --show
```

For custom display sizes:

```bat
build-cuda-ninja\export_cost_matrix.exe --tasks 96 --resources 128 --output-dir results\cost_matrix_96x128
python scripts\plot_cost_matrix.py --input-dir results\cost_matrix_96x128 --max-surface-dim 160
```

Export a small spatial-event scene for visual inspection:

```bat
build-cuda-ninja\export_spatial_events.exe --output-dir results\spatial_events_demo
```

Plot the exported scene, event overlay, event-code matrix, and score matrix:

```bat
python scripts\plot_spatial_events.py --input-dir results\spatial_events_demo --show
```

For a custom readable scene:

```bat
build-cuda-ninja\export_spatial_events.exe --tracks 48 --zones 16 --output-dir results\spatial_events_48x16
python scripts\plot_spatial_events.py --input-dir results\spatial_events_48x16 --max-overlay-events 300
```

Write JSONL results:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark foundation_smoke --preset small --repeat 5 --output results\smoke.jsonl
build-cuda-ninja\gpu_algobench.exe --benchmark polynomial_batch --preset medium --repeat 5 --warmup 1 --output results\polynomial_medium.jsonl
build-cuda-ninja\gpu_algobench.exe --benchmark cost_matrix --preset medium --repeat 3 --warmup 1 --output results\cost_matrix_medium.jsonl
build-cuda-ninja\gpu_algobench.exe --benchmark spatial_events --preset medium --repeat 3 --warmup 1 --output results\spatial_events_medium.jsonl
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset medium --repeat 3 --warmup 1 --output results\graph_cc_medium.jsonl
```

## Execute all tests and current benchmarks

From the repository root:

```bat
execute_all_tests.bat
```

The script normalizes build paths without a trailing backslash before calling `ctest`, because quoted Windows paths ending in `\` can be parsed incorrectly by some tools.

The script runs:

1. `list_benchmarks`
2. `cuda_probe`, if built
3. `ctest -N` to print discovered tests, then `ctest --verbose` to execute them
4. `validate_all`
5. `gpu_algobench --benchmark all`

At the top of `execute_all_tests.bat`, change these values once:

```bat
set "BENCHMARK_PRESET=small"
set "BENCHMARK_REPEAT=5"
set "BENCHMARK_WARMUP=1"
```

Results are written to:

```text
results\execute_all_tests.jsonl
```

## Output format

Each benchmark run emits one JSON object per line:

```json
{"benchmark":"foundation_smoke","variant":"cpu","preset":"small","repeat":5,"warmup":1,"input_size":{"values":1000000},"total_ms":3.4,"h2d_ms":0.0,"kernel_ms":0.0,"d2h_ms":0.0,"correct":true,"device":"CPU","notes":"infrastructure smoke benchmark"}
```

A polynomial result also records benchmark-specific metadata such as `coefficient_count`, `x_step`, `x_cycle`, `checksum`, `reference_checksum`, `max_abs_error`, and `max_rel_error`. A cost-matrix result records `feasible_count`, `reference_feasible_count`, `feasibility_mismatches`, `checksum`, `reference_checksum`, `max_abs_error`, and `max_rel_error`. A spatial-event result records `event_count`, `reference_event_count`, `event_mismatches`, per-event-type counts, `checksum`, `reference_checksum`, `max_abs_error`, and `max_rel_error`. A graph-BFS result records graph shape metadata, reachability statistics, distance checksums, mismatch counts, and, for the GPU row, the `kernel_ms_semantics` note explaining that the timed region covers the traversal loop rather than a single isolated kernel.

Use JSONL because it is easy to append, diff, parse, and plot.

## Phase roadmap

- Phase 1: benchmark infrastructure.
- Phase 1.1: repeatable Windows test runner and CUDA build fixes.
- Phase 1.2: smoke-checksum correctness fix.
- Phase 1.3: infrastructure tests.
- Phase 2.1: polynomial batch evaluation.
- Phase 2.2: complex cost matrix generation.
- Phase 2.3: spatial event detection. **Implemented.**
- Phase 3.1: CSR graph foundation and deterministic graph generators. **Implemented.**
- Phase 3.1.1: graph export/visualization utilities. **Implemented.**
- Phase 3.2: graph BFS and reachability. **Implemented.**
- Phase 3.2.1: BFS interpretation notes and graph-shape comparison runner. **Implemented.**
- Phase 3.2.2: layered BFS scale sweep and crossover plotting. **Implemented.**
- Phase 3.2.3: graph-shape × scale BFS sweep and plots. **Implemented.**
- Phase 3.3: connected components. **Implemented.**
- Phase 3.3.1: connected-components shape × scale sweep and crossover plotting. **Implemented.**
- Phase 3.4: weighted relaxation.
- Phase 4: combinations, constraint network, local-search scoring, assignment preprocessing, scenario simulation.

## Important benchmark rule

Always compare:

- CPU total time.
- GPU end-to-end time.
- GPU timed compute-region time.
- Host-to-device transfer time.
- Device-to-host transfer time.

The GPU compute region may be fast while the full application is not. Some graph benchmarks also include orchestration and synchronization inside their timed compute region; benchmark docs call those cases out explicitly.

## Correctness policy

Benchmark repeats are for timing only. Validation metadata such as `checksum` should describe the final single output produced by the benchmark, not an accumulated checksum across repeats. This keeps CPU and GPU rows semantically comparable.

## Current CTest suite

The project currently builds eleven dependency-free C++ test executables, plus three exporter smoke tests registered directly with CTest:

```text
test_foundation
test_polynomial
test_cost_matrix
test_spatial_events
test_graph_foundation
test_graph_bfs
test_registry
test_cli
test_json_writer
test_random_utils
test_device_info
export_cost_matrix_smoke
export_spatial_events_smoke
export_graph_foundation_smoke
```

Run them directly with:

```bat
ctest --test-dir build-cuda-ninja --output-on-failure
```

or use:

```bat
execute_all_tests.bat
```

See `docs/phase_01_tests.md` for details.


## Full Graph BFS sweep flow

The only supported BFS batch runner is:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

It is self-contained: it does not call any other `execute_graph_bfs*.bat` file. It runs the layered-only scale sweep, the chain/grid/layered/random shape × scale sweep, the JSONL analyzer, and the plotting scripts.

The old split BFS batch files were removed to avoid label/subroutine bugs and command confusion.

Important variables at the top of the script:

```bat
set "RUN_LAYERED_SWEEP=1"
set "RUN_SHAPE_SCALE_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "INCLUDE_VERY_LARGE=1"
set "INCLUDE_HEAVY_CASES=0"
```

`INCLUDE_HEAVY_CASES=0` does **not** remove chain/grid/layered/random. It only skips the largest stress cases, such as `random_1048576`. Set it to `1` when you want the full heavy shape sweep.


## Connected-components sweep conclusions

The connected-components CPU baseline is **Union-Find**, while the GPU baseline is **iterative label propagation**.
That means the GPU story is different from BFS:

- GPU tends to win when a family has **many edges per iteration** and **few convergence iterations**.
- CPU can still win when the graph family forces the GPU to perform many global propagation rounds.
- In our representative sweep:
  - `random_clusters` crosses over earliest and scales best on GPU.
  - `grid_components` also becomes clearly GPU-friendly at larger sizes.
  - `chains` crosses over later because the GPU needs more rounds.
  - `mixed` is the cautionary case: it combines GPU-friendly and GPU-unfriendly structures, so the strong CPU Union-Find baseline can remain better even on heavier cases.

Use the canonical runner:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

Important output plots now include:

- `graph_cc_shape_scaling_times.png`
- `graph_cc_shape_scaling_speedup.png`
- `graph_cc_speedup_vs_edge_count.png`
- `graph_cc_speedup_vs_edge_iterations.png`
- `graph_cc_mixed_family_focus.png`


## Non-naive GPU connected-components variant

The connected-components benchmark now emits three rows per benchmark point when CUDA is enabled:

```text
cpu
    CPU Union-Find reference baseline.

gpu
    Original educational GPU baseline: node-parallel label propagation using atomicMin plus one-step pointer jumping.

gpu-non-naive
    Improved educational GPU variant: edge-parallel root hooking plus full pointer-jumping compression.
```

The non-naive variant is not intended to be a final production GPU connected-components implementation. It is a second experimental step that tests whether reducing convergence rounds improves the graph families where the naive GPU version struggled, especially `chains` and `mixed`.

The canonical connected-components sweep runner includes all three variants automatically:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

The analysis report now includes first crossover points for both GPU variants, and the plots include comparisons such as:

- `graph_cc_shape_scaling_times.png`
- `graph_cc_shape_scaling_speedup.png`
- `graph_cc_naive_vs_non_naive.png`
- `graph_cc_speedup_vs_edge_count.png`
- `graph_cc_speedup_vs_edge_iterations.png`
- `graph_cc_mixed_family_focus.png`


### What to look for in the new connected-components run

The important experiment is now whether `gpu-non-naive`:

1. reduces the number of GPU convergence iterations,
2. improves `mixed` and `chains`, where naive label propagation paid too many rounds,
3. keeps or improves the strong random-cluster behavior,
4. or becomes slower because each iteration does more work.

Both outcomes are useful. If non-naive is faster, we have shown the value of algorithmic redesign. If it is slower on some graph families, we have shown the cost of heavier root-finding work inside each GPU iteration.


### Connected-components GPU variant naming note

The benchmark executable still uses the variant name `gpu` for the original naive GPU implementation. In analysis files and documentation, read that as `gpu-naive`. The newer optimized experiment is `gpu-non-naive`.

The analysis CSV no longer writes a generic `gpu_ms_per_run` column, because it was only a duplicate alias for `gpu_naive_ms_per_run` and made it look like there were three GPU variants. Use these explicit columns instead:

```text
gpu_naive_ms_per_run
gpu_non_naive_ms_per_run
speedup_cpu_over_gpu_naive
speedup_cpu_over_gpu_non_naive
gpu_naive_to_non_naive_speedup
```


Run the weighted shortest-path benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 5 --warmup 1
```

Compare weighted graph families:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 3 --warmup 1 --set graph=chain
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 3 --warmup 1 --set graph=grid
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 3 --warmup 1 --set graph=layered
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 3 --warmup 1 --set graph=random
```

The GPU implementation is a simple edge-parallel relaxation baseline, not a production shortest-path solver. It is included to show the algorithmic tradeoff between CPU Dijkstra and GPU-friendly repeated edge scans.


## Weighted graph relaxation sweep

Phase 3.4 includes a canonical weighted shortest-path experiment flow:

```bat
execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
```

The script runs `graph_weighted_relaxation` over four graph families:

- `chain`
- `grid`
- `layered`
- `random`

and increasing sizes. It writes:

```text
results\graph_weighted_relaxation_shape_scale_sweep.jsonl
results\graph_weighted_relaxation_shape_scale_analysisresults\graph_weighted_relaxation_shape_scale_plots```

The benchmark compares:

- CPU Dijkstra with a priority queue
- GPU iterative edge-parallel relaxation with `atomicMin`

The most important plots are:

- `graph_wr_shape_scaling_times.png`
- `graph_wr_shape_scaling_speedup.png`
- `graph_wr_gpu_iterations.png`
- `graph_wr_speedup_vs_edge_iterations.png`

Interpretation: weighted shortest path is not automatically GPU-friendly. The GPU version needs enough edge-parallel work and few convergence rounds. Chain/grid-like graphs may favor CPU Dijkstra; layered/random graphs are better candidates for GPU edge relaxation.


## Weighted-relaxation convergence guard

The GPU weighted-relaxation benchmark uses repeated edge-relaxation passes.
A final pass with no changes is needed to prove convergence. The chain-family
default therefore uses `N` passes for `N` nodes, not `N - 1`, so the first sweep
case (`chain_256`) can complete correctly instead of stopping the batch early.


## Weighted-relaxation variants

`graph_weighted_relaxation` now emits three variants:

- `cpu` - Dijkstra with a priority queue.
- `gpu` - global Bellman-Ford-style edge scan using `atomicMin`.
- `gpu-frontier` - active-frontier GPU relaxation that expands only nodes whose distances changed.

Run the complete weighted experiment with:

```bat
execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
```

The analysis and plots compare CPU, global GPU, and frontier GPU. The frontier variant was added because the global scan variant repeatedly scans all edges and can be much slower on chain/grid/layered graphs.


## Weighted-relaxation backend recommendations

Weighted shortest path now reports three variants:

- `cpu` - Dijkstra with a priority queue.
- `gpu` - global GPU Bellman-Ford-style edge scan.
- `gpu-frontier` - active-frontier GPU relaxation.

The canonical weighted runner also generates a backend recommendation report:

```bat
execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
```

Recommendation output:

```text
results\graph_weighted_relaxation_backend_recommendations```

The main current conclusion is that `gpu-frontier` is an educational experiment,
not a default winner. CPU Dijkstra remains best for high-diameter chain/grid-like
cases in the measured range, while the global GPU scan wins on large low-diameter
random graphs.
