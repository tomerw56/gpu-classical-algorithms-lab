# Phase 3.3.3 - Non-naive GPU connected components

This patch adds a third measured connected-components variant:

```text
cpu
    CPU Union-Find reference baseline.

gpu
    Original educational GPU baseline. It is node-parallel: one thread scans a
    node's adjacency list, pushes smaller labels with atomicMin, then performs a
    one-step pointer-jumping compression pass.

gpu-non-naive
    New experimental GPU variant. It is edge-parallel: one thread processes one
    CSR edge, finds the current roots of the two endpoints, hooks the larger
    root to the smaller root with atomicMin, then performs full pointer-jumping
    compression.
```

## Why add this variant?

The naive GPU implementation is useful for teaching, but it can require many
synchronization rounds. In graph families like `chains` and `mixed`, a few
slow-converging structures can dominate the entire GPU loop.

The `gpu-non-naive` variant tries to reduce convergence rounds by operating on
component roots rather than only on current endpoint labels.

## Expected outcomes

The improved variant may help when:

- the naive implementation spends too many iterations propagating labels,
- chain-like substructures dominate convergence,
- mixed anatomy causes the slowest components to hold back the entire GPU loop.

It may hurt when:

- the graph already converges in very few iterations,
- root chasing inside each edge-thread costs more than the saved iterations,
- the CPU Union-Find baseline is already extremely fast for the graph size.

So the experiment is not expected to be a universal GPU victory. It is intended
to show the effect of algorithmic redesign.

## Running the experiment

The canonical connected-components runner automatically includes all three
variants:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

The main comparison plots are:

```text
graph_cc_shape_scaling_times.png
graph_cc_shape_scaling_speedup.png
graph_cc_naive_vs_non_naive.png
graph_cc_speedup_vs_edge_count.png
graph_cc_speedup_vs_edge_iterations.png
graph_cc_mixed_family_focus.png
```

`graph_cc_naive_vs_non_naive.png` is especially useful. Values above `1.0` mean:

```text
naive GPU time > non-naive GPU time
```

so the non-naive variant was faster.


### Connected-components GPU variant naming note

The benchmark executable still uses the variant name `gpu` for the original naive GPU implementation. In analysis files and documentation, read that as `gpu-naive`. The newer optimized experiment is `gpu-non-naive`.

The analysis CSV no longer writes a generic `gpu_ms_per_run` column, because it was only a duplicate alias for `gpu_naive_ms_per_run` and made it look like there were three GPU variants. Use these explicit columns instead:

```text
gpu_naive_ms_per_run
gpu_non_naive_ms_per_run
speedup_cpu_over_gpu_naive
speedup_cpu_over_gpu_non_naive
gpu_naive_to_non_naive_speedup
```
