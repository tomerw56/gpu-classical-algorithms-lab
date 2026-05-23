# Appendix: graph benchmark conclusions so far

## BFS

The current GPU BFS implementation is deliberately educational rather than state-of-the-art.
The sweep results show that graph *anatomy* matters as much as graph size:

- `chain` graphs are a very poor GPU match because the frontier is effectively width 1.
- `grid` graphs improve with size but still have many synchronization levels.
- `layered` graphs are more GPU-oriented because they expose wider frontiers.
- `random` graphs are the best case in our current implementation because they combine wide frontiers with small BFS depth.

## Connected components

The CPU baseline is Union-Find. The GPU baseline is iterative label propagation.
This means the GPU does not simply need a large graph; it needs a graph that offers **lots of edge work per pass** and **few passes**.

Representative conclusions from the connected-components sweep:

- `random_clusters` is the earliest and strongest GPU crossover family.
- `grid_components` becomes clearly GPU-friendly as the grids get larger.
- `chains` can eventually cross over, but only later, because the GPU needs more rounds.
- `mixed` may stay CPU-favored in the measured range because it combines chains, stars, and rings; the strong Union-Find baseline handles this mixture very efficiently, while the GPU still pays repeated global propagation rounds.

## Recommended reading of the plots

For connected components, the most explanatory plots are:

- `graph_cc_shape_scaling_speedup.png`
- `graph_cc_speedup_vs_edge_count.png`
- `graph_cc_speedup_vs_edge_iterations.png`
- `graph_cc_mixed_family_focus.png`

The last plot is especially helpful for understanding why the `mixed` family can remain CPU-favored.


## Non-naive GPU connected-components variant

The connected-components benchmark now emits three rows per benchmark point when CUDA is enabled:

```text
cpu
    CPU Union-Find reference baseline.

gpu
    Original educational GPU baseline: node-parallel label propagation using atomicMin plus one-step pointer jumping.

gpu-non-naive
    Improved educational GPU variant: edge-parallel root hooking plus full pointer-jumping compression.
```

The non-naive variant is not intended to be a final production GPU connected-components implementation. It is a second experimental step that tests whether reducing convergence rounds improves the graph families where the naive GPU version struggled, especially `chains` and `mixed`.

The canonical connected-components sweep runner includes all three variants automatically:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

The analysis report now includes first crossover points for both GPU variants, and the plots include comparisons such as:

- `graph_cc_shape_scaling_times.png`
- `graph_cc_shape_scaling_speedup.png`
- `graph_cc_naive_vs_non_naive.png`
- `graph_cc_speedup_vs_edge_count.png`
- `graph_cc_speedup_vs_edge_iterations.png`
- `graph_cc_mixed_family_focus.png`


## Weighted shortest paths

`graph_weighted_relaxation` compares CPU Dijkstra with a simple GPU Bellman-Ford-style edge relaxation. The main lesson is algorithmic: GPU-shaped parallelism is not always enough. Dijkstra performs selective priority-queue expansion, while the GPU baseline repeatedly scans all edges until convergence.

This phase sets up future optimized variants such as delta-stepping, bucketed relaxation, or many-query batching.


## Weighted-relaxation backend policy

See `phase_03_graph_weighted_relaxation_backend_policy.md` for the current conclusion on CPU Dijkstra vs global GPU relaxation vs active-frontier GPU relaxation, including why the frontier method improved but still usually loses.


## Weighted relaxation delta-stepping experiment

The weighted-relaxation phase now includes a fourth backend row:

```text
gpu-delta-stepping
```

This is a bucketed active-relaxation experiment inspired by delta stepping. It is compared against CPU Dijkstra, the original global GPU edge scan, and the active frontier GPU variant. The analysis scripts and backend-recommendation flow now include delta-stepping columns and plots.

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
