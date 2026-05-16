# Phase 3.2.3 — Graph BFS Shape × Scale Sweep

Phase 3.2.2 asked a size-only question for the most GPU-friendly existing benchmark shape:

```text
As a layered graph gets larger, does the educational GPU BFS catch up?
```

Phase 3.2.3 asks the richer and more important question:

```text
How do graph size and graph anatomy combine to affect CPU vs GPU BFS?
```

The graph kind matters because BFS does not expose the same amount of parallel work at every level. A graph may have many nodes overall but still offer only a tiny active frontier at a time. The current GPU BFS is especially sensitive to this because it launches one traversal round per BFS level.

## Graph shapes in the sweep

The new runner compares four graph families:

| Graph kind | Structural idea | Expected lesson |
|---|---|---|
| `chain` | One long path | Worst case for current GPU BFS: only one active node per level. |
| `grid` | Local 2D wavefront | More parallel than a chain, but still many BFS levels. |
| `layered` | Wide levels with fanout | Friendliest current GPU case: many active nodes per level. |
| `random` | Controlled sparse random topology | Lower-diameter frontier bursts may make GPU behavior more interesting. |

The sweep is deliberately not trying to make all graphs identical. Instead, it shows that **similar node counts do not imply similar BFS behavior**.

## Run the shape × scale sweep

From the repository root:

```bat
execute_graph_bfs_shape_scale_sweep.bat
```

The script writes:

```text
results\graph_bfs_shape_scale_sweep.jsonl
```

At the top of the batch file, the most useful controls are:

```bat
set "BFS_REPEAT=5"
set "BFS_WARMUP=1"
set "RUN_GPU=1"
set "OVERWRITE_RESULTS=1"
set "INCLUDE_HEAVY_CASES=0"
```

`INCLUDE_HEAVY_CASES=0` is intentional. The default sweep still runs several scale points for **every** graph family; this flag only disables the optional longest-running stress points listed below. Chain and grid traversals can become expensive for the current educational GPU BFS because their BFS depth grows substantially. Turn this flag on when you want a deeper, longer-running study.

The batch commands use `--preset tiny` as a neutral base preset before explicit `--set` dimensions define the actual graph size. This is not a tiny-only run. To avoid a misleading console table, the runner now also passes `--set sweep_label=...`, and `graph_bfs` displays that scale label in the result table `preset` column. Example labels include:

```text
chain_4096
grid_256x256
layered_64x4096
random_262144
```

## Default sweep points

### Chain

```text
256, 1024, 4096, 8192 nodes
```

Optional heavy point:

```text
16384 nodes
```

### Grid

```text
16x16, 64x64, 128x128, 256x256
```

Optional heavy point:

```text
512x512
```

### Layered

```text
6x16, 8x64, 16x256, 32x512, 64x4096
```

Optional heavy point:

```text
96x8192
```

### Random sparse

```text
256, 4096, 16384, 65536, 262144 nodes
```

Optional heavy point:

```text
1048576 nodes
```

The random sweep uses fixed out-degree and seed values so repeated runs stay comparable.

## Plot the sweep

After the batch completes:

```bat
python scripts\plot_graph_bfs_shape_scaling.py ^
  --jsonl results\graph_bfs_shape_scale_sweep.jsonl ^
  --output-dir results\graph_bfs_shape_scale_plots ^
  --show
```

The plotter writes:

```text
graph_bfs_shape_scaling_summary.csv
graph_bfs_shape_scaling_speedup.png
graph_bfs_shape_scaling_cpu_times.png
graph_bfs_shape_scaling_gpu_times.png
```

## Timing normalization

The benchmark executable reports `total_ms` across all measured repeats. The plotter normalizes to:

```text
average_ms_per_BFS_run = total_ms / repeat
```

The CPU/GPU speedup curve is:

```text
speedup = CPU_ms_per_run / GPU_ms_per_run
```

Interpretation:

```text
speedup < 1.0  -> CPU is faster
speedup = 1.0  -> parity
speedup > 1.0  -> GPU is faster
```

## What to look for

The most informative figure is:

```text
graph_bfs_shape_scaling_speedup.png
```

Use it to compare the four graph families:

- `chain` should remain strongly CPU-favored for the current implementation.
- `grid` may improve with size, but its many traversal levels still hurt the GPU.
- `layered` should be one of the best shapes for the current GPU BFS.
- `random` may become interesting because a low-diameter topology can create large frontier bursts.

The script also prints a per-shape crossover summary:

```text
kind       first crossover / best observed speedup
```

If a shape never crosses the `1.0x` parity line, the report still identifies the best observed point.

## Why this matters

This sweep is one of the clearest demonstrations in the repository that algorithmic hardware decisions should not be made from problem labels alone.

```text
"BFS on GPU" is not one workload.
```

The shape of the graph controls how much useful parallel work is exposed at each step. That is why a GPU can look very poor on a chain and far more competitive on a wide or low-diameter graph, even when the total node count is similar.
