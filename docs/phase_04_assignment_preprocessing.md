# Phase 4.3 - Assignment preprocessing

This phase demonstrates a GPU-assisted preprocessing step for assignment-style optimization problems.

The benchmark is **not** a full assignment solver. Instead, it performs the expensive dense preprocessing step that often happens before an assignment solver or heuristic:

1. generate many tasks and resources;
2. evaluate every task/resource pair for feasibility and cost;
3. keep only the top-K feasible resources per task;
4. hand the reduced candidate set to a downstream CPU solver, heuristic, or operations-research model.

This is a practical GPU use case because each task/resource pair can be scored independently, while the output can be kept small by storing only top-K candidates per task.

## Benchmark name

```text
assignment_preprocessing
```

## Variants

```text
cpu
  nested task/resource scan with per-task top-K insertion

gpu
  one CUDA thread per task; each thread scans resources and keeps local top-K
```

The GPU implementation intentionally avoids returning the full dense matrix. It returns only the top-K resources per task. This models a common production pattern: do a large parallel feasibility/cost pass, but keep the output compact.

## Problem definition

For each task `t` and resource `r`, compute:

```text
feasible(t, r)
cost(t, r)
```

A pair can be rejected by:

```text
skill mismatch
insufficient capacity
time-window conflict
distance limit
forbidden zone
risk tolerance
```

For each task, the benchmark keeps:

```text
top_k lowest-cost feasible resources
```

## Why this is different from the full assignment problem

The full assignment problem chooses a globally consistent assignment, for example one resource per task and possibly one task per resource. This phase does not solve that global optimization problem. It reduces the candidate set first.

For example, if there are:

```text
10,000 tasks
2,000 resources
```

then the dense candidate space is:

```text
20,000,000 task/resource pairs
```

If `top_k = 8`, preprocessing reduces that to at most:

```text
80,000 task/resource candidates
```

That smaller candidate set is much friendlier for a later CPU solver or heuristic.

## Expected GPU behavior

This benchmark should be more GPU-friendly than BFS or weighted shortest path because there is no graph traversal dependency and no convergence loop.

The GPU should benefit as the number of task/resource pairs grows, but there are still pitfalls:

- if the problem is small, CPU wins because GPU launch and copy overhead dominate;
- one-thread-per-task is simple but not the most optimized top-K approach;
- if resource count becomes huge, a block-per-task or tiled resource scan may be better;
- if top-K is large, per-thread local insertion becomes more expensive.

## Canonical runner

```bat
execute_assignment_preprocessing_all_sweeps_and_analyze.bat
```

The runner performs a scale sweep, writes JSONL, analyzes the result, plots CPU/GPU scaling, and exports one small inspectable problem instance.

## Useful output files

```text
results/assignment_preprocessing_scale_sweep.jsonl
results/assignment_preprocessing_analysis/assignment_preprocessing_analysis_report.md
results/assignment_preprocessing_analysis/assignment_preprocessing_analysis_summary.csv
results/assignment_preprocessing_plots/assignment_preprocessing_times.png
results/assignment_preprocessing_plots/assignment_preprocessing_speedup.png
results/assignment_preprocessing_plots/assignment_preprocessing_candidate_reduction.png
results/assignment_preprocessing_problem_demo/plots/assignment_feasibility_matrix.png
results/assignment_preprocessing_problem_demo/plots/assignment_cost_matrix.png
results/assignment_preprocessing_problem_demo/plots/assignment_topk_resource_usage.png
```

## Interpretation

A good result for this phase is not only GPU speedup. The candidate-reduction ratio matters too. If the dense pair matrix has millions of pairs but the output is only `tasks × top_k`, the preprocessing stage is doing useful algorithmic reduction even before a solver runs.

## Plotting-only workflow

For iteration on figures and reports, use the dedicated plotting batch:

```bat
execute_assignment_preprocessing_plots.bat
```

It expects the benchmark JSONL to already exist at:

```text
results\assignment_preprocessing_scale_sweep.jsonl
```

It then reruns the analysis step, regenerates the scaling plots, and optionally exports/plots a small inspectable assignment problem instance.
This is useful when changing Python plotting logic or documentation without rerunning the full CPU/GPU benchmark sweep.
