# Assignment preprocessing benchmark

This document describes the `assignment_preprocessing` benchmark implementation.

## Data model

The synthetic problem contains:

- tasks with location, required skill mask, demand, time window, zone, priority, and risk;
- resources with location, skill mask, capacity, availability window, forbidden zones, max travel distance, risk tolerance, and load.

For each task/resource pair, the benchmark evaluates feasibility and cost.

## Pair feasibility

A pair is infeasible when any of these checks fails:

```text
resource lacks required task skill
resource capacity < task demand
time windows do not overlap well enough
distance exceeds resource max distance
task zone is forbidden for resource
task risk exceeds resource risk tolerance
```

## Cost model

For feasible pairs, lower cost is better. The synthetic cost combines:

```text
distance
resource load
task risk
task priority
task duration
```

The cost model is synthetic, but it is intentionally similar to real assignment preprocessing: score all plausible pairings, then pass only the best candidate edges onward.

## CPU implementation

The CPU implementation scans all resources for each task and maintains a small top-K list using insertion replacement.

```text
for task in tasks:
    local_top_k = empty
    for resource in resources:
        eval = evaluate(task, resource)
        if feasible:
            insert into local_top_k
```

## GPU implementation

The GPU implementation maps one CUDA thread to one task. Each thread scans all resources and keeps a local top-K list.

```text
thread task_i:
    local_top_k = empty
    for every resource:
        eval = evaluate(task_i, resource)
        if feasible:
            insert into local_top_k
    write top_k entries for task_i
```

This is simple and robust. A future optimized version could use one block per task and parallelize the resource scan inside the block.

## Why top-K matters

Returning the full dense cost matrix can be expensive. Returning top-K candidates per task keeps output bounded:

```text
full output:    task_count × resource_count
reduced output: task_count × top_k
```

That is the central lesson of this phase: GPU preprocessing is often useful when it turns a huge dense candidate space into a smaller solver-ready sparse candidate set.

## Plotting-only batch

The full runner executes the benchmark sweep, analysis, scaling plots, and a small problem-definition export.
When results already exist and you only want to regenerate the plots, use:

```bat
execute_assignment_preprocessing_plots.bat
```

This plotting-only batch does not rerun the expensive benchmark sweep. It uses:

```text
results\assignment_preprocessing_scale_sweep.jsonl
```

and regenerates:

```text
results\assignment_preprocessing_analysis\
results\assignment_preprocessing_plots\
results\assignment_preprocessing_problem_demo\
```

Important variables at the top of the batch file:

```bat
set "RUN_ANALYSIS=1"
set "RUN_PROBLEM_EXPORT=1"
set "PLOT_SHOW=0"
set "AP_PROBLEM_TASKS=32"
set "AP_PROBLEM_RESOURCES=24"
set "AP_PROBLEM_TOP_K=4"
```

Use `RUN_PROBLEM_EXPORT=0` when you want to replot an already-exported problem instance without regenerating it.
