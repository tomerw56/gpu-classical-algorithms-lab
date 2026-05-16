# Phase 3.1 - Graph Data Foundation

Phase 3 begins with graph storage rather than immediately jumping into a GPU traversal kernel. This phase adds the shared graph substrate that later BFS, connected-components, and weighted-relaxation benchmarks will reuse.

## Files added

```text
include/graph/graph_foundation.hpp
src/graph/graph_foundation.cpp
tests/test_graph_foundation.cpp
docs/graph_foundation.md
docs/phase_03_graph_foundation.md
```

Phase 3.1.1 adds graph visualization files described in `docs/phase_03_graph_visualization.md`.

## Files updated

```text
CMakeLists.txt
README.md
docs/00_project_overview.md
docs/phase_01_tests.md
```

## What this phase implements

- `DirectedEdge`
- `CsrGraph`
- `build_csr_graph()`
- `validate_csr_graph()`
- `summarize_graph()`
- `make_chain_graph()`
- `make_grid_graph()`
- `make_layered_graph()`
- `make_random_sparse_graph()`

## Why this phase exists

The upcoming GPU graph benchmarks need a graph format that is:

- flat,
- deterministic,
- easy to upload to device memory,
- reusable across several algorithms,
- precise about directed adjacency-entry counts.

CSR gives us that.

## Intended use by later phases

### Phase 3.2 - BFS

- chain graph -> intentionally narrow frontier,
- grid graph -> spatially structured frontier,
- layered graph -> intentionally wide frontier,
- random sparse graph -> less regular traversal shape.

### Phase 3.3 - Connected components

The same CSR representation can be treated as an undirected adjacency store when generated with bidirectional edges.

### Phase 3.4 - Weighted relaxation

The current CSR is unweighted, but the storage pattern and graph-shape generators establish the topology side of the problem before weighted payloads are added.

## Running tests

```bat
configure_ninja_cuda128.bat
execute_all_tests.bat
```

The new CTest entry is:

```text
test_graph_foundation
```

It validates CSR construction, malformed CSR detection, chain/grid/layered/sparse generator shapes, degree statistics, and bad-input rejection.

## Visualization follow-up

Phase 3.1.1 keeps the graph foundation phase inspectable before BFS work begins. It adds:

```text
apps/export_graph_foundation.cpp
scripts/plot_graph_foundation.py
```

The exporter writes a four-graph bundle containing chain, grid, layered, and random sparse snapshots. The plotter renders topology diagrams and an out-degree histogram. See:

```text
docs/phase_03_graph_visualization.md
```

