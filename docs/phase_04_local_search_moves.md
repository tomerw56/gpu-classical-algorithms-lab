# Phase 4.4: local-search move evaluation

This phase adds `local_search_moves`, a benchmark for evaluating a large neighborhood of candidate replacement moves from a current assignment.

## Run

```bat
configure_ninja_cuda128.bat
execute_local_search_moves_all_sweeps_and_analyze.bat
```

The runner writes:

```text
results\local_search_moves_scale_sweep.jsonl
results\local_search_moves_analysis\
results\local_search_moves_plots\
results\local_search_moves_problem_demo\
```

## Plots

- `local_search_moves_times.png`
- `local_search_moves_speedup.png`
- `local_search_moves_valid_improving_ratio.png`
- `local_search_moves_violation_reasons.png`

The problem exporter additionally plots the current assignment geometry and the distribution of move deltas.

## Why this phase matters

This is a practical GPU-assisted optimization pattern:

```text
CPU keeps the current solution and applies moves.
GPU scores a large neighborhood of candidate moves.
CPU or solver consumes the best/relevant moves.
```


## Plotting-only workflow

After you already have `results/local_search_moves_scale_sweep.jsonl`, you can regenerate analysis, scaling plots, and the inspectable problem plots without rerunning the benchmark sweep:

```bat
execute_local_search_moves_plots.bat
```


## Speedup plateau explanation

If the speedup curve rises and then becomes stable, see `docs/phase_04_local_search_moves_speedup_plateau.md`. The short explanation is that the GPU reaches steady throughput: each move has a fixed amount of work, and both CPU and GPU scale roughly linearly once fixed GPU overhead has been amortized.
