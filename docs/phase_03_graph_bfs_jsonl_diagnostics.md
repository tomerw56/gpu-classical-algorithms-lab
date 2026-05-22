# Phase 3.2.6 — Graph BFS JSONL Diagnostics Flow

The BFS shape/scale plots depend on metadata emitted by the `graph_bfs` benchmark.
If a plot appears empty, the problem is usually not Matplotlib. It is usually one
of these data-flow issues:

1. The JSONL file was produced by an older executable.
2. The JSONL file has CPU rows but no matching GPU rows, or vice versa.
3. The JSONL file is a layered-only sweep, so shape-comparison plots contain only
   one graph family.
4. Exact frontier-anatomy fields are missing.
5. The plotter script being run is stale and does not include fallback handling.

This phase adds a diagnostic flow that makes those issues explicit.

## One-command flow

Run from the repository root:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

The script performs three steps:

```text
1. execute_graph_bfs_all_sweeps_and_analyze.bat
2. scripts/analyze_graph_bfs_jsonl.py
3. scripts/plot_graph_bfs_shape_scaling.py
```

It writes:

```text
results/graph_bfs_shape_scale_sweep.jsonl
results/graph_bfs_shape_scale_analysis/graph_bfs_jsonl_analysis_report.md
results/graph_bfs_shape_scale_analysis/graph_bfs_jsonl_analysis_summary.csv
results/graph_bfs_shape_scale_plots/*.png
```

## Analyzer only

To inspect an existing JSONL file without rerunning the benchmark:

```bat
python scripts\analyze_graph_bfs_jsonl.py ^
  --jsonl results\graph_bfs_shape_scale_sweep.jsonl ^
  --output-dir results\graph_bfs_shape_scale_analysis
```

For the layered-only sweep:

```bat
python scripts\analyze_graph_bfs_jsonl.py ^
  --jsonl results\graph_bfs_layered_scale_sweep.jsonl ^
  --output-dir results\graph_bfs_layered_scale_analysis
```

## Exact vs fallback frontier metrics

Newer benchmark binaries emit exact fields such as:

```text
frontier_level_count
max_frontier_size
mean_frontier_size
reached_edge_visits
max_frontier_edge_visits
mean_frontier_edge_visits
mean_reached_out_degree
frontier_width_to_depth
```

Older JSONL files usually only contain:

```text
max_distance
reached_count
mean_out_degree
edge_count
```

For older files, the analyzer and plotter can derive useful fallback estimates:

```text
frontier_level_count      ~= max_distance + 1
mean_frontier_size        ~= reached_count / frontier_level_count
reached_edge_visits       ~= reached_count * mean_out_degree
mean_frontier_edge_visits ~= reached_edge_visits / frontier_level_count
```

These estimates are good enough to explain why random graphs expose much more
useful work per BFS synchronization point than chain or grid graphs. They are not
a substitute for the exact frontier instrumentation.

## Why `max_frontier_size` may be missing

`max_frontier_size` cannot be reconstructed exactly from the older JSONL fields.
If the analyzer says:

```text
max_frontier_size is unavailable
```

then the `graph_bfs_shape_scaling_max_frontier.png` plot is expected to be skipped
or empty. Rebuild after applying the frontier-anatomy patch and rerun the sweep to
get exact max-frontier values.

## Strict mode

To fail the analysis if exact Phase 3.2.4 metrics are missing:

```bat
python scripts\analyze_graph_bfs_jsonl.py ^
  --jsonl results\graph_bfs_shape_scale_sweep.jsonl ^
  --output-dir results\graph_bfs_shape_scale_analysis ^
  --require-exact-frontier
```

The batch flow exposes this as:

```bat
set "REQUIRE_EXACT_FRONTIER=1"
```

inside `execute_graph_bfs_all_sweeps_and_analyze.bat`.

## Interpreting the uploaded-file issue

If `useful work per synchronization point` appears empty even though the analyzer
shows `mean_frontier_edge_visits`, then the most likely cause is that Windows is
running an older copy of `scripts/plot_graph_bfs_shape_scaling.py`.

The analyzer report should always be checked first. If it says:

```text
The useful-work metric is available
```

then the plotter should generate:

```text
graph_bfs_shape_scaling_mean_edges_per_level.png
graph_bfs_speedup_vs_frontier_work.png
```


## Full BFS sweep/analyze/plot runner

Use the single canonical BFS runner:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

It is self-contained and does not call any other `execute_graph_bfs*.bat` file. It runs:

1. The layered-only scale sweep.
2. The chain/grid/layered/random shape × scale sweep.
3. JSONL diagnostics for both outputs.
4. Plot generation for both outputs.

This avoids the confusing situation where only the layered JSONL is analyzed. The output folders are separate:

```text
results/graph_bfs_layered_scale_analysis
results/graph_bfs_layered_scale_plots
results/graph_bfs_shape_scale_analysis
results/graph_bfs_shape_scale_plots
```
