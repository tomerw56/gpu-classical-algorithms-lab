# Phase 4.5 - scenario simulation / robust planning

This phase demonstrates GPU-assisted **robust plan evaluation**.

Given one candidate plan, we evaluate it under many possible scenarios:

```text
normal load
resource failure
task demand spike
delay multiplier
cost/risk multiplier
weather penalty
```

The goal is not to optimize the plan inside the GPU. The goal is to quickly answer:

```text
How does this plan behave across many possible worlds?
```

## Why this belongs in the project

This phase is a strong contrast to the graph chapters.

Graph traversal can be difficult on GPUs because of frontier shape, synchronization, and irregular memory access. Scenario simulation is much cleaner:

```text
scenario 0 independent of scenario 1
scenario 1 independent of scenario 2
...
```

That makes it a realistic GPU candidate for classical planning and optimization teams.

## Runner

```bat
execute_scenario_simulation_all_sweeps_and_analyze.bat
```

The runner executes a scenario-count sweep, analyzes the JSONL, generates plots, and exports one small inspectable problem instance.

Important knobs:

```bat
set "SC_REPEAT=5"
set "SC_WARMUP=1"
set "INCLUDE_HEAVY_CASES=0"
set "SC_PROBLEM_TASKS=32"
set "SC_PROBLEM_RESOURCES=16"
set "SC_PROBLEM_SCENARIOS=2048"
```

## Plot-only workflow

```bat
execute_scenario_simulation_plots.bat
```

Use this after a sweep already exists and you only want to regenerate analysis and figures.

## Expected lesson

Small scenario counts may still favor the CPU because the GPU pays fixed overhead.
As scenario count grows, the GPU should reach a throughput plateau and become a strong accelerator.

This phase is a useful example of GPU as a **robustness evaluator** rather than a full optimization solver.


## Sweep size policy

The default sweep stops at `sc_4m`. The original `sc_16m` point was removed because it took too long to run and did not materially improve the conclusion. The optional heavy mode now only adds `sc_8m`.

Use the default for normal demos and documentation runs:

```bat
set "INCLUDE_HEAVY_CASES=0"
```

Use heavy mode only for a deeper local run:

```bat
set "INCLUDE_HEAVY_CASES=1"
```


## Feasibility calibration and correctness semantics

See `docs/phase_04_scenario_simulation_feasibility_calibration.md`. The `correct` field means CPU/GPU agreement, not plan quality. Robustness quality is read from `feasible_ratio`, violation counts, score summaries, and robustness score.


For the final feasibility calibration pass, see `docs/phase_04_scenario_simulation_feasibility_calibration_v2.md`.
