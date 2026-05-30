# Scenario simulation benchmark

`scenario_simulation` evaluates one fixed plan under many independent uncertainty cases.

The synthetic problem is:

```text
tasks + resources + current assignment + scenarios
```

Each scenario can change:

- resource availability/failure,
- task demand,
- delay multiplier,
- cost multiplier,
- risk multiplier,
- weather penalty.

The benchmark computes one evaluation per scenario:

```text
scenario -> feasible flag, violation counts, score
```

## CPU/GPU shape

CPU:

```text
for scenario in scenarios:
    evaluate all assigned tasks under this scenario
```

GPU:

```text
one CUDA thread = one scenario
```

This is an intentionally GPU-friendly robust-planning shape. Unlike graph traversal, there is no frontier, priority queue, or convergence loop. The scenario evaluations are independent.

## Reported metrics

The benchmark records:

- `feasible_scenarios`
- `infeasible_scenarios`
- `failure_scenarios`
- `capacity_scenarios`
- `risk_scenarios`
- `delay_scenarios`
- `mean_score`
- `best_score`
- `worst_score`
- `robustness_score`

`robustness_score` is a synthetic teaching metric:

```text
mean_score + 0.25 * worst_score + 10000 * infeasible_ratio
```

Lower is better.

## Commands

Run the full sweep:

```bat
execute_scenario_simulation_all_sweeps_and_analyze.bat
```

Regenerate plots only:

```bat
execute_scenario_simulation_plots.bat
```

Direct benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark scenario_simulation --preset small --repeat 5 --warmup 1
```


## Sweep size policy

The canonical runner is intentionally demo-friendly. It stops at `sc_4m` by default.
The previous `sc_16m` stress point was removed because it took too long and did not add enough extra explanatory value for the lecture/demo flow.

Set `INCLUDE_HEAVY_CASES=1` in `execute_scenario_simulation_all_sweeps_and_analyze.bat` only when you want to add the optional `sc_8m` stress point.


## Feasibility calibration and correctness semantics

See `docs/phase_04_scenario_simulation_feasibility_calibration.md`. The `correct` field means CPU/GPU agreement, not plan quality. Robustness quality is read from `feasible_ratio`, violation counts, score summaries, and robustness score.
