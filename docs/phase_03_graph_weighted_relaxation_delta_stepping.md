# Phase 3.4.5 - Weighted relaxation: delta-stepping experiment

This phase adds a third GPU weighted-shortest-path experiment:

```text
gpu-delta-stepping
```

The existing variants remain:

```text
cpu           Dijkstra with a priority queue
gpu           global Bellman-Ford-style full edge scan
gpu-frontier  active frontier relaxation
gpu-delta-stepping bucketed active relaxation inspired by delta stepping
```

## Why add it?

The previous `gpu-frontier` variant reduced some wasted full-edge scanning, but it still did not consistently beat the global GPU scan or CPU Dijkstra. The reason was algorithmic: active frontiers alone do not provide enough ordering for weighted shortest paths.

Delta stepping adds a simple ordering idea: distances are grouped into buckets of width `delta`.

```text
bucket 0: distances [0, delta)
bucket 1: distances [delta, 2*delta)
bucket 2: distances [2*delta, 3*delta)
...
```

The new implementation processes the current bucket until it is locally quiet, while deferring farther-distance work to future buckets.

## Implementation used here

This is an educational CUDA implementation, not a production-grade SSSP library.

It uses:

```text
current bucket frontier
future frontier list
atomicMin distance updates
bucket selection from deferred nodes
exact validation against CPU Dijkstra
```

The important behavior is that `gpu-delta-stepping` should avoid some useless work from the global full-edge scan, while adding less unordered noise than the plain frontier queue.

## Tunable parameter

The benchmark accepts:

```bat
--set delta=16
```

A larger delta creates fewer buckets but more work inside each bucket. A smaller delta creates more buckets and more ordering, but also more synchronization.

## Expected results

Do not assume delta stepping will always win. The expected result depends on graph family:

- `random`: may already be handled well by the global scan; delta stepping may or may not improve it.
- `layered`: likely an interesting case because there is structure but also significant parallelism.
- `grid`: may improve, but weighted diameter can still create many bucket transitions.
- `chain`: still difficult for GPU because there is little parallelism.

The single weighted-relaxation runner now includes all four backends in the same analysis and plots.


## Delta-stepping scheduler fix

See `docs/phase_03_graph_weighted_relaxation_delta_scheduler_fix.md` for the fix that prevents the `gpu-delta-stepping` variant from stopping early on high-diameter chain/grid-style cases. The result table was also widened so `gpu-delta-stepping` is displayed cleanly.


See also: `phase_03_graph_weighted_relaxation_delta_tuning.md` for the delta-width tuning workflow.
