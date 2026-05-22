# Graph BFS Benchmark

`graph_bfs` is the first real traversal benchmark built on top of the Phase 3.1 CSR graph foundation.

The benchmark computes unweighted shortest-path distance from a source node to every reachable node:

```text
source -> all reachable nodes
```

A distance of `-1` means the node was not reached.

## CPU baseline

The CPU implementation is a classic queue-based breadth-first search over CSR:

```text
push source
while queue is not empty:
    pop node
    for each outgoing neighbor:
        if neighbor was not visited:
            distance[neighbor] = distance[node] + 1
            push neighbor
```

This is a strong baseline for small and medium graphs because it has low overhead and excellent control-flow locality.

## GPU implementation

The CUDA implementation is a level-synchronous frontier BFS:

```text
current_frontier = [source]
while current_frontier is not empty:
    launch kernel over current_frontier
    each thread scans one frontier node's neighbors
    atomicCAS claims newly discovered neighbors
    atomicAdd appends each discovered node to next_frontier
    swap frontiers
```

The important mapping is:

```text
one GPU thread = one active frontier node
```

This is intentionally simple and educational. It is not a fully optimized production GPU BFS. More advanced implementations would use edge-based load balancing, prefix-sum compaction, warp-level queues, or library algorithms.

For this benchmark, `kernel_ms` is best interpreted as the timed GPU traversal-loop region, not as one isolated frontier kernel. The loop contains repeated frontier rounds plus control synchronization needed to know when BFS is finished.

## Graph shapes

The benchmark can run on several deterministic graph generators from `graph_foundation`:

| Graph kind | How to select | Why it matters |
|---|---|---|
| Chain | `--set graph=chain` | Narrow frontier; poor GPU utilization. |
| Grid | `--set graph=grid` | Structured wavefront; more realistic than a chain. |
| Layered | `--set graph=layered` | Wide frontiers; better GPU teaching case. |
| Random sparse | `--set graph=random` | Irregular topology and less predictable traversal. |

The default graph kind is `layered` because it produces wide frontiers and makes the CPU/GPU contrast easier to see.

## Reproduce shape comparisons with one command

Run:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

This executes `chain`, `grid`, `layered`, and `random` graph cases and writes:

```text
results\graph_bfs_shape_comparison.jsonl
```

The script defaults to `small` so the intentionally GPU-hostile chain case remains practical. Change the variables at the top of the script for heavier studies.

## Sweep layered graph sizes and plot a possible crossover

To study how the CPU/GPU relationship changes from tiny to very large layered graphs, run:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

Then plot the result:

```bat
python scripts\plot_graph_bfs_scaling.py ^
  --jsonl results\graph_bfs_layered_scale_sweep.jsonl ^
  --output-dir results\graph_bfs_layered_scale_plots ^
  --show
```

The plotter writes a timing curve, a speedup/crossover curve, and a CSV summary. It normalizes `total_ms` to **average milliseconds per BFS run** by dividing by the recorded repeat count.

See `docs/phase_03_graph_bfs_scaling.md` for the exact scale points and interpretation.

## Sweep graph anatomy and scale together

To compare whether the GPU behaves differently on chain, grid, layered, and random graphs as each family grows, run:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

Then plot:

```bat
python scripts\plot_graph_bfs_shape_scaling.py ^
  --jsonl results\graph_bfs_shape_scale_sweep.jsonl ^
  --output-dir results\graph_bfs_shape_scale_plots ^
  --show
```

This writes per-shape speedup, CPU-time, GPU-time, and frontier-anatomy plots. It is the clearest way to see that graph **anatomy**, not only node count, changes how the current GPU BFS behaves.

See `docs/phase_03_graph_bfs_shape_scaling.md` for the exact sweep points and interpretation, and `docs/phase_03_graph_bfs_frontier_anatomy.md` for the frontier-width/depth explanation.

## Example commands

Default layered graph:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1
```

Chain graph, intentionally bad GPU shape:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1 --set graph=chain
```

Grid graph:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1 --set graph=grid
```

Layered graph with custom dimensions:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset tiny --repeat 3 --warmup 1 ^
  --set graph=layered ^
  --set layers=8 ^
  --set nodes_per_layer=64 ^
  --set fanout=4
```

Random sparse graph:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 3 --warmup 1 --set graph=random
```


## Observed baseline result

During development on an RTX 1000 Ada Generation Laptop GPU, the default layered benchmark produced representative runs like:

```text
small:
  CPU  0.598 ms
  GPU  6.992 ms

large:
  CPU  22.020 ms
  GPU  25.895 ms
```

This is an intentional teaching point: the dense Phase 2 benchmarks map cleanly to GPUs, while naïve frontier BFS can lose to a strong CPU queue implementation. The relative gap narrows as the layered graph gets larger, but the transparent GPU implementation is still not a performance winner in those runs.

For the detailed interpretation, see `docs/phase_03_graph_bfs_analysis.md`.

## Validation

The GPU result is validated against the CPU queue BFS reference.

Validation checks:

```text
distance[node] exact match for every node
reached node count
max distance
checksum
mismatch count
```

The benchmark metadata includes:

```text
graph_kind
shape
source
edge_count
reached_count
max_distance
frontier_level_count
max_frontier_size
mean_frontier_size
reached_edge_visits
max_frontier_edge_visits
mean_frontier_edge_visits
mean_reached_out_degree
frontier_width_to_depth
mismatch_count
checksum
reference_checksum
frontier_iterations   # GPU row only
kernel_ms_semantics    # GPU row only
```

The frontier metrics are computed from the CPU reference BFS tree and are written for both CPU and GPU rows. They explain *why* the timing curves differ by graph anatomy.

## Strengths

GPU BFS is more interesting when:

```text
frontiers are wide
many nodes are active at the same level
CSR data is already on the GPU
BFS is part of a larger GPU pipeline
```

The layered graph is designed to show this case.

## Pitfalls

GPU BFS may disappoint when:

```text
frontiers are tiny
there are many BFS levels with little work per level
degree distribution is highly irregular
atomics become a bottleneck
host/device synchronization happens every BFS level
```

The chain graph is the deliberate bad case: each level has only one active node, so a GPU kernel launch does almost no useful parallel work.

## Teaching message

This benchmark is meant to show that “graph algorithm on GPU” is not one category. The graph shape matters.

```text
chain   -> CPU should usually be better
grid    -> mixed result
layered -> friendliest case for this initial GPU BFS, but not guaranteed to win
random  -> depends on topology and degree distribution; low-depth random graphs may expose enough frontier work for the GPU to win
```

## Shape × scale sweep labels

`execute_graph_bfs_all_sweeps_and_analyze.bat` uses explicit `--set` graph dimensions, so its `--preset tiny` argument is only a neutral fallback before those dimensions override the actual graph size. The runner passes a `sweep_label` such as `grid_256x256` or `layered_64x4096`; `graph_bfs` displays that label in the result-table `preset` column and writes it to JSONL through the ordinary `preset` field. This keeps the console output readable while the plotter still groups rows by concrete node/edge counts and graph-shape metadata.


## Canonical BFS runner

Use a single batch file for all BFS sweep, analysis, and plot generation:

```bat
execute_graph_bfs_all_sweeps_and_analyze.bat
```

This script is self-contained. It runs the layered-only sweep, the chain/grid/layered/random shape × scale sweep, the JSONL analyzer, and both plotters. Older split `execute_graph_bfs*.bat` wrappers were retired to avoid duplicated logic and batch-label errors.
