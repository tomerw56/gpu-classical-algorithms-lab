# Phase 3.2.1 — Why the First GPU BFS Can Lose

Phase 3.2 intentionally uses a readable, level-synchronous GPU BFS rather than a heavily optimized production implementation. The first measurements therefore teach an important lesson:

```text
A graph algorithm being parallel in principle does not guarantee that a straightforward GPU mapping will beat a strong CPU baseline.
```

## Development-machine observation

On the development machine used while building this lab, the default layered BFS case produced results like:

```text
small:
  CPU  0.598 ms
  GPU  6.992 ms

large:
  CPU  22.020 ms
  GPU  25.895 ms
```

The GPU result improves relative to the CPU as the graph grows, but the educational implementation still loses in these representative runs. That is a useful result, not a mistake.

## Why the GPU implementation loses

The current GPU traversal loop performs one breadth-first level at a time:

```text
1. reset next-frontier size
2. launch a frontier kernel
3. use atomicCAS to claim unseen nodes
4. use atomicAdd to append newly reached nodes
5. copy the next-frontier size back to the host
6. decide whether another level is needed
```

This creates several costs that the CPU queue BFS does not have:

- many kernel launches,
- per-level synchronization,
- per-level frontier-size device-to-host control copies,
- atomics during discovery and frontier construction,
- poor utilization whenever the active frontier is narrow.

The CPU queue baseline, meanwhile, is compact, cache-friendly, and has very little orchestration overhead.

## Interpreting `kernel_ms`

For most dense benchmarks in this repository, `kernel_ms` is close to "the main CUDA kernel timing." For `graph_bfs`, it should be read more carefully:

```text
kernel_ms = timed GPU traversal-loop region
```

It brackets the repeated BFS-level loop, not a single isolated frontier kernel. Benchmark metadata records this explicitly as `kernel_ms_semantics`.

## Shape matters

The graph generators are not cosmetic. They intentionally stress different traversal patterns:

| Shape | Expected lesson |
|---|---|
| `chain` | GPU-hostile: one-node-wide frontier and many traversal levels. |
| `grid` | Structured local wavefront with moderate frontier width. |
| `layered` | Wider frontiers and the friendliest case for this GPU design. |
| `random` | Irregular sparse topology; result depends on frontier and degree behavior. |

## Reproducible shape-comparison runner

Use:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

It executes:

```text
graph_bfs on chain
graph_bfs on grid
graph_bfs on layered
graph_bfs on random
```

and writes:

```text
results\graph_bfs_shape_comparison.jsonl
```

The script defaults to `small` because `large chain` can become intentionally slow: it requires many BFS levels with almost no parallel work per level. Change the variables at the top of the script to run a heavier study.

## Manual large-case commands

For an explicit heavier comparison:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset large --repeat 5 --warmup 1 --set graph=chain
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset large --repeat 5 --warmup 1 --set graph=grid
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset large --repeat 5 --warmup 1 --set graph=layered
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset large --repeat 5 --warmup 1 --set graph=random
```

## What would be needed for a faster GPU BFS

A stronger GPU BFS implementation would likely need some combination of:

- work-efficient frontier compaction,
- better edge/load balancing,
- warp- or block-level queueing,
- fewer host roundtrips,
- direction-optimizing traversal for suitable graphs,
- or keeping BFS as part of a larger all-GPU pipeline.

This repository may later add an optimized GPU BFS variant. The current version is deliberately the transparent baseline that exposes the core tradeoff.

## Follow-up: scale sweep instead of single points

Phase 3.2.2 adds a layered-graph sweep so this discussion can be tested as a curve rather than only as `small` and `large` examples:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
python scripts\plot_graph_bfs_scaling.py --jsonl results\graph_bfs_layered_scale_sweep.jsonl --output-dir results\graph_bfs_layered_scale_plots --show
```

The plotter reports average milliseconds per BFS run and prints whether a GPU crossover was actually measured. See `docs/phase_03_graph_bfs_scaling.md` for details.

Phase 3.2.3 broadens the experiment from one graph shape to four. It sweeps `chain`, `grid`, `layered`, and `random` graph families across size ranges and plots speedup curves per anatomy:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
python scripts\plot_graph_bfs_shape_scaling.py --jsonl results\graph_bfs_shape_scale_sweep.jsonl --output-dir results\graph_bfs_shape_scale_plots --show
```

This is useful because node count alone does not explain BFS performance. Frontier width, traversal depth, and topology determine how much useful parallel work the GPU sees at each level. See `docs/phase_03_graph_bfs_shape_scaling.md` for the sweep design.

## Follow-up: frontier-anatomy explanation

Phase 3.2.4 adds frontier-anatomy metadata to `graph_bfs` and extends the shape-scaling plotter. The key new idea is to compare timing against useful work per BFS level:

```text
mean_frontier_edge_visits = reached_edge_visits / frontier_level_count
```

This explains the representative shape-scaling result:

- chain graphs have huge depth and almost no per-level parallelism;
- grids expose more work but still require many levels;
- layered graphs approach parity only when very large;
- random graphs can have low depth and large frontier bursts, which is why the GPU can cross over strongly.

See `docs/phase_03_graph_bfs_frontier_anatomy.md` for the detailed table and the new plots.


## Canonical BFS runner

Use a single batch file for all BFS sweep, analysis, and plot generation:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

This script is self-contained. It runs the layered-only sweep, the chain/grid/layered/random shape × scale sweep, the JSONL analyzer, and both plotters. Older split `execute_graph_bfs*.bat` wrappers were retired to avoid duplicated logic and batch-label errors.
