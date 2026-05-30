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
