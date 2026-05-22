# Phase 3.2.4 — Graph BFS Frontier Anatomy Metrics

Phase 3.2.3 showed that graph shape changes the CPU/GPU result dramatically. The next question is:

```text
Why does one large graph stay CPU-favored while another becomes strongly GPU-favored?
```

This patch adds BFS frontier-anatomy metrics to the `graph_bfs` benchmark metadata and extends the shape-scaling plotter to visualize those metrics.

## New metadata fields

Both CPU and GPU `graph_bfs` rows now include these fields in `metadata`:

| Field | Meaning |
|---|---|
| `frontier_level_count` | Number of BFS levels reached from the chosen source. Usually `max_distance + 1`. |
| `max_frontier_size` | Largest number of active nodes in any BFS level. |
| `mean_frontier_size` | Average number of reached nodes per BFS level. |
| `reached_edge_visits` | Sum of outgoing edges incident to reached nodes. This approximates top-down BFS edge-scanning work. |
| `max_frontier_edge_visits` | Largest outgoing-edge workload in any BFS level. |
| `mean_frontier_edge_visits` | Average outgoing-edge work per BFS level. This is a useful proxy for GPU work per synchronization point. |
| `mean_reached_out_degree` | `reached_edge_visits / reached_count`. |
| `frontier_width_to_depth` | `max_frontier_size / max_distance`, a compact but rough width-vs-depth ratio. |

These metrics are computed from the CPU reference distance vector, so they are independent of whether the row is produced by the CPU or GPU implementation.

## Why these metrics matter

The current CUDA BFS is level-synchronous:

```text
one BFS level -> one frontier kernel launch -> copy next-frontier size -> repeat
```

So the GPU benefits when each level contains a lot of useful work. A graph with many nodes is not enough; the graph needs enough **frontier width** and enough **edge work per level**.

This is why the shape-scaling results should be read as:

```text
size + anatomy -> performance
```

not simply:

```text
size -> performance
```

## Observed shape × scale result

A representative development run produced this summary:

```text
kind       nodes        edges         depth    CPU ms/run   GPU ms/run   CPU/GPU speedup
--------------------------------------------------------------------------------------------
chain      256          510               255     0.001680     7.602957            0.0002x
chain      1024         2046             1023     0.008840    58.266954            0.0002x
chain      4096         8190             4095     0.021980   168.769648            0.0001x
chain      8192         16382            8191     0.043600   310.987263            0.0001x
chain      16384        32766           16383     0.123620   619.418244            0.0002x

grid       256          960                30     0.002280     1.069978            0.0021x
grid       4096         16128             126     0.029760     4.995341            0.0060x
grid       16384        65024             254     0.155360    10.298618            0.0151x
grid       262144       1046528          1022     2.766140    49.554876            0.0558x

layered    96           320                 5     0.001020     0.260826            0.0039x
layered    512          1792                7     0.001870     0.319795            0.0058x
layered    4096         30720              15     0.026240     0.791459            0.0332x
layered    16384        126976             31     0.102570     2.049002            0.0501x
layered    262144       2064384            63     1.311370     3.814666            0.3438x
layered    786432       6225920            95     4.690610     6.238438            0.7519x

random     256          2048                4     0.005210     0.261434            0.0199x
random     4096         32768               6     0.122420     0.364550            0.3358x
random     16384        131072              6     0.553870     0.489526            1.1314x
random     65536        524288              7     2.377140     0.692522            3.4326x
random     262144       2097152             8    13.486440     1.130106           11.9338x
random     1048576      8388608             8   111.904650     2.944896           37.9995x
```

The result strongly supports the anatomy hypothesis:

- `chain` is terrible for GPU BFS because the frontier is essentially one node wide and the depth grows with node count.
- `grid` improves with size but still has many levels and moderate frontiers.
- `layered` gets close to parity at very large sizes, but the current implementation still pays repeated frontier synchronization and atomic overhead.
- `random` crosses over early because the graph has low depth and large frontier bursts, giving the GPU a lot of work per traversal level.


## Compatibility with older JSONL files

Older sweep files generated before Phase 3.2.4 do not contain the exact frontier-anatomy fields. They usually still contain enough metadata for useful estimates:

```text
max_distance
reached_count
mean_out_degree
```

The plotter now handles both formats:

1. If exact fields such as `mean_frontier_edge_visits` are present, it uses them directly.
2. If they are missing, it derives fallback estimates:

```text
frontier_level_count      ~= max_distance + 1
mean_frontier_size        ~= reached_count / frontier_level_count
reached_edge_visits       ~= reached_count * mean_out_degree
mean_frontier_edge_visits ~= reached_edge_visits / frontier_level_count
```

These fallback values are good enough for interpretation and keep old JSONL files plottable. They should be treated as estimates, not exact traversal instrumentation. For exact `max_frontier_size`, `max_frontier_edge_visits`, and exact per-level work, rerun the sweep after this patch.

The plotter also no longer writes empty plots when a metric is unavailable. For example, if `max_frontier_size` is absent and cannot be estimated safely, it prints a skip message instead of producing a blank `graph_bfs_shape_scaling_max_frontier.png`.

## Updated plotter output

After running:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

plot with:

```bat
python scripts\plot_graph_bfs_shape_scaling.py ^
  --jsonl results\graph_bfs_shape_scale_sweep.jsonl ^
  --output-dir results\graph_bfs_shape_scale_plots ^
  --show
```

The script now writes:

```text
graph_bfs_shape_scaling_summary.csv
graph_bfs_shape_scaling_speedup.png
graph_bfs_shape_scaling_cpu_times.png
graph_bfs_shape_scaling_gpu_times.png
graph_bfs_shape_scaling_depth.png
graph_bfs_shape_scaling_max_frontier.png
graph_bfs_shape_scaling_mean_edges_per_level.png
graph_bfs_speedup_vs_frontier_work.png
```

The most useful new figure is:

```text
graph_bfs_speedup_vs_frontier_work.png
```

It plots speedup against average edge work per BFS level. That plot helps explain why the random graph can win even when other large graphs do not.

## Main conclusion

The refined BFS lesson is:

```text
Large graph + narrow frontier + high depth
    -> CPU may still dominate.

Large graph + wide frontier + low/moderate depth + enough edge work per level
    -> GPU can become much stronger, even with the educational implementation.
```

This is one of the clearest examples in the repository of why algorithmic hardware decisions must consider workload anatomy, not just problem category.


## Canonical BFS runner

Use a single batch file for all BFS sweep, analysis, and plot generation:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

This script is self-contained. It runs the layered-only sweep, the chain/grid/layered/random shape × scale sweep, the JSONL analyzer, and both plotters. Older split `execute_graph_bfs*.bat` wrappers were retired to avoid duplicated logic and batch-label errors.
