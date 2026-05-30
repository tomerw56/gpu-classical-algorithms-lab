# Local-search move evaluation

`local_search_moves` models a common inner loop of local-search and metaheuristic optimization: given a current plan, evaluate many candidate neighborhood moves and keep the improving moves.

This benchmark does **not** run a full iterative optimizer. It measures the expensive evaluation stage:

```text
current assignment + candidate replacement moves -> score deltas
```

Each move replaces the resource assigned to one task with a proposed alternative resource. The evaluator checks skill, capacity, distance, and risk constraints, then computes the move delta:

```text
delta = new_cost - old_cost
```

A negative delta means the candidate move improves the current solution.

## CPU/GPU mapping

- CPU: loop over candidate moves.
- GPU: one CUDA thread evaluates one candidate move.

This is expected to be a good GPU-shaped workload when the neighborhood is large, because moves are independent and there is no graph traversal dependency or convergence loop.

## Educational point

Local search is often not about solving the whole problem in one kernel. Instead, the GPU can accelerate the neighborhood-scoring stage so a CPU or solver can choose and apply the best move.


## Plotting-only workflow

After you already have `results/local_search_moves_scale_sweep.jsonl`, you can regenerate analysis, scaling plots, and the inspectable problem plots without rerunning the benchmark sweep:

```bat
execute_local_search_moves_plots.bat
```


## Speedup plateau explanation

If the speedup curve rises and then becomes stable, see `docs/phase_04_local_search_moves_speedup_plateau.md`. The short explanation is that the GPU reaches steady throughput: each move has a fixed amount of work, and both CPU and GPU scale roughly linearly once fixed GPU overhead has been amortized.
