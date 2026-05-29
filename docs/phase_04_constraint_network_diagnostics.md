# Phase 4.1.3 - Constraint-network diagnostics plots

The constraint-network benchmark is intentionally GPU-friendly: every candidate assignment can be checked independently.
The first scaling plots answer **when** the GPU becomes faster. This diagnostic layer answers **what the data means**.

## New metadata

Each `constraint_network` result row now includes per-reason violation counts:

- `skill_violations`
- `capacity_violations`
- `task_window_violations`
- `resource_window_violations`
- `distance_violations`
- `forbidden_zone_violations`
- `risk_violations`

A single candidate can violate more than one constraint, so the sum of these counts can be larger than `invalid_count`.

## New script

```bat
python scripts\plot_constraint_network_diagnostics.py ^
  --jsonl results\constraint_network_scale_sweep.jsonl ^
  --output-dir results\constraint_network_diagnostics ^
  --show
```

The canonical runner executes this automatically:

```bat
execute_constraint_network_all_sweeps_and_analyze.bat
```

## Plots

The diagnostic script writes:

- `constraint_network_valid_invalid_ratio.png` - how much of the candidate pool is feasible.
- `constraint_network_violation_reasons.png` - which constraints reject candidates.
- `constraint_network_gpu_time_breakdown.png` - host-to-device copy, kernel time, and device-to-host copy.
- `constraint_network_penalty_error.png` - floating-point validation behavior for the penalty field.

## Interpretation

This phase is a positive contrast to the graph traversal chapters. The constraint-network workload has no frontier depth, no convergence loop, and no global priority queue. As candidate count grows, the GPU has a clean opportunity to amortize launch and transfer overhead.
