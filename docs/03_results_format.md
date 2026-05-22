# Results Format

The benchmark runner writes JSON Lines, one result per line.

Example:

```json
{"benchmark":"foundation_smoke","variant":"cpu","preset":"small","repeat":5,"warmup":1,"input_size":{"values":1000000},"total_ms":3.4,"h2d_ms":0.0,"kernel_ms":0.0,"d2h_ms":0.0,"correct":true,"device":"CPU","notes":"infrastructure smoke benchmark"}
```

## Fields

- `benchmark`: benchmark name.
- `variant`: `cpu`, `gpu`, or another named variant.
- `preset`: input-size preset.
- `repeat`: number of measured repeats.
- `warmup`: number of warmup runs.
- `input_size`: benchmark-specific dimensions.
- `total_ms`: total measured time.
- `h2d_ms`: host-to-device transfer time.
- `kernel_ms`: GPU timed compute-region time. For dense single-kernel benchmarks this is close to kernel-only time; benchmark docs clarify exceptions. `graph_bfs` uses this field for its timed traversal-loop region.
- `d2h_ms`: device-to-host transfer time.
- `correct`: correctness result.
- `device`: CPU or GPU description.
- `notes`: short implementation note.

## Why JSONL

JSONL is convenient because it can be appended during long runs, parsed line-by-line, and loaded easily from Python.


## Polynomial metadata

`polynomial_batch` adds benchmark-specific metadata:

- `coefficient_count`: currently `16`.
- `x_step`: currently `100`.
- `x_cycle`: the bounded stride-100 domain cycle length.
- `checksum`: sum of the final output vector.
- `reference_checksum`: CPU-reference sum for the same final vector.
- `max_abs_error`: largest element-wise absolute error.
- `max_rel_error`: largest element-wise relative error.
- `validation`: short description of the validator.

These fields are useful when plotting performance while still preserving evidence that CPU and GPU variants computed the same logical benchmark.


## Cost-matrix metadata

`cost_matrix` adds benchmark-specific metadata:

- `checksum`: sum of feasible final matrix costs.
- `reference_checksum`: CPU-reference sum over feasible final matrix costs.
- `feasible_count`: number of feasible final matrix cells.
- `reference_feasible_count`: CPU-reference feasible-cell count.
- `feasibility_mismatches`: count of GPU/CPU feasibility disagreements.
- `max_abs_error`: largest cell-wise absolute cost error.
- `max_rel_error`: largest cell-wise relative cost error.
- `cost_model`: compact description of the branch-heavy scoring model.
- `validation`: short description of the validator.

These fields make it possible to compare performance while confirming that branch-heavy feasibility logic stayed semantically aligned between CPU and GPU.


## Spatial-event metadata

`spatial_events` adds benchmark-specific metadata:

- `checksum`: sum of final event scores for non-`none` cells.
- `reference_checksum`: CPU-reference score sum for the same event matrix.
- `event_count`: number of non-`none` final event cells.
- `reference_event_count`: CPU-reference non-`none` count.
- `event_mismatches`: count of event-code disagreements.
- `none_count`, `enter_count`, `exit_count`, `stay_inside_count`, `cross_through_count`: actual final event-code distribution.
- `reference_none_count`, `reference_enter_count`, `reference_exit_count`, `reference_stay_inside_count`, `reference_cross_through_count`: CPU-reference event-code distribution.
- `max_abs_error`: largest cell-wise absolute score error.
- `max_rel_error`: largest cell-wise relative score error.
- `event_model`: compact description of the emitted event classes.
- `validation`: short description of the validator.

These fields make it possible to compare CPU/GPU performance while confirming that geometry classification and floating-point scoring stayed aligned.


## Graph-BFS metadata

For scale-sweep plots, scripts may normalize a timing field by repeat count. In particular, `scripts/plot_graph_bfs_scaling.py` plots:

```text
average_ms_per_BFS_run = total_ms / repeat
```

and reports:

```text
speedup = CPU_ms_per_run / GPU_ms_per_run
```

`scripts/plot_graph_bfs_shape_scaling.py` applies the same normalization while retaining graph-kind and shape metadata so chain, grid, layered, and random graph families can be compared in one report.

`graph_bfs` adds benchmark-specific metadata:

- `graph_kind`: `chain`, `grid`, `layered`, or `random`.
- `shape`: compact shape description such as layer count or grid dimensions.
- `source`: BFS source node.
- `edge_count`, `min_out_degree`, `max_out_degree`, `mean_out_degree`, `zero_out_degree_count`: graph-summary fields.
- `reached_count`: how many nodes were reachable from the source.
- `max_distance`: largest discovered BFS distance.
- `frontier_level_count`: number of reached BFS levels, usually `max_distance + 1`.
- `max_frontier_size`: largest active node frontier in any BFS level.
- `mean_frontier_size`: average active nodes per reached BFS level.
- `reached_edge_visits`: outgoing edges incident to reached nodes; a proxy for top-down BFS edge-scanning work.
- `max_frontier_edge_visits`: largest per-level outgoing-edge workload.
- `mean_frontier_edge_visits`: average outgoing-edge work per BFS level. This is the most useful compact proxy for GPU work per synchronization point.
- `mean_reached_out_degree`: average out-degree among reached nodes.
- `frontier_width_to_depth`: rough width-vs-depth ratio, computed as `max_frontier_size / max_distance`.
- `mismatch_count`: exact distance mismatches against the CPU reference.
- `checksum` and `reference_checksum`: compact final-distance signatures.
- `frontier_iterations`: GPU row only; number of BFS frontier rounds.
- `gpu_algorithm`: GPU row only; description of the level-synchronous frontier algorithm.
- `kernel_ms_semantics`: GPU row only; explains that the timed region covers the traversal loop rather than a single isolated frontier kernel.
- `validation`: short description of the distance validator.

These fields are particularly important because BFS performance depends heavily on graph topology and frontier shape. The shape-scaling plotter uses the frontier fields to show why very large random graphs can cross over while chain/grid cases stay CPU-favored.
