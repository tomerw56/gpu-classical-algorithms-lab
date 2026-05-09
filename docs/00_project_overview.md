# Project Overview

This codebase is a teaching and benchmarking lab for classical algorithms on CPU and GPU.

The project intentionally avoids machine-learning examples. The focus is on workloads such as:

- Polynomial batch calculations.
- Complex cost matrix generation.
- Assignment preprocessing.
- Graph BFS and reachability.
- Connected components.
- Weighted graph relaxation.
- Constraint-network validation.
- Combination search.
- Spatial event detection.
- Local-search optimization candidate scoring.
- Scenario simulation.

The important question is not:

> Is the GPU faster?

The better question is:

> For which input sizes and algorithm shapes does GPU acceleration pay off end-to-end?

## Design rule

Every benchmark should eventually have:

- A CPU baseline.
- A GPU implementation.
- A deterministic input generator.
- A correctness comparison.
- Multiple input sizes.
- Repeated timing.
- JSONL output.
- A short Markdown explanation of strengths and pitfalls.

## Phase 1 scope

Phase 1 only builds the shared infrastructure. The actual algorithm demos are added in later phases.
