# Phase 1.3 — Infrastructure Tests

Phase 1.3 expands the CTest suite around the benchmark infrastructure.

The tests are intentionally lightweight and dependency-free. They use a small local assertion helper in `tests/test_support.hpp` rather than a full testing framework. This keeps the CUDA/MSVC/Ninja workflow simple while the project foundation is still changing.

## Test executables

CTest currently runs these executables:

| Test | Purpose |
|---|---|
| `test_foundation` | Checks `foundation_smoke` presets, CPU result metadata, correctness, and repeat/checksum policy. |
| `test_polynomial` | Checks polynomial presets, 16-coefficient layout, stride-100 x policy, Horner correctness, benchmark metadata, and repeat/checksum policy. |
| `test_cost_matrix` | Checks cost-matrix presets, deterministic task/resource generation, forced feasible/infeasible pairs, result metadata, and repeat/checksum policy. |
| `test_spatial_events` | Checks spatial-event presets, deterministic tracks/zones, forced enter/exit/stay/cross/none cases, result metadata, and repeat/checksum policy. |
| `test_graph_foundation` | Checks CSR building/validation, chain/grid/layered/sparse graph generators, degree statistics, and bad-input rejection. |
| `test_graph_bfs` | Checks CPU queue BFS distances on chain/grid/disconnected graphs, tiny benchmark metadata, and bad-source rejection. |
| `test_graph_connected_components` | Checks Union-Find labels, normalization, disconnected graph families, tiny benchmark metadata, and bad-input rejection. |
| `test_registry` | Checks default benchmark registration, `all`, named benchmark execution, and unknown benchmark errors. |
| `test_cli` | Checks command-line parsing, flags, repeated `--set`, and invalid argument handling. |
| `test_json_writer` | Checks JSONL serialization, escaping, append/truncate behavior, and table printing. |
| `test_random_utils` | Checks deterministic seeded random vectors and bounds. |
| `test_device_info` | Checks CPU device string and CUDA runtime-info behavior in both CPU-only and CUDA builds. |

## Running tests

From the build directory:

```bat
ctest --output-on-failure
```

From the repository root, use the wrapper:

```bat
execute_all_tests.bat
```

The wrapper now prints the discovered CTest tests first:

```bat
ctest --test-dir "<build-dir>" -N
```

Then it runs them with verbose output:

```bat
ctest --test-dir "<build-dir>" --output-on-failure --verbose
```

## Correctness policy covered by tests

The foundation, polynomial, cost-matrix, and spatial-event tests verify that `--repeat` is a timing setting only. The reported checksum must describe the final single output vector, not the sum of all repeated executions.

This matters because CPU and GPU benchmark rows must describe the same logical computation. Different checksum semantics would make them different benchmarks.

## Future testing direction

When the real algorithm benchmarks are added, each one should include:

1. Tiny deterministic correctness cases.
2. CPU/GPU equivalence checks.
3. Bad-input tests.
4. Preset-size tests.
5. Benchmark-result metadata tests.

For now, the local assertion helper is enough. Later, we can move to Catch2 or GoogleTest if the test suite grows significantly.


## Exporter smoke coverage

Phase 2.2 adds a small CTest entry named `export_cost_matrix_smoke`. It runs the CSV exporter on a `4 x 5` matrix and verifies through process success that the inspectable cost-matrix export path remains wired into the build.

Phase 2.3 adds `export_spatial_events_smoke`. It runs the spatial exporter on a `5 tracks x 4 zones` scene and verifies through process success that the track/zone visualization export path remains wired into the build.

Phase 3.1.1 adds `export_graph_foundation_smoke`. It exports small chain, grid, layered, and random sparse graph snapshots and verifies through process success that the graph visualization bundle path remains wired into the build.

Phase 3.3 adds `export_graph_components_smoke`. It exports a small random-cluster connected-components graph and verifies through process success that the component-label visualization export path remains wired into the build.


## Graph foundation coverage

Phase 3.1 adds `test_graph_foundation`. Phase 3.2 adds `test_graph_bfs`, which validates the queue BFS reference and the registered benchmark metadata before relying on it for CPU/GPU timing comparisons. Phase 3.3 adds `test_graph_connected_components`, which validates Union-Find labels, label normalization, and component-family generation before relying on them for CPU/GPU comparisons.


## Phase 3.4 weighted-relaxation tests

- `test_graph_weighted_relaxation` checks deterministic weighted graph construction, CPU Dijkstra validation, and benchmark metadata for `graph_weighted_relaxation`.


## Weighted-relaxation final stress point

The weighted-relaxation runner includes an optional/default very-very-large random stress point. This is not a unit test; it is a benchmark evidence point for the final weighted-shortest-path conclusion.


## Constraint-network tests

`test_constraint_network` checks the direct constraint evaluator, deterministic problem generation, CPU reference validation, and benchmark metadata.


- `docs/phase_04_constraint_network_validation_fix.md` - explains the Phase 4.1 GPU validation tolerance fix for the constraint-network benchmark.


The constraint-network sweep runner also invokes `scripts/plot_constraint_network_diagnostics.py` to produce data-understanding plots from the JSONL results.

## Combination finder tests

Phase 4.2 adds `test_combination_finder`, which checks:

- small binomial counts,
- lexicographic combination unranking,
- CPU aggregate correctness,
- benchmark result metadata for a tiny custom case.

## Assignment preprocessing tests

Phase 4.3 adds `test_assignment_preprocessing`, which checks:

- custom task/resource/top-K shape parsing,
- deterministic synthetic problem generation,
- CPU top-K reference output size and aggregate counts,
- self-validation of selected resources and costs,
- benchmark metadata for a tiny custom case.

It also adds `export_assignment_preprocessing_smoke`, which exports a small task/resource problem and verifies that the feasibility/cost/top-K visualization data path remains wired into the build.


## Local-search move tests

Phase 4.4 adds `test_local_search_moves`, which checks:

- deterministic local-search task/resource/move generation,
- CPU reference evaluation of candidate replacement moves,
- aggregate counts for valid, invalid, and improving moves,
- self-validation of move deltas and violation masks,
- benchmark metadata for a tiny custom case.

It also adds `export_local_search_moves_smoke`, which exports a small current-assignment/move problem and verifies that the local-search problem visualization data path is wired into the build.


## Scenario simulation tests

Phase 4.5 adds:

- `test_scenario_simulation`
- `export_scenario_simulation_smoke`

These verify the CPU reference evaluator, aggregation/validation metadata, registry integration, and the scenario problem exporter.


## Scenario simulation sweep runtime note

The scenario-simulation default runner stops at `sc_4m`; `sc_16m` was removed from the normal flow because it was too slow for the value it added. This does not affect CTest coverage.


## Scenario simulation calibration test note

The scenario simulation tests continue to validate CPU-side agreement. The calibrated generator is designed so analysis reports can detect degenerate all-feasible or all-infeasible sweeps.


Scenario simulation feasibility calibration v2 is documented in `docs/phase_04_scenario_simulation_feasibility_calibration_v2.md`.
