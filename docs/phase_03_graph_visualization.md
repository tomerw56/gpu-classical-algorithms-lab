# Phase 3.1.1 - Graph Foundation Visualization Utilities

Phase 3.1 establishes CSR graph storage and deterministic graph generators. This small follow-up adds an export/plot path so those graph shapes can be inspected visually before Phase 3.2 adds BFS timing benchmarks.

## Files added

```text
apps/export_graph_foundation.cpp
scripts/plot_graph_foundation.py
docs/phase_03_graph_visualization.md
```

## Files updated

```text
CMakeLists.txt
README.md
docs/00_project_overview.md
docs/graph_foundation.md
docs/phase_03_graph_foundation.md
docs/phase_01_tests.md
```

## Export bundle

The exporter writes one inspectable bundle containing four deterministic graph families:

```bat
build-cuda-ninja\export_graph_foundation.exe --output-dir results\graph_foundation_demo
```

Output:

```text
results/graph_foundation_demo/
├── manifest.json
├── chain/
│   ├── nodes.csv
│   ├── edges.csv
│   └── graph_summary.json
├── grid/
│   ├── nodes.csv
│   ├── edges.csv
│   └── graph_summary.json
├── layered/
│   ├── nodes.csv
│   ├── edges.csv
│   └── graph_summary.json
└── random_sparse/
    ├── nodes.csv
    ├── edges.csv
    └── graph_summary.json
```

Each `nodes.csv` contains deterministic display coordinates plus out-degree metadata. Each `edges.csv` contains stored directed adjacency entries. Each `graph_summary.json` reports graph statistics and generator parameters.

## Default exported demo sizes

| Graph | Default shape | Why it is useful |
|---|---|---|
| Chain | 16 nodes | Shows a narrow frontier / one-node-at-a-time propagation shape. |
| Grid | 6 × 5 | Shows local 4-neighbor structure. |
| Layered | 4 layers × 6 nodes, fanout 2 | Shows intentionally wide traversal frontiers. |
| Random sparse | 24 nodes, out-degree 3 | Shows a deterministic less-regular topology. |

The sizes are intentionally small enough for readable diagrams. Later BFS benchmarks will reuse the same generators at much larger scales.

## Plotting

```bat
python scripts\plot_graph_foundation.py --input-dir results\graph_foundation_demo --show
```

The Python script writes:

```text
chain_graph.png
grid_graph.png
layered_graph.png
random_sparse_graph.png
degree_histogram.png
```

The four graph images show the exported topology using deterministic layouts. Node size/color reflects out-degree. The degree histogram compares the exported graph families in one view.

## Custom export sizes

```bat
build-cuda-ninja\export_graph_foundation.exe ^
  --chain-nodes 24 ^
  --grid-width 8 ^
  --grid-height 6 ^
  --layers 5 ^
  --nodes-per-layer 8 ^
  --fanout 3 ^
  --random-nodes 32 ^
  --random-out-degree 4 ^
  --output-dir results\graph_foundation_custom

python scripts\plot_graph_foundation.py --input-dir results\graph_foundation_custom
```

Keep custom display exports moderate. Very large graphs are useful for timing, but they do not produce explanatory diagrams.

## Why this matters before BFS

The upcoming BFS benchmark will compare graph shapes rather than only graph sizes. These plots make the intended contrast visible:

- chain: almost no frontier parallelism,
- grid: regular wavefront growth,
- layered: deliberately broad frontiers,
- random sparse: irregular topology with controlled degree.

The pictures therefore support the performance story instead of merely decorating it.

## Test coverage

CTest now includes:

```text
export_graph_foundation_smoke
```

It runs the exporter on small graphs and verifies that the export path remains wired into the build.
