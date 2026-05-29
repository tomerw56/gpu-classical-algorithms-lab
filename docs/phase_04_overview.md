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
