# Lecture packaging

This document turns the benchmark repository into a lecture/demo package.
It is intended for an algorithmic/software audience that wants to decide when GPU support is worth requesting in hardware.

## Recommended lecture title

**GPU for classical algorithms: where it helps, where it does not, and how to decide**

## Core message

The project does not try to prove that GPU is always faster. The point is more useful:

> GPU helps classical algorithms when the workload exposes many independent work items, enough arithmetic or memory work per synchronization point, and a data layout that fits the GPU execution model.

The best engineering answer is often **CPU**, **GPU**, or **hybrid**, depending on the algorithm and data anatomy.

## Lecture structure

Suggested 45-60 minute flow:

| Section | Time | Purpose |
|---|---:|---|
| Motivation and hardware question | 5 min | Why the department cares about GPU as a hardware option. |
| GPU mental model | 5 min | Many independent work items, memory traffic, transfer overhead, synchronization. |
| Dense data-parallel wins | 8 min | Polynomial batch, cost matrix, spatial events. |
| Graph counterexamples | 12 min | BFS, connected components, weighted shortest path. Show graph anatomy matters. |
| Optimization-support wins | 12 min | Constraints, combinations, assignment preprocessing, local search, scenario simulation. |
| Decision guide | 5 min | CPU vs GPU vs hybrid checklist. |
| Demo and Q&A | remaining | Run selected benchmark rows and show plots. |

## What to run live

Use the curated core demo when time is limited:

```bat
configure_ninja_cuda128.bat
execute_lecture_demo_core.bat
```

This writes:

```text
results\lecture_demo_core.jsonl
```

For full evidence, use the phase-specific runners from `docs/documentation_index.md`.

## Recommended live-demo story

1. **Start with a clean positive case**: `polynomial_batch`, `cost_matrix`, or `scenario_simulation`.
2. **Show a counterexample**: `graph_bfs` chain/grid or weighted shortest path chain/grid.
3. **Show graph anatomy**: random graph BFS and connected components can become GPU-friendly when the graph exposes wide active work.
4. **Show the hybrid story**: assignment preprocessing reduces a large task/resource matrix to top-K candidates for a CPU/solver stage.
5. **End with the decision guide**: use GPU where the evaluation workload is large, independent, and batchable.

## Demo caution

`execute_lecture_demo_core.bat` is intentionally short. It is not the source of record for final conclusions. The source of record is the per-phase sweep scripts, analysis reports, and plots.


## Lecture demo runner fix

If `execute_lecture_demo_core.bat` prints usage/help for every benchmark instead of running them, see `docs/lecture_demo_runner_fix.md`. The fixed runner passes benchmark arguments as a quoted command fragment and echoes each command before execution.
