# Phase 3.2 — BFS / Reachability

Phase 3.2 adds the first graph traversal benchmark: `graph_bfs`.

It builds directly on the Phase 3.1 CSR foundation and uses the same deterministic graph generators that the visualization phase already exposes.

## Added files

```text
include/graph/graph_bfs.hpp
src/graph/graph_bfs_common.cpp
src/graph/graph_bfs_cpu.cpp
src/graph/graph_bfs_gpu.cu
tests/test_graph_bfs.cpp
docs/graph_bfs.md
```

## CPU algorithm

The CPU implementation is a normal queue BFS. It is the correctness reference for all BFS rows.

## GPU algorithm

The GPU implementation is a simple frontier BFS:

```text
one CUDA thread = one current-frontier node
```

Each thread scans neighbors and uses:

```text
atomicCAS -> first thread to discover a node sets its distance
atomicAdd -> append newly discovered nodes to the next frontier
```

This simple implementation is intentionally readable. It is good for teaching the algorithmic shape, not for claiming state-of-the-art GPU BFS performance.

## Why this phase matters

Graph workloads are more subtle than polynomial, cost-matrix, and spatial-event benchmarks.

Those earlier examples are mostly dense pairwise or per-item computations. BFS is different because work arrives in levels, and each level may have a very different amount of parallelism.

Phase 3.2 therefore lets us compare graph shapes:

```text
chain   -> narrow frontier, bad GPU case
grid    -> structured wavefront
layered -> wide frontier, better GPU case
random  -> irregular sparse graph
```

## Commands

Default layered graph:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1
```

Bad GPU shape:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1 --set graph=chain
```

Grid:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 5 --warmup 1 --set graph=grid
```

Random sparse:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark graph_bfs --preset small --repeat 3 --warmup 1 --set graph=random
```

## Observed first result

On the development laptop, the default layered case produced representative runs such as:

```text
small:
  CPU  0.598 ms
  GPU  6.992 ms

large:
  CPU  22.020 ms
  GPU  25.895 ms
```

So the first transparent GPU BFS is slower than the CPU queue baseline in those runs, although the gap narrows at larger size. This is not treated as a failure; it is the lesson of the phase.

## Why

The GPU implementation advances level by level and currently pays for repeated frontier orchestration:

```text
kernel launch per BFS level
frontier reset per level
atomics for discovery and append
frontier-size device-to-host control copy
```

The CPU queue BFS has much lower orchestration overhead.

For this benchmark, `kernel_ms` should be read as the timed GPU traversal-loop region, not a single frontier-kernel measurement.

## Shape-comparison runner

Run:

```bat
execute_graph_bfs_shapes.bat
```

It evaluates `chain`, `grid`, `layered`, and `random` graph shapes and writes:

```text
results\graph_bfs_shape_comparison.jsonl
```

The detailed explanation lives in `docs/phase_03_graph_bfs_analysis.md`.

## Expected lesson

This benchmark should not be read as “GPU BFS always wins.”

The expected lesson is:

```text
BFS is GPU-friendly only when there is enough frontier parallelism, and even then a transparent baseline may still lose without additional GPU-specific optimization.
```

That lesson will be important for later graph phases such as connected components and weighted relaxation.
