# Phase 4.5.2 - Scenario feasibility calibration

The first scenario-simulation sweep was excellent for performance, but it exposed a modeling problem: every generated scenario was infeasible. That made the GPU speedup story clear, but the robust-planning story weak.

This patch calibrates the generated base plan so the sweep contains a mix of feasible and infeasible outcomes. The benchmark now chooses a plausible task-to-resource assignment rather than a purely modular assignment, gives tasks more realistic risk/latest-finish values, and keeps scenario uncertainty strong enough to create failures.

## Correctness is not plan quality

The result-table `correct` column means:

```text
CPU and GPU agree on scenario feasibility, violation masks, counts, and scores.
```

It does **not** mean:

```text
the plan is feasible, robust, or operationally good.
```

Plan quality is represented by:

- `feasible_ratio`
- feasible/infeasible scenario counts
- violation breakdown
- mean/best/worst score
- robustness score

The analyzer now warns if all rows are degenerate, meaning every scenario is feasible or every scenario is infeasible.
