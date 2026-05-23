# Phase 3.3 — Connected Components

Phase 3.3 adds `graph_connected_components`.

This benchmark compares:

```text
CPU: Union-Find
GPU: iterative label propagation with atomicMin and pointer-jumping compression
```

## Why this phase matters

BFS showed that a graph algorithm may be hard for a naive GPU implementation because of frontier depth and synchronization.

Connected components is a useful next graph example because it has a different shape:

- it touches the whole graph, not just one source frontier
- every component can be processed concurrently in principle
- it still needs convergence, atomics, and repeated synchronization

So it teaches a more nuanced point:

```text
GPU graph algorithms are not just about graph size.
They depend on component diameter, degree distribution, and convergence behavior.
```

## Implemented graph families

```text
chains
    many independent chain components

grid_components
    many independent grid components

random_clusters
    many independent random-ish clusters with a guaranteed backbone

mixed
    deterministic mixture of chains, stars, and ring-like components
```

## Commands

Default benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1
```

Family comparison:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=chains
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=grid_components
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=random_clusters
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=mixed
```

Visualization:

```bat
build-cuda-ninja\export_graph_components.exe --graph random_clusters --components 6 --component-size 24 --output-dir results\graph_components_demo
python scripts\plot_graph_components.py --input-dir results\graph_components_demo --show
```

## Expected observations

- Chain components are a bad case for label propagation because the minimum label moves slowly along long paths.
- Grid components are better but still require many propagation rounds for large grids.
- Random clusters can converge faster because shortcut edges reduce effective diameter.
- CPU Union-Find is very hard to beat for moderate sparse graphs.

## Files

```text
include/graph/graph_connected_components.hpp
src/graph/graph_connected_components_common.cpp
src/graph/graph_connected_components_cpu.cpp
src/graph/graph_connected_components_gpu.cu
tests/test_graph_connected_components.cpp
apps/export_graph_components.cpp
scripts/plot_graph_components.py
docs/graph_connected_components.md
```


## Scaling and crossover runner

Phase 3.3.1 adds a single connected-components scaling runner:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

This replaces manual one-off family comparisons when the goal is to understand performance. It runs all connected-components graph families across increasing sizes, analyzes the resulting JSONL file, and generates plots.

Generated files:

```text
results\graph_connected_components_shape_scale_sweep.jsonl
results\graph_connected_components_shape_scale_analysis\
results\graph_connected_components_shape_scale_plots\
```

The key teaching point is the same as in BFS: graph size alone is not enough. Component diameter, convergence iterations, and graph family strongly affect whether the GPU label-propagation implementation can beat CPU Union-Find.

See also:

```text
docs/phase_03_graph_connected_components_scaling.md
```


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
