# Constraint Network Benchmark

`constraint_network` evaluates many candidate task/resource assignments against a small set of deterministic constraints.

The benchmark represents a realistic optimization substep: before a solver chooses an assignment, a system often needs to reject infeasible candidates and score partially bad candidates.

## Problem shape

Each candidate contains:

```text
task_index
resource_index
start_time
duration
```

For each candidate, the evaluator checks:

- skill compatibility
- capacity
- task time window
- resource availability window
- distance from resource home to task location
- forbidden zone
- risk tolerance

The output for every candidate is:

```text
valid flag
violation mask
violation count
penalty score
```

## CPU baseline

The CPU implementation loops over candidates:

```text
for candidate in candidates:
    task = tasks[candidate.task_index]
    resource = resources[candidate.resource_index]
    evaluate constraints
```

## GPU implementation

The CUDA version maps naturally:

```text
one CUDA thread = one candidate assignment
```

This is intentionally different from graph traversal: there is no frontier, no global queue, and no dependency between candidates.

## Why GPU should help

The workload is embarrassingly parallel:

- every candidate is independent
- all candidates run the same constraint code
- output is compact
- the same task/resource arrays are reused across many candidates

Small inputs may still favor CPU due to launch and transfer overhead, but larger candidate batches should become GPU-friendly.

## Important caveat

The constraint logic is branch-heavy. Different candidates can violate different subsets of constraints, causing warp divergence. This benchmark is therefore more realistic than a pure arithmetic vector kernel.

## Run

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark constraint_network --preset small --repeat 5 --warmup 1
```

Full sweep/analyze/plot:

```bat
execute_constraint_network_all_sweeps_and_analyze.bat
```


## Penalty validation tolerance

The categorical results are still checked exactly:

```text
valid flag
violation mask
violation count
```

The floating-point `penalty` field is checked with a combined absolute/relative tolerance. This is intentional: the CPU and CUDA paths both use single-precision arithmetic, but the host compiler and CUDA compiler may make slightly different choices around multiply-add fusion and instruction ordering. Larger candidate sets are more likely to encounter cases where the absolute difference exceeds a tiny fixed threshold even though the categorical constraint decision is identical.

The benchmark records:

```text
max_abs_error
max_rel_error
penalty_abs_tolerance
penalty_rel_tolerance
```

A row is considered correct when categorical fields match exactly and the penalty difference is within the combined tolerance.


## Fine-grained scaling sweep

Use:

```bat
execute_constraint_network_all_sweeps_and_analyze.bat
```

The runner now evaluates more than the four built-in presets. It uses custom candidate counts from `cn_4k` through `cn_4m`, with optional `cn_8m` and `cn_16m` stress points controlled by `INCLUDE_HEAVY_CASES`. This gives the timing and speedup plots enough resolution to show where the GPU overtakes the CPU.


## Constraint-network diagnostics

Phase 4.1 includes a richer diagnostic plotting script:

```bat
python scripts\plot_constraint_network_diagnostics.py ^
  --jsonl results\constraint_network_scale_sweep.jsonl ^
  --output-dir results\constraint_network_diagnostics ^
  --show
```

The canonical runner also invokes this script automatically. It visualizes valid/invalid ratios, violation reasons, GPU transfer/kernel/output-copy breakdown, and CPU/GPU penalty validation error.

## Problem-definition visualization

The benchmark now includes an exporter/plotter that shows the generated constraint problem itself, not only performance results.

```bat
build-cuda-ninja\export_constraint_network.exe --tasks 32 --resources 16 --candidates 512 --output-dir results\constraint_network_problem_demo
python scripts\plot_constraint_network_problem.py --input-dir results\constraint_network_problem_demo --show
```

This produces matrices such as:

- `constraint_skill_compatibility_matrix.png` - which resources have the skills for each task.
- `constraint_pair_feasibility_matrix.png` - which task/resource pairs pass all pair-level constraints.
- `constraint_candidate_assignments.png` - the actual generated candidates, valid vs invalid.
- `constraint_time_windows.png` - task windows and resource availability windows.

See `docs/phase_04_constraint_network_problem_visualization.md` for details.
