# Graph Foundation: CSR Layout and Deterministic Generators

Phase 3 starts by fixing the graph representation that the later CPU/GPU graph benchmarks will share. We do this before adding BFS so the traversal algorithms do not each invent their own data layout.

## Why CSR?

The project uses **Compressed Sparse Row** storage:

```text
row_offsets
column_indices
```

For a node `u`, its outgoing neighbors are stored in:

```text
column_indices[row_offsets[u] ... row_offsets[u + 1])
```

### Small example

Suppose the directed graph is:

```text
0 -> 1, 2
1 -> 2
2 -> none
```

Then CSR is:

```text
node_count     = 3
row_offsets    = [0, 2, 3, 3]
column_indices = [1, 2, 2]
```

Interpretation:

- node `0`: neighbors `column_indices[0..2)` -> `1, 2`
- node `1`: neighbors `column_indices[2..3)` -> `2`
- node `2`: neighbors `column_indices[3..3)` -> none

CSR is a strong fit for the upcoming GPU work because adjacency is stored in flat contiguous arrays rather than pointer-heavy nested containers.

## Directed adjacency entries

The graph foundation stores **directed adjacency entries**. If an algorithm conceptually needs an undirected edge between `u` and `v`, the generator emits two directed entries:

```text
u -> v
v -> u
```

This keeps the storage model explicit and avoids ambiguity when later graph algorithms report edge counts.

## Public types

```cpp
struct DirectedEdge
{
    std::int32_t source;
    std::int32_t target;
};

struct CsrGraph
{
    std::int32_t node_count;
    std::vector<std::int64_t> row_offsets;
    std::vector<std::int32_t> column_indices;
};
```

`CsrGraph` also exposes:

```cpp
edge_count()
out_degree(node)
```

## CSR builder

```cpp
build_csr_graph(node_count, edges)
```

The builder:

1. validates source and target ids,
2. sorts adjacency entries by `(source, target)`,
3. removes duplicate directed edges by default,
4. constructs row offsets and flattened neighbor storage,
5. validates the finished graph before returning it.

### Builder example

Input edges:

```text
0 -> 2
0 -> 1
0 -> 1   duplicate
1 -> 2
```

Stored CSR after sorting and duplicate removal:

```text
row_offsets    = [0, 2, 3, 3]
column_indices = [1, 2, 2]
```

## Validation and statistics

```cpp
validate_csr_graph(graph)
summarize_graph(graph)
```

Validation checks:

- `row_offsets.size() == node_count + 1`,
- first row offset is zero,
- row offsets are non-decreasing,
- final row offset equals `column_indices.size()`,
- every target node id is in range.

Summary statistics report:

- node count,
- directed adjacency-entry count,
- min/max/mean out-degree,
- count of zero-out-degree nodes.

Those fields will become useful metadata for BFS and connected-components benchmark rows later.

## Deterministic graph generators

### 1. Chain graph

```cpp
make_chain_graph(node_count, bidirectional)
```

The bidirectional version stores:

```text
0 <-> 1 <-> 2 <-> 3 ...
```

Why it matters: this graph creates a **narrow BFS frontier**. It is a useful counterexample where a GPU frontier-based BFS should not be expected to shine.

### 2. Grid graph

```cpp
make_grid_graph(width, height, bidirectional)
```

Node ids are row-major:

```text
node_id = y * width + x
```

The generator creates 4-neighbor right/down adjacency, and the bidirectional form also stores the reverse directions. This will be useful for spatially structured BFS examples.

Example: a `3 x 2` bidirectional grid has:

```text
6 nodes
14 directed adjacency entries
```

### 3. Layered graph

```cpp
make_layered_graph(layer_count, nodes_per_layer, fanout)
```

Each node in layer `L` connects to `fanout` deterministic nodes in layer `L + 1`.

Why it matters: this graph deliberately creates **wide BFS frontiers**, which is the shape that can better expose GPU parallelism in Phase 3.2.

Example:

```text
3 layers
4 nodes per layer
fanout 2
```

produces:

```text
12 nodes
16 directed adjacency entries
```

The last layer has zero out-degree.

### 4. Deterministic sparse graph

```cpp
make_random_sparse_graph(node_count, out_degree, seed)
```

This is pseudo-random but repeatable:

- no self-loops,
- exact fixed out-degree,
- deterministic seed,
- duplicate targets avoided.

Why it matters: this gives future graph benchmarks a less structured workload than chain/grid/layered cases while still remaining reproducible.

## Pitfalls this phase avoids

### Pointer-heavy adjacency containers

A `std::vector<std::vector<int>>` representation is convenient on CPU, but it is not the representation we want to upload to GPU kernels. CSR fixes the storage contract early.

### Confusing directed and undirected counts

A logical undirected edge is two stored directed entries here. The docs and tests consistently report **stored adjacency entries**.

### Hidden duplicate edges

The builder removes duplicate directed edges by default. This keeps degree counts predictable and avoids accidental duplicate work during BFS.

### Non-deterministic graph generation

Every generator is deterministic. Later CPU/GPU benchmark comparisons will therefore traverse the same graph topology every time.

## Export and visualization utilities

Phase 3.1.1 adds an inspectable graph-export bundle:

```bat
build-cuda-ninja\export_graph_foundation.exe --output-dir results\graph_foundation_demo
```

The exporter writes four graph directories under the output folder:

```text
chain/
grid/
layered/
random_sparse/
```

Each directory contains:

```text
nodes.csv
edges.csv
graph_summary.json
```

The `nodes.csv` file includes deterministic display coordinates plus out-degree metadata. `edges.csv` contains stored directed adjacency entries. `graph_summary.json` captures graph statistics and the generator parameters used to create the snapshot.

Plot the exported graphs with:

```bat
python scripts\plot_graph_foundation.py --input-dir results\graph_foundation_demo --show
```

The plotter writes:

```text
chain_graph.png
grid_graph.png
layered_graph.png
random_sparse_graph.png
degree_histogram.png
```

These diagrams are intentionally small and explanatory. They help show why a chain is a poor frontier-parallelism case, why a layered graph is more GPU-friendly for future BFS work, and how the degree profiles differ across the demo topologies.

## What comes next

Phase 3.2 will place traversal code on top of this foundation:

```text
CPU queue BFS
GPU frontier BFS
single-source and selected multi-source cases
```

The chain, grid, layered, and sparse generators in this phase are intentionally chosen so Phase 3.2 can show both favorable and unfavorable shapes for GPU BFS.
