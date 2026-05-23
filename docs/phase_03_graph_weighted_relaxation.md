# Phase 3.4: weighted graph relaxation

This phase adds `graph_weighted_relaxation`, a CPU/GPU weighted shortest-path benchmark.

## Why after BFS and connected components?

BFS showed that unweighted traversal is very sensitive to frontier width and depth. Connected components showed that changing the GPU algorithm can dramatically change the result.

Weighted shortest path adds another important lesson:

> The CPU algorithm may be algorithmically stronger even when the GPU has more raw parallelism.

The CPU implementation uses Dijkstra. The GPU implementation uses global iterative edge relaxation.

## Variants

| variant | implementation | intent |
|---|---|---|
| `cpu` | priority-queue Dijkstra | strong CPU reference |
| `gpu` | edge-parallel iterative relaxation | simple GPU-shaped baseline |

## Interpretation

If the GPU loses, that does not mean weighted shortest paths cannot use GPUs. It means this baseline is doing repeated global edge scans, while Dijkstra avoids much of that work.

Future optimized variants could include:

- delta-stepping / bucketed relaxation
- frontier-based weighted relaxation
- multi-source or many-query batching
- graph-family-specific preprocessing

Those would be stronger GPU candidates than the deliberately simple baseline added in this phase.


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


## Weighted-relaxation backend policy

See `phase_03_graph_weighted_relaxation_backend_policy.md` for the current conclusion on CPU Dijkstra vs global GPU relaxation vs active-frontier GPU relaxation, including why the frontier method improved but still usually loses.
