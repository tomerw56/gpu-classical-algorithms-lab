# Phase 3.4.1 — weighted graph relaxation sweep/analyze/plot flow

This phase completes the weighted graph relaxation benchmark with a single canonical experiment runner:

```bat
execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
```

The runner performs all weighted shortest-path sweep work in one place:

1. Run `graph_weighted_relaxation` over several graph families and sizes.
2. Write one JSONL file.
3. Analyze that JSONL file into CSV and Markdown reports.
4. Generate scaling and crossover plots.

## Compared algorithms

The benchmark compares:

| Variant | Meaning |
|---|---|
| `cpu` | Dijkstra with a priority queue over positive integer weighted CSR graphs |
| `gpu` | Iterative edge-parallel relaxation using `atomicMin` on integer distances |

The GPU method is intentionally educational. It is closer to Bellman-Ford-style global relaxation than to Dijkstra. It can expose much more parallel work, but it may also scan every edge repeatedly.

## Graph families

The sweep covers:

| Family | Why it matters |
|---|---|
| `chain` | High-diameter path-like graph; usually bad for global relaxation |
| `grid` | Structured local wavefront; more parallelism than chain but still many rounds |
| `layered` | Lower-depth, wide edge work per layer; more GPU-oriented |
| `random` | Low effective diameter and high edge parallelism; often the best GPU candidate |

## Important variables

At the top of the batch file:

```bat
set "WR_REPEAT=3"
set "WR_WARMUP=1"
set "INCLUDE_HEAVY_CASES=0"
```

`INCLUDE_HEAVY_CASES=0` still runs every graph family. It only skips the largest stress points.
Weighted relaxation can be expensive for chain/grid cases because the naive GPU algorithm may require many global passes.

## Output files

```text
results\graph_weighted_relaxation_shape_scale_sweep.jsonl
results\graph_weighted_relaxation_shape_scale_analysis\graph_weighted_relaxation_analysis_summary.csv
results\graph_weighted_relaxation_shape_scale_analysis\graph_weighted_relaxation_analysis_report.md
results\graph_weighted_relaxation_shape_scale_plots\*.png
```

## Generated plots

The plotter generates:

- `graph_wr_shape_scaling_times.png`
- `graph_wr_shape_scaling_speedup.png`
- `graph_wr_gpu_iterations.png`
- `graph_wr_max_finite_distance.png`
- `graph_wr_mean_out_degree.png`
- `graph_wr_speedup_vs_iterations.png`
- `graph_wr_speedup_vs_edge_count.png`
- `graph_wr_speedup_vs_edge_iterations.png`

`edge-iterations` means:

```text
edge_count × gpu_iterations
```

It is a useful proxy for the amount of global edge-relaxation work performed by the GPU.

## Expected lesson

The expected conclusion is not simply “GPU is faster for bigger graphs.”
The more precise lesson is:

```text
GPU edge relaxation improves when the graph has enough parallel edge work and few relaxation rounds.
CPU Dijkstra remains strong when graph anatomy causes many GPU iterations or when graph size is too small to amortize GPU overhead.
```


## Convergence guard note

The GPU relaxation loop needs one extra no-change pass to prove convergence.
For a chain with `N` nodes, the farthest shortest path has `N - 1` edges, but
the convergence flag is only observed on the following pass. Therefore the
default chain guard is `N` passes, not `N - 1`. This prevents a case where the
distance vector is already correct but the row is marked `correct=no` only
because convergence was not yet observed.
