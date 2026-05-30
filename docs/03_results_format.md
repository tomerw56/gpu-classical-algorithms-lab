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


## Graph connected-components metadata

`graph_connected_components` adds metadata fields such as:

```text
graph_kind
shape
component_count
largest_component_size
isolated_node_count
mismatch_count
checksum
reference_checksum
iterations
converged
```

For the GPU row, `kernel_ms` describes the timed label-propagation convergence loop, not one isolated kernel. The loop may launch multiple hook/compress kernels until labels stop changing or `max_iterations` is reached.


## Connected-components scaling summaries

The script `plot_graph_connected_components_scaling.py` writes `graph_connected_components_shape_scaling_summary.csv`. It normalizes benchmark `total_ms` by `repeat`, then reports CPU/GPU milliseconds per run, CPU/GPU speedup, GPU convergence iterations, component counts, largest component size, and mean out-degree.


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


## `graph_weighted_relaxation` metadata

The weighted shortest-path benchmark writes metadata including:

- `graph_kind`
- `shape`
- `source`
- `edge_count`
- `reached_count`
- `max_finite_distance`
- `iterations`
- `converged`
- `mismatch_count`
- `checksum` / `reference_checksum`

For the GPU row, `kernel_ms` represents the timed iterative relaxation loop, including repeated global edge scans and host convergence checks. It is not a single isolated kernel duration.


## Weighted-relaxation sweep outputs

The canonical weighted graph flow writes:

```text
results\graph_weighted_relaxation_shape_scale_sweep.jsonl
```

Each benchmark point contains CPU and GPU rows. Important metadata fields include:

```text
graph_kind
shape
iterations
max_iterations
converged
reached_count
max_finite_distance
mean_out_degree
mismatch_count
```

The analysis script derives:

```text
gpu_edge_iterations = edge_count × gpu_iterations
speedup_cpu_over_gpu = cpu_ms_per_run / gpu_ms_per_run
```


## Weighted-relaxation backend recommendation outputs

The weighted-relaxation runner writes an additional recommendation folder:

```text
results/graph_weighted_relaxation_backend_recommendations/
```

Important files:

- `graph_weighted_relaxation_backend_recommendations.csv`
- `results\graph_weighted_relaxation_backend_recommendations\graph_weighted_relaxation_backend_recommendations.md`
- `graph_wr_backend_recommendations.png`

These files summarize the fastest measured backend among the weighted-relaxation variants that are present in the JSONL, including `cpu`, `gpu`, `gpu-frontier`, `gpu-delta-stepping`, and `gpu-delta-light-heavy`.


## Weighted-relaxation delta-stepping fields

The weighted-relaxation analyzer may emit the following delta-stepping columns:

```text
gpu_delta_ms_per_run
speedup_cpu_over_gpu_delta
gpu_global_to_delta_speedup
gpu_frontier_to_delta_speedup
gpu_delta_iterations
gpu_delta_converged
gpu_delta_active_node_visits
gpu_delta_edge_work_estimate
gpu_delta_max_frontier_size
```

These are produced by the `gpu-delta-stepping` variant.


## Constraint-network metadata

`constraint_network` rows include:

- `valid_count`
- `reference_valid_count`
- `invalid_count`
- `total_violations`
- `reference_total_violations`
- `validity_mismatches`
- `mask_mismatches`
- `violation_count_mismatches`
- `checksum`
- `reference_checksum`
- `max_abs_error`
- `max_rel_error`


## `constraint_network` sweep labels

The constraint-network runner passes `--set sweep_label=...` so the `preset` field in JSONL/reporting can show labels such as `cn_64k`, `cn_512k`, and `cn_4m`. The underlying benchmark size is still recorded explicitly in `input_size.tasks`, `input_size.resources`, and `input_size.candidates`.

## `combination_finder` metadata

The `combination_finder` benchmark writes aggregate metadata rather than returning every valid group:

```text
valid_count
reference_valid_count
invalid_count
budget_violations
risk_violations
coverage_violations
spread_violations
checksum
reference_checksum
best_score
reference_best_score
best_rank
```

This is intentional: the benchmark demonstrates candidate-space throughput while avoiding output explosion.

## Assignment preprocessing metadata

`assignment_preprocessing` rows include dense-pair and reduced-output metadata:

```text
feasible_pair_count
infeasible_pair_count
selected_candidate_count
tasks_with_any_candidate
candidate_reduction_ratio
skill_violations
capacity_violations
time_violations
distance_violations
zone_violations
risk_violations
selected_cost_checksum
best_cost_sum
mean_selected_cost
selected_resource_mismatches
max_abs_error
max_rel_error
```

The key reduction metric is:

```text
candidate_reduction_ratio = selected_candidate_count / (task_count * resource_count)
```

A small ratio means the preprocessing stage successfully converts a dense task/resource matrix into a much smaller candidate graph for downstream assignment logic.


## `local_search_moves` metadata

Important metadata fields include:

- `valid_moves` / `invalid_moves`
- `improving_moves`
- `best_move_index` and `best_delta`
- violation counts: `skill_violations`, `capacity_violations`, `distance_violations`, `risk_violations`, `no_change_violations`
- validation fields: `max_abs_error`, `max_rel_error`, mismatch counts


## Scenario simulation metadata

`scenario_simulation` rows include:

- `tasks`, `resources`, `scenarios` in `input_size`
- `feasible_scenarios`
- `infeasible_scenarios`
- `failure_scenarios`
- `capacity_scenarios`
- `risk_scenarios`
- `delay_scenarios`
- `mean_score`, `best_score`, `worst_score`
- `robustness_score`

The robustness score is synthetic and is meant for teaching relative behavior across generated scenarios.


## Scenario simulation correctness semantics

For `scenario_simulation`, `correct=yes` means CPU/GPU agreement, not plan quality. Rows include metadata such as `correctness_semantics=cpu_gpu_agreement_not_plan_quality`, `feasibility_status`, and `feasible_ratio` to separate implementation correctness from robust-plan quality.
