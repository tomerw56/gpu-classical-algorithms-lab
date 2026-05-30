# Phase 4: Combinatorics, constraints, and optimization candidate scoring

Phase 4 moves from graph algorithms into optimization-support workloads.

The important idea is that the GPU does not need to own the entire optimizer. It can accelerate expensive repeated substeps:

- candidate validation
- constraint checking
- candidate scoring
- neighborhood move evaluation
- scenario evaluation

## Phase 4.1

`constraint_network` is the first example. It evaluates many candidate task/resource assignments against several constraints.

This is a GPU-friendly pattern because every candidate can be checked independently.

## Phase 4.2: combination finder

`combination_finder` extends the candidate-evaluation story from single task/resource assignments to groups of items. It evaluates every `k`-combination of a generated item set and keeps compact aggregate outputs.

This phase is useful for optimization lectures because it shows both sides of brute-force enumeration:

- GPU can evaluate many independent groups quickly.
- The number of groups grows combinatorially.
- Returning all valid groups can be worse than the computation itself, so the benchmark returns aggregates.

See:

- `docs/phase_04_combination_finder.md`
- `docs/phase_04_combination_problem_definitions.md`


## Phase 4.4 - Local-search move evaluation

`local_search_moves` evaluates many candidate neighborhood moves from a current assignment. The GPU maps one thread to one candidate move, making this a clean optimization-support workload when the neighborhood is large enough.


## Phase 4.5 - scenario simulation

`scenario_simulation` evaluates one candidate plan under many independent uncertainty scenarios. It extends the optimization-support story from candidate validation and move scoring into robust planning / what-if analysis.


## Scenario-simulation default sweep

The scenario-simulation runner is intentionally kept demo-friendly. It stops at `sc_4m` by default; the old `sc_16m` stress point was removed. Optional heavy mode adds only `sc_8m`.
