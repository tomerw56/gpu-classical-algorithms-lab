# Phase 3.4 final weighted-relaxation conclusions

This chapter intentionally became a counterexample chapter.

The weighted shortest-path benchmark compares:

- `cpu` - Dijkstra with a priority queue.
- `gpu` - global Bellman-Ford-style full edge scan.
- `gpu-frontier` - active-frontier relaxation.
- `gpu-delta-stepping` - simplified bucketed relaxation.
- `gpu-delta-light-heavy` - educational light/heavy delta-style variant.

## Current conclusion

The measured backend policy is:

| Graph family | Recommended backend | Why |
|---|---|---|
| `chain` | CPU Dijkstra | Very high diameter; GPU relaxation needs many rounds. |
| `grid` | CPU Dijkstra | Local wavefront structure creates many relaxation rounds. |
| `layered` | CPU Dijkstra in the measured range | More GPU-friendly than chain/grid, but Dijkstra remains strong. |
| small `random` | CPU Dijkstra | Too small to amortize GPU overhead. |
| large `random` | `gpu` global scan | Low effective diameter and many edges per round expose enough GPU work. |

The key lesson is not that weighted graph problems are bad for GPU. The lesson is that simple GPU adaptations are not enough. Weighted SSSP needs either a graph anatomy that exposes lots of parallel edge work with few rounds, or a much more sophisticated GPU graph algorithm.

## Why the final delta variants did not win

The delta variants add ordering and bucket structure, but they also add:

- bucket-management overhead,
- additional queues/frontiers,
- more synchronization,
- more kernel work,
- and more bookkeeping than the simple global scan.

For the current synthetic graph families and weight range, that overhead is larger than the saved work.

## Very-very-large random stress point

The canonical runner now includes an explicit very-very-large random graph point:

```bat
set "INCLUDE_VERY_VERY_LARGE_RANDOM=1"
set "VERY_VERY_LARGE_RANDOM_NODES=1048576"
set "VERY_VERY_LARGE_RANDOM_REPEAT=1"
```

This point is included to make the random-family conclusion definitive: if the graph is large, random-like, and low-diameter, the simple `gpu` global scan can be a good backend.

This stress point is intentionally separate from `INCLUDE_HEAVY_CASES`; chain/grid heavy cases can be very slow, while the very large random case is the family where GPU is expected to win.

## Recommended lecture message

Use this chapter to say:

> GPU is not a magic graph accelerator. On weighted shortest paths, CPU Dijkstra can beat several GPU attempts on chain/grid/layered graphs. The GPU only clearly wins in the large random family where the graph anatomy exposes massive parallel edge work with few convergence rounds.

This is valuable because it prevents the team from making a hardware decision based only on problem category names such as “graph theory” or “shortest path.”
