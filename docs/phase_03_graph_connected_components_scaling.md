# Phase 3.3.1 — Connected-Components Shape × Scale Sweep

This phase adds one canonical connected-components sweep/analyze/plot runner:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

The goal is the same as the BFS scaling work: do not rely on one small timing point. Run multiple graph families across multiple sizes, then plot where the GPU remains worse, approaches parity, or crosses over.

## What the runner does

The runner performs four steps:

```text
1. Run graph_connected_components over multiple graph families and sizes.
2. Write one JSONL result file.
3. Analyze the JSONL file and produce a Markdown report + CSV summary.
4. Plot timing, speedup, and convergence behavior.
```

Output files:

```text
results\graph_connected_components_shape_scale_sweep.jsonl
results\graph_connected_components_shape_scale_analysis\
results\graph_connected_components_shape_scale_plots\
```

## Graph families

The sweep includes every connected-components graph family:

```text
chains
    Many independent chain components.
    This is deliberately GPU-unfriendly because label propagation must travel
    across high-diameter components.

grid_components
    Many independent 2D grid components.
    This has structured locality and moderate diameter.

random_clusters
    Many independent clusters with a guaranteed backbone and deterministic
    shortcut edges.
    This is usually the most GPU-oriented family because shortcuts reduce
    effective diameter and lower the number of label-propagation iterations.

mixed
    A deterministic mixture of chains, stars, and ring-like components.
    This intentionally combines GPU-friendly and GPU-unfriendly anatomy.
```

## Important variables

At the top of the batch file:

```bat
set "CC_REPEAT=3"
set "CC_WARMUP=1"
set "INCLUDE_HEAVY_CASES=0"
```

`INCLUDE_HEAVY_CASES=0` still runs all graph families. It only skips the largest stress points.

Use:

```bat
set "INCLUDE_HEAVY_CASES=1"
```

when you want a longer run that pushes larger cases.

## Plots

The plotter writes:

```text
graph_cc_shape_scaling_times.png
graph_cc_shape_scaling_speedup.png
graph_cc_gpu_iterations.png
graph_cc_speedup_vs_iterations.png
graph_cc_largest_component_size.png
graph_cc_mean_out_degree.png
graph_connected_components_shape_scaling_summary.csv
```

The most important plot is:

```text
graph_cc_shape_scaling_speedup.png
```

where:

```text
speedup = CPU ms/run / GPU ms/run
```

Interpretation:

```text
< 1.0  CPU faster
= 1.0  parity
> 1.0  GPU faster
```

The convergence plot is also important:

```text
graph_cc_gpu_iterations.png
```

because the educational GPU algorithm is iterative. A large graph with many iterations can still lose to CPU. A large graph with many edges but few convergence iterations may be more GPU-oriented.

## Expected lesson

Do not expect a simple rule like:

```text
bigger graph -> GPU wins
```

The better rule is:

```text
large graph + enough parallel edge work + low/moderate convergence depth
    -> GPU has a chance

large graph + high-diameter components + many label-propagation iterations
    -> CPU Union-Find may still dominate
```

This makes connected components another useful counterexample to naive GPU expectations, while also giving us a way to identify the graph anatomies where GPU becomes attractive.


## New explanatory plots

The connected-components scaling flow now also generates:

- `graph_cc_speedup_vs_edge_count.png`
- `graph_cc_speedup_vs_edge_iterations.png`
- `graph_cc_mixed_family_focus.png`

`edge-iterations` means:

```text
edge_count × gpu_iterations
```

This is a useful proxy for the total amount of GPU propagation work. It helps explain why some heavier cases still favor the CPU: the GPU may be scanning many edges repeatedly, while the CPU Union-Find baseline finishes in fewer effective passes.

## Mixed-family conclusion

The `mixed` family is intentionally heterogeneous. It combines chain-like, ring-like, and star-like local structures. In practice this means:

- enough work exists to keep the GPU busy,
- but not in a uniformly GPU-friendly way,
- and the CPU Union-Find baseline remains very strong.

So `mixed` is a good reminder that **large** does not automatically mean **GPU-favored**.


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
