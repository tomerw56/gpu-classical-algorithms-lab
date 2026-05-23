# Graph weighted relaxation benchmark

`graph_weighted_relaxation` is Phase 3.4 of the graph section. It compares two ways of computing weighted single-source shortest-path distances on positive integer edge weights.

## What problem is being solved?

Given a weighted directed CSR graph and a source node, compute:

```text
distance[v] = minimum total edge cost from source to v
```

The benchmark uses the same qualitative graph families as the BFS study:

- `chain`
- `grid`
- `layered`
- `random`

Each CSR adjacency entry receives a deterministic positive integer weight.

## CPU baseline: Dijkstra

The CPU implementation is a conventional priority-queue Dijkstra reference.

This is a strong CPU baseline for positive weights because it avoids repeated global scans. It expands the currently best-known node and relaxes only its outgoing neighbors.

## GPU baseline: iterative edge relaxation

The GPU implementation is deliberately GPU-shaped but algorithmically simple:

```text
repeat until no changes or max_iterations:
    in parallel, for every edge (u -> v):
        if distance[u] + weight(u,v) < distance[v]:
            atomicMin(distance[v], candidate)
```

This is closer to Bellman-Ford-style global relaxation than to Dijkstra. It exposes a lot of parallel edge work, but it may require many global iterations on high-diameter graphs.

## Why this phase matters

Weighted shortest path is a good warning example:

```text
Single-query Dijkstra is naturally CPU-friendly.
Naive global edge relaxation is GPU-friendly in shape but may do much more total work.
```

The benchmark teaches that a GPU version can look worse even though it has more parallelism, because it trades a strong sequential algorithm for many repeated global edge scans.

## Expected behavior

- `chain`: poor GPU case because shortest-path information must move along the chain.
- `grid`: may improve with size, but the number of relaxation rounds can still be large.
- `layered`: better GPU candidate because useful work is organized by layers.
- `random`: can be a better GPU candidate if low effective diameter allows convergence in few iterations.

## Useful commands

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 5 --warmup 1 --set graph=layered
```

Compare graph families:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 3 --warmup 1 --set graph=chain
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 3 --warmup 1 --set graph=grid
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 3 --warmup 1 --set graph=layered
build-cuda-ninja\gpu_algobench.exe --benchmark graph_weighted_relaxation --preset small --repeat 3 --warmup 1 --set graph=random
```

## Metadata

Rows include:

- `graph_kind`
- `shape`
- `source`
- `edge_count`
- `reached_count`
- `max_finite_distance`
- `mismatch_count`
- `checksum`
- `reference_checksum`
- `iterations`
- `converged`

For the GPU row, `kernel_ms` means the timed relaxation loop, including repeated global edge scans and the host-side convergence checks.


## Sweep / analyze / plot flow

Use the single canonical weighted-relaxation runner:

```bat
execute_graph_weighted_relaxation_all_sweeps_and_analyze.bat
```

It runs chain, grid, layered, and random weighted graph families across increasing sizes, then generates CSV/Markdown analysis and plots.

The most important diagnostic is `edge_count × gpu_iterations`, because the GPU algorithm performs repeated global edge-relaxation passes. Large graphs only become GPU-friendly when they provide enough useful edge work per pass and converge in relatively few passes.


## Convergence guard note

The GPU relaxation loop needs one extra no-change pass to prove convergence.
For a chain with `N` nodes, the farthest shortest path has `N - 1` edges, but
the convergence flag is only observed on the following pass. Therefore the
default chain guard is `N` passes, not `N - 1`. This prevents a case where the
distance vector is already correct but the row is marked `correct=no` only
because convergence was not yet observed.

## Why the first GPU method can be slower

The original `gpu` variant is intentionally simple: it performs a global Bellman-Ford-style edge relaxation. Each GPU iteration scans every edge, even when only a small portion of the graph has an improved distance. This explains the common result where CPU Dijkstra is faster on chain, grid, and some layered cases.

The useful rule is:

```text
GPU-global work ~= edge_count × iterations
```

For high-diameter graphs, this can explode. A chain with 4096 nodes may require thousands of relaxation rounds even though each round has very little useful new information.

## New `gpu-frontier` variant

The `gpu-frontier` variant is a more targeted GPU experiment. It keeps an active frontier of nodes whose distances changed. Each iteration only expands those nodes, relaxes their outgoing edges, and appends improved targets directly into the next frontier. This replaced an earlier full-node flag-compaction prototype, which turned out to be slower because it scanned all nodes every iteration.

Conceptually:

```text
current_frontier = {source}
while current_frontier is not empty:
    relax outgoing edges of current_frontier nodes
    every improved target joins next_frontier
    current_frontier = directly_appended_improved_targets
```

This avoids repeated full-graph scans on graph families where only a small wavefront is active at a time, especially chains and grids.

It is still not a production-grade delta-stepping implementation. It has overhead from per-round kernel launches, atomic appends, possible duplicate frontier entries, and host-side frontier-size checks. It can still be worse than global scanning on low-diameter random graphs where most of the graph becomes active quickly.

## Variants

| variant | meaning |
|---|---|
| `cpu` | CPU Dijkstra with a priority queue. |
| `gpu` | Global Bellman-Ford-style edge scan using `atomicMin`. |
| `gpu-frontier` | Active-frontier GPU relaxation with direct append of improved targets. |

The analyzer and plotter compare all three variants.


See also: `phase_03_graph_weighted_relaxation_frontier_retrospective.md`.


## Weighted-relaxation backend policy

See `phase_03_graph_weighted_relaxation_backend_policy.md` for the current conclusion on CPU Dijkstra vs global GPU relaxation vs active-frontier GPU relaxation, including why the frontier method improved but still usually loses.


## Weighted relaxation delta-stepping experiment

The weighted-relaxation phase now includes a fourth backend row:

```text
gpu-delta-stepping
```

This is a bucketed active-relaxation experiment inspired by delta stepping. It is compared against CPU Dijkstra, the original global GPU edge scan, and the active frontier GPU variant. The analysis scripts and backend-recommendation flow now include delta-stepping columns and plots.


## Delta-stepping scheduler fix

See `docs/phase_03_graph_weighted_relaxation_delta_scheduler_fix.md` for the fix that prevents the `gpu-delta-stepping` variant from stopping early on high-diameter chain/grid-style cases. The result table was also widened so `gpu-delta-stepping` is displayed cleanly.


See also: `phase_03_graph_weighted_relaxation_delta_tuning.md` for the delta-width tuning workflow.

## Weighted SSSP final attempt: `gpu-delta-light-heavy`

The weighted-relaxation chapter includes one final GPU attempt named
`gpu-delta-light-heavy`. It is a delta-stepping-style experiment that separates
light edges (`weight <= delta`) from heavy edges (`weight > delta`). It closes a
bucket with light edges first, then relaxes heavy edges outward.

This variant is intentionally treated as the final educational attempt for the
weighted shortest-path section. If it does not beat CPU Dijkstra or the simpler
`gpu` global scan, the documented conclusion is that weighted SSSP requires a
more serious GPU graph algorithm or a mature graph library rather than a small
frontier/bucket tweak.


## Final weighted-relaxation conclusion

See `docs/phase_03_graph_weighted_relaxation_final_conclusions.md` for the final conclusion of the weighted-shortest-path experiment. The canonical runner includes a very-very-large random stress point controlled by:

```bat
set "INCLUDE_VERY_VERY_LARGE_RANDOM=1"
set "VERY_VERY_LARGE_RANDOM_NODES=1048576"
set "VERY_VERY_LARGE_RANDOM_REPEAT=1"
```

The final expected policy is: CPU Dijkstra for chain/grid/layered and small random graphs; global GPU scan for sufficiently large random graphs. The frontier and delta variants remain useful educational counterexamples.
