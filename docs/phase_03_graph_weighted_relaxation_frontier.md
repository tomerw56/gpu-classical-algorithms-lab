# Phase 3.4.2 - Weighted relaxation frontier GPU variant

## Motivation

The first weighted-relaxation GPU implementation used a global edge scan. That is a useful educational baseline, but it is naturally slow on high-diameter graph families:

```text
for every iteration:
    scan every edge
    update distances with atomicMin
    check whether anything changed
```

The analysis report showed no GPU crossover for `chain`, `grid`, or `layered`, while `random` crossed over once the graph was large enough. This is expected: random graphs expose a lot of parallel edge work and converge in few iterations, while chain/grid graphs force many global passes.

## Added variant: `gpu-frontier`

The new `gpu-frontier` variant is not a full production shortest-path solver, but it is a more realistic improvement over the naive global scan.

It uses an active set:

```text
current_frontier = nodes whose distance changed last round
```

Each GPU iteration:

1. Expands only nodes in the current frontier.
2. Relaxes their outgoing edges with `atomicMin`.
3. Appends improved target nodes directly into the next frontier.
4. Avoids the earlier full-node flag-compaction pass that scanned every node per iteration.
5. Stops when the frontier becomes empty.

## Expected impact

The expected behavior is graph-family dependent:

| graph family | expected impact |
|---|---|
| `chain` | Much less edge work than global scan, but still many iterations and launch overhead. |
| `grid` | Should improve because only the active wavefront expands. |
| `layered` | May improve, especially when frontiers are not the whole graph. |
| `random` | May improve or regress; random graphs often activate a large fraction of the graph quickly, so global scans can already be efficient. |

## New plots

The weighted-relaxation plotter now generates:

- `graph_wr_global_speedup.png`
- `graph_wr_frontier_speedup.png`
- `graph_wr_global_to_frontier_speedup.png`
- `graph_wr_global_iterations.png`
- `graph_wr_frontier_iterations.png`
- `graph_wr_frontier_max_size.png`
- `graph_wr_global_speedup_vs_edge_iterations.png`
- `graph_wr_frontier_speedup_vs_work_estimate.png`

## Interpretation

This phase demonstrates an important GPU-design lesson:

```text
The first GPU version may be slow not because the GPU is bad,
but because the algorithm performs too much repeated global work.
```

The `gpu-frontier` variant tests whether reducing the amount of repeated work changes the conclusion.


See also: `phase_03_graph_weighted_relaxation_frontier_retrospective.md`.
