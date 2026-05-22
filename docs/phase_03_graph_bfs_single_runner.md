# Phase 3.2.8 — Single Graph BFS Sweep Runner

This patch consolidates all graph-BFS batch execution into one canonical script:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

Older split `execute_graph_bfs*.bat` wrappers were removed. This avoids duplicated batch subroutines and the Windows batch-label failure mode where a wrapper calls a label that does not exist in the current file.

## What the script runs

The script is self-contained and does not call any other graph-BFS batch file.

It runs:

1. The layered-only BFS scale sweep.
2. The chain/grid/layered/random shape × scale sweep.
3. JSONL diagnostics for both outputs.
4. Plot generation for both outputs.

## Main switches

At the top of the script:

```bat
set "RUN_LAYERED_SWEEP=1"
set "RUN_SHAPE_SCALE_SWEEP=1"
set "RUN_ANALYSIS=1"
set "RUN_PLOTS=1"
set "INCLUDE_VERY_LARGE=1"
set "INCLUDE_HEAVY_CASES=0"
```

`INCLUDE_HEAVY_CASES=0` still runs every graph family: chain, grid, layered, and random. It only skips the largest stress cases, such as `random_1048576` and `grid_1024x1024`.

## Outputs

```text
results\graph_bfs_layered_scale_sweep.jsonl
results\graph_bfs_shape_scale_sweep.jsonl
results\graph_bfs_layered_scale_analysis\
results\graph_bfs_shape_scale_analysis\
results\graph_bfs_layered_scale_plots\
results\graph_bfs_shape_scale_plots\
```

The shape-scale JSONL is the one that contains all graph anatomies:

```text
chain
grid
layered
random
```

## Why this matters

The BFS section is about more than graph size. The same BFS implementation behaves very differently depending on graph anatomy:

- Chain graphs have large depth and tiny frontiers.
- Grid graphs have local wavefronts and many synchronization levels.
- Layered graphs expose wide regular frontiers.
- Random sparse graphs can have low depth and large frontier bursts.

The single runner makes that comparison reproducible with one command.
