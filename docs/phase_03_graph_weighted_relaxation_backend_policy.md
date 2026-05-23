# Phase 3.4.4 - Weighted-relaxation backend policy

The weighted-relaxation benchmark now intentionally keeps three rows in the
same sweep:

| Variant | Meaning |
|---|---|
| `cpu` | CPU Dijkstra with a priority queue. |
| `gpu` | Global Bellman-Ford-style GPU edge scan. |
| `gpu-frontier` | Active-frontier GPU relaxation. |

The latest measurements show that `gpu-frontier` improved after removing the
full-node compaction pass, but it still usually does not beat the global GPU
scan or CPU Dijkstra.

## Why the frontier variant still struggles

The frontier variant reduces wasted edge scans, but it still pays for:

- one kernel launch / host synchronization per relaxation round,
- atomic appends into the next frontier,
- duplicate frontier entries when several sources improve the same target,
- many rounds on high-diameter families such as `chain` and `grid`.

For `chain`, the frontier size is essentially one node per round. That means the
GPU performs thousands of tiny rounds. The amount of useful arithmetic is small,
so launch/synchronization overhead dominates.

For `grid`, the frontier is larger, but the weighted shortest path wavefront
still requires many rounds. CPU Dijkstra is a strong baseline here.

For `random`, the graph has enough edge parallelism and low enough effective
relaxation depth that the global GPU edge scan can win strongly.

## Practical conclusion

The best practical policy from this benchmark is not "always GPU". It is:

```text
chain / grid / layered in this measured range:
    use CPU Dijkstra

large low-diameter random sparse graphs:
    use global GPU edge relaxation

gpu-frontier:
    keep as an educational experiment, not the default winner
```

## New recommendation script

The weighted-relaxation flow now runs:

```bat
scripts\recommend_graph_weighted_relaxation_backend.py
```

This reads the analysis CSV and writes:

```text
results\graph_weighted_relaxation_backend_recommendations\
    graph_weighted_relaxation_backend_recommendations.csv
    graph_weighted_relaxation_backend_recommendations.md
    graph_wr_backend_recommendations.png
```

The report names the fastest measured backend for each graph family/size and
explains why.

## Next serious GPU direction

The next algorithmic step should not be another small frontier tweak. Better
candidates are:

1. **Delta-stepping / bucketed SSSP** for positive weights.
2. **Graph-family-specific solvers**, such as prefix/scan-style logic for pure
   chain-like graphs.
3. **Tiled/local methods** for grids.
4. **Backend selection**, where graph anatomy decides whether to run CPU,
   global GPU scan, or a specialized GPU method.

This phase therefore becomes another useful negative result: a generic active
frontier is not enough to beat a strong CPU Dijkstra baseline on all weighted
shortest-path shapes.
