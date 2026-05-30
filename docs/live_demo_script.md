# Live demo script

This script describes what to run and what to say during a live demo.

## Preparation

```bat
configure_ninja_cuda128.bat
execute_all_tests.bat
```

For the short lecture demo:

```bat
execute_lecture_demo_core.bat
```

## Demo sequence

### 1. Foundation smoke

Run or show `foundation_smoke` to verify CPU/GPU timing and device detection.

Talking point: measure total time and kernel time separately.

### 2. Dense data-parallel win

Use `polynomial_batch`, `cost_matrix`, or `spatial_events`.

Talking point: one independent work item per thread is the clean GPU case.

### 3. Graph counterexample

Use BFS chain/grid versus random, or weighted relaxation.

Talking point: graph size alone is not enough; frontier width, depth, and synchronization dominate.

### 4. Connected components nuance

Show that the non-naive GPU variant improved connected components, but not all graph anatomy behaves the same.

Talking point: better GPU algorithms matter, but workload anatomy still matters.

### 5. Assignment preprocessing

Show the feasibility/cost matrix and top-K candidate reduction plots.

Talking point: GPU reduces the task/resource search space; CPU/solver can solve the smaller problem.

### 6. Local-search moves

Show scaling and throughput plateau.

Talking point: speedup rises until GPU overhead is amortized, then stabilizes at a throughput ratio.

### 7. Scenario simulation

Show scenario score distribution and speedup.

Talking point: many independent possible worlds are a strong GPU fit.

## Closing statement

The project is not a GPU sales pitch. It is a decision framework: GPU is valuable when the algorithm and data anatomy match the hardware.


## Lecture demo runner fix

If `execute_lecture_demo_core.bat` prints usage/help for every benchmark instead of running them, see `docs/lecture_demo_runner_fix.md`. The fixed runner passes benchmark arguments as a quoted command fragment and echoes each command before execution.
