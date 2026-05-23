# Graph Connected Components Benchmark

Phase 3.3 adds `graph_connected_components`, a graph benchmark that finds all connected components in a CSR graph.

The goal is to demonstrate a different graph pattern from BFS. BFS follows one expanding frontier from a source. Connected components touches the whole graph and repeatedly propagates labels until every node in the same component has the same representative.

## Problem

Input:

```text
CSR graph:
    row_offsets
    column_indices
```

Output:

```text
component_label[node]
```

Nodes with the same label belong to the same connected component.

The benchmark currently uses undirected logical graphs represented as directed adjacency entries. For example, an undirected edge `u -- v` is stored as both `u -> v` and `v -> u`.

## CPU baseline

The CPU implementation uses Union-Find:

```text
for each directed edge u -> v:
    union(u, v)

for each node:
    label[node] = find(node)
```

The union operation keeps the smaller root as the representative, so labels are deterministic and easy to validate.

Strengths:

- very strong CPU baseline
- low overhead
- deterministic labels
- excellent for sparse graphs

## GPU implementation

The GPU implementation uses iterative label propagation:

```text
label[node] = node

repeat until no changes or max_iterations:
    for every node in parallel:
        inspect neighbors
        atomicMin labels toward the smallest neighboring label
    for every node in parallel:
        compress label[node] toward label[label[node]]
```

This is intentionally educational rather than state-of-the-art.

It shows the same lesson as BFS in a slightly different way:

```text
GPU can inspect many edges at once,
but convergence depth, atomics, and repeated kernel launches still matter.
```

## Graph families

The benchmark supports several disconnected graph families:

```text
chains
    many independent chain components
    high component diameter
    poor convergence shape for naive GPU label propagation

grid_components
    many independent 2D grid components
    moderate diameter and structured locality

random_clusters
    many independent clusters with a guaranteed backbone plus deterministic random edges
    extra shortcuts reduce effective diameter

mixed
    varied components: chains, stars, and ring-like structures
```

## Run examples

Default run:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1
```

Compare graph families:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=chains
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=grid_components
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=random_clusters
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset small --repeat 5 --warmup 1 --set graph=mixed
```

Custom random cluster case:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_connected_components --preset tiny --repeat 3 --warmup 1 ^
  --set graph=random_clusters ^
  --set components=32 ^
  --set component_size=256 ^
  --set random_out_degree=4
```

## Validation

The GPU labels are normalized and compared against the CPU Union-Find reference.

The benchmark records:

```text
component_count
largest_component_size
isolated_node_count
mismatch_count
checksum
reference_checksum
iterations
converged
```

A GPU row is correct only if:

```text
labels match the Union-Find reference
and the label-propagation loop converged before max_iterations
```

## Visualization

Export a small connected-components graph:

```bat
build-cuda-ninja\export_graph_components.exe --graph random_clusters --components 6 --component-size 24 --output-dir results\graph_components_demo
```

Plot it:

```bat
python scripts\plot_graph_components.py --input-dir results\graph_components_demo --show
```

Generated plots:

```text
graph_components.png
graph_component_sizes.png
```

## Shape × scale sweep

For a reproducible CPU/GPU connected-components study, use the single canonical runner:

```bat
execute_graph_connected_components_all_sweeps_and_analyze.bat
```

It runs `chains`, `grid_components`, `random_clusters`, and `mixed` across increasing sizes, then analyzes the JSONL file and produces plots.

The main result file is:

```text
results\graph_connected_components_shape_scale_sweep.jsonl
```

The plot directory is:

```text
results\graph_connected_components_shape_scale_plots\
```

The important plots are:

```text
graph_cc_shape_scaling_speedup.png
graph_cc_gpu_iterations.png
graph_cc_speedup_vs_iterations.png
```

These plots help answer two different questions:

```text
1. Where does GPU become faster than CPU, if anywhere?
2. Which graph anatomy makes the iterative GPU algorithm converge quickly enough to matter?
```

The expected pattern is that long chain components are GPU-unfriendly, while random clusters with shortcut edges are more GPU-oriented.

## Pitfalls

The GPU version can be slower than CPU because:

- Union-Find is a very strong CPU baseline.
- Label propagation may need many iterations on high-diameter components.
- Every iteration launches kernels and performs atomic operations.
- The result depends on graph anatomy, not only graph size.

Good teaching conclusion:

```text
Connected components is more global than single-source BFS,
but a naive GPU label-propagation implementation still needs favorable graph anatomy to beat a strong CPU baseline.
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
