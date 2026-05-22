# GPU Classical Algorithms Lab

A C++/CUDA benchmark playground for testing where GPU acceleration helps classical algorithmic workloads: graph theory, constraint checking, cost matrices, spatial events, combinations, local search, and scenario simulation.

This repository is currently in **Phase 3.2.8: single graph-BFS sweep runner**, after the Phase 1 infrastructure and the three Phase 2 CPU/GPU pairwise/data-parallel benchmarks.

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
- CTest coverage for the registry, CLI, JSONL writer, random utilities, device info, foundation benchmark semantics, polynomial, cost-matrix, spatial-event, graph-foundation, and graph-BFS semantics.
- `polynomial_batch`: degree-15 polynomial evaluation over many stride-100 `x` values, with CPU and CUDA implementations.
- `cost_matrix`: branch-heavy task/resource feasibility and cost generation, with CPU and CUDA implementations.
- `spatial_events`: track-segment versus circular-zone event detection, with CPU and CUDA implementations.
- `export_cost_matrix`: inspectable CSV exporter used by the Python matrix plotting utility.
- `export_spatial_events`: inspectable CSV exporter used by the Python spatial-event plotting utility.
- `export_graph_foundation`: inspectable multi-graph CSR exporter used by the Python graph-foundation plotting utility.
- Graph foundation utilities: deterministic CSR construction plus chain, grid, layered, and sparse graph generators.
- `graph_bfs`: CPU queue BFS versus CUDA frontier BFS over the generated CSR graph shapes.


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
- Phase 3.3: connected components.
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
