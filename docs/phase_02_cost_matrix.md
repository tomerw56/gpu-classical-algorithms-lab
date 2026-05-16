# Phase 2.2 - Branch-Heavy Cost Matrix Generation

Phase 2.2 adds the second real algorithm benchmark:

```text
cost_matrix
```

## Why this benchmark exists

Many classical optimization pipelines first build a candidate or cost matrix before solving assignment, routing, scheduling, or dispatch decisions. That matrix often contains much richer logic than Euclidean distance alone.

This phase models that situation with a branch-heavy pair-evaluation function and compares:

- CPU nested loops.
- CUDA one-thread-per-matrix-cell execution.

## Files added

```text
include/cost_matrix/cost_matrix.hpp
src/cost_matrix/cost_matrix_common.cpp
src/cost_matrix/cost_matrix_cpu.cpp
src/cost_matrix/cost_matrix_gpu.cu
tests/test_cost_matrix.cpp
apps/export_cost_matrix.cpp
scripts/plot_cost_matrix.py
docs/cost_matrix.md
docs/phase_02_cost_matrix.md
```

## Files updated

```text
CMakeLists.txt
README.md
src/common/benchmark_registry.cpp
tests/test_registry.cpp
docs/00_project_overview.md
docs/01_benchmarking_methodology.md
docs/03_results_format.md
docs/phase_01_tests.md
```

## Documentation focus

This phase now documents the parts that are most useful when reading or extending the example:

- how `make_resources()` constructs deterministic resource fields and skill masks,
- how `make_tasks()` complements those resources,
- how the dense matrix is flattened into row-major storage,
- how CPU loops and CUDA threads address the same matrix cells,
- worked examples for both infeasible and feasible task/resource pairs.

See [`docs/cost_matrix.md`](cost_matrix.md) for the detailed walkthrough.

The phase also includes a separate visualization path:

```text
export_cost_matrix -> CSV/metadata export
plot_cost_matrix.py -> heatmap, feasibility mask, 3D cost surface
```

This keeps benchmark timing separate from plotting and export overhead.

## Benchmark characteristics

The cost function includes:

- Skill compatibility filtering.
- Urgent-task dispatch limits.
- Lateness rejection.
- Urgency-dependent penalties.
- Load-tier penalties.
- Location/zone proxy penalties.

That makes it a useful contrast with the polynomial benchmark:

```text
polynomial_batch -> very regular, arithmetic-heavy
cost_matrix      -> independent cells, but branch-heavy and memory-output-heavy
```

## Validation

The benchmark validates final CPU/GPU matrices against the shared pair-evaluation reference. It checks both feasibility flags and costs. Repeats are timing repeats only.

## Running the phase

```bat
configure_ninja_cuda128.bat
execute_all_tests.bat
```

Direct run:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark cost_matrix --preset small --repeat 5 --warmup 1
```

Visual inspection run:

```bat
build-cuda-ninja\export_cost_matrix.exe --preset tiny --output-dir results\cost_matrix_tiny
python scripts\plot_cost_matrix.py --input-dir results\cost_matrix_tiny --show
```
