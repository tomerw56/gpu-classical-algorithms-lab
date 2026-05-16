# Phase 3.2.2 — Graph BFS Scaling Sweep and Crossover Plot

Phase 3.2.1 established that the first educational GPU BFS can lose to a strong CPU queue BFS. Phase 3.2.2 adds a repeatable way to ask a more precise question:

```text
As the graph gets larger, does the GPU gap narrow enough to cross over?
```

The default sweep focuses on the **layered graph** because it is the most GPU-friendly shape in the current benchmark family: it creates broad frontiers and gives the GPU many active nodes per BFS level.

## Run the sweep

From the repository root:

```bat
execute_graph_bfs_scale_sweep.bat
```

The script writes:

```text
results\graph_bfs_layered_scale_sweep.jsonl
```

At the top of the batch file, the most useful controls are:

```bat
set "BFS_REPEAT=5"
set "BFS_WARMUP=1"
set "RUN_GPU=1"
set "OVERWRITE_RESULTS=1"
set "INCLUDE_VERY_LARGE=1"
```

The final `very_large` point is intentionally optional. It is useful when looking for a crossover, but it also requires more graph-generation time and memory than the earlier points.

## Scale points

The runner uses custom layered graph dimensions rather than only the named benchmark presets:

| Label | Layers | Nodes/layer | Fanout | Approximate purpose |
|---|---:|---:|---:|---|
| `tiny_debug` | 6 | 16 | 4 | trivial correctness-scale graph |
| `tiny_wider` | 8 | 64 | 4 | still tiny, slightly broader frontier |
| `smallish` | 16 | 256 | 8 | early meaningful traversal |
| `repository_small` | 32 | 512 | 8 | same shape family as the default small case |
| `mid` | 48 | 1024 | 8 | in-between scaling point |
| `larger` | 64 | 2048 | 8 | larger frontier work |
| `repository_medium` | 64 | 4096 | 8 | same shape family as the default medium case |
| `repository_large` | 96 | 8192 | 8 | same shape family as the default large case |
| `very_large` | 128 | 16384 | 8 | optional deeper crossover search |

The exact node count is:

```text
nodes = layers × nodes_per_layer
```

For layered graphs, the edge count is approximately:

```text
edges ≈ (layers - 1) × nodes_per_layer × fanout
```

## Plot the sweep

After the batch completes:

```bat
python scripts\plot_graph_bfs_scaling.py ^
  --jsonl results\graph_bfs_layered_scale_sweep.jsonl ^
  --output-dir results\graph_bfs_layered_scale_plots ^
  --show
```

The plotter writes:

```text
graph_bfs_scaling_summary.csv
graph_bfs_scaling_times.png
graph_bfs_scaling_speedup.png
```

### Timing normalization

The benchmark executable reports `total_ms` across all measured repeats. The scaling plotter normalizes to:

```text
average_ms_per_BFS_run = total_ms / repeat
```

The CPU/GPU speedup curve is computed as:

```text
speedup = CPU_ms_per_run / GPU_ms_per_run
```

Interpretation:

```text
speedup < 1.0  -> CPU is faster
speedup = 1.0  -> parity
speedup > 1.0  -> GPU is faster
```

The plotter prints the first measured GPU crossover when one is present. If no crossover appears, it reports the best observed speedup and the corresponding graph size.

## Why this is useful

This sweep turns a single anecdotal result into a curve. It helps answer questions such as:

- Does the GPU remain behind at every size?
- Does the GPU approach parity as frontier work grows?
- Is the current implementation limited by fixed per-level overhead or by raw traversal throughput?
- Would an optimized GPU BFS variant be worth implementing next?

That is exactly the kind of evidence this repository is intended to produce.
