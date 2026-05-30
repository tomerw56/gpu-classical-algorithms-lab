# Literature and problem definitions

This appendix collects the source-backed definitions and references used by the benchmark lab. It is meant to answer: **what problem are we demonstrating, where does the formal problem come from, and why is the CPU/GPU comparison interesting?**

Links were checked while preparing this documentation patch. Prefer the named sources here over ad-hoc web searches when preparing the lecture.

## How to read this appendix

Each section contains:

- **Problem definition** - the classical problem or computational pattern.
- **Repository benchmark** - where it appears in this project.
- **GPU lesson** - why the workload helps or hurts the GPU.
- **References** - source-backed reading links.

## CUDA / GPU programming model

### Problem definition

CUDA is NVIDIA's programming platform for running parallel kernels on GPUs. The benchmark lab uses CUDA C++ kernels for all GPU variants and records both kernel time and end-to-end GPU time.

### Repository use

Used throughout:

- `foundation_smoke`
- `polynomial_batch`
- `cost_matrix`
- `spatial_events`
- `graph_bfs`
- `graph_connected_components`
- `graph_weighted_relaxation`
- `constraint_network`
- `combination_finder`

### GPU lesson

A GPU is strongest when the workload exposes many independent or regular operations and weakest when synchronization, divergence, atomics, or CPU/GPU transfers dominate.

### References

- NVIDIA CUDA C++ Programming Guide: https://docs.nvidia.com/cuda/cuda-programming-guide/
- CUDA C++ Programming Guide PDF: https://docs.nvidia.com/cuda/pdf/CUDA_C_Programming_Guide.pdf

## Polynomial batch evaluation

### Problem definition

Polynomial evaluation computes `p(x)` for a polynomial and one or more values of `x`. The benchmark uses Horner's method to evaluate a degree-15 polynomial over many independent `x` values.

### Repository benchmark

- `polynomial_batch`
- Docs: `docs/polynomial_batch.md`, `docs/phase_02_polynomial_batch.md`

### GPU lesson

This is an embarrassingly parallel workload: each `x` value is independent. It is a clean positive GPU example once there are enough values to amortize launch and transfer overhead.

### References

- Horner's method: https://en.wikipedia.org/wiki/Horner%27s_method
- Polynomial evaluation: https://en.wikipedia.org/wiki/Polynomial_evaluation

## Cost matrix generation and assignment-style scoring

### Problem definition

Many assignment and dispatch systems start by generating a cost or feasibility matrix `cost[task][resource]`. The global assignment problem asks how to assign agents/resources to tasks with minimal cost under constraints.

### Repository benchmark

- `cost_matrix`
- Docs: `docs/cost_matrix.md`, `docs/phase_02_cost_matrix.md`

### GPU lesson

Cost-matrix generation is a strong GPU candidate because every task-resource cell can be scored independently. Branches still matter: skill checks, feasibility rules, and penalty tiers can introduce warp divergence.

### References

- Assignment problem: https://en.wikipedia.org/wiki/Assignment_problem
- Hungarian algorithm: https://en.wikipedia.org/wiki/Hungarian_algorithm

## Spatial event detection

### Problem definition

Spatial event detection tests moving objects against regions: entering a zone, leaving it, staying inside, or crossing through it. The benchmark uses track segments and circular zones.

### Repository benchmark

- `spatial_events`
- Docs: `docs/spatial_events.md`, `docs/phase_02_spatial_events.md`

### GPU lesson

Track-zone pairs are independent, so the pairwise matrix maps naturally to GPU threads. The output can still be large, so the useful output should usually be compacted, counted, or summarized.

### References

- Computational geometry overview: https://en.wikipedia.org/wiki/Computational_geometry
- Point in polygon: https://en.wikipedia.org/wiki/Point_in_polygon
- Collision detection overview: https://en.wikipedia.org/wiki/Collision_detection

## CSR graph representation

### Problem definition

Compressed sparse row (CSR) stores sparse adjacency data using row offsets and column indices. It is a common layout for sparse matrices and graph adjacency lists.

### Repository benchmark

- `graph_foundation`
- Used by BFS, connected components, and weighted relaxation.
- Docs: `docs/graph_foundation.md`, `docs/phase_03_graph_foundation.md`

### GPU lesson

CSR is compact and GPU-friendly for edge scans, but irregular degree distribution can cause load imbalance.

### References

- Sparse matrix / CSR background: https://en.wikipedia.org/wiki/Sparse_matrix
- Gunrock graph algorithms list, including BFS and connected components: https://gunrock.github.io/gunrock/gunrock.wiki/Graph-Algorithms.html

## Breadth-first search / reachability

### Problem definition

Breadth-first search explores graph vertices level by level. It is used for reachability and unweighted shortest-path distance.

### Repository benchmark

- `graph_bfs`
- Docs: `docs/graph_bfs.md`, `docs/phase_03_graph_bfs_analysis.md`, `docs/phase_03_graph_bfs_shape_scaling.md`

### GPU lesson

BFS is a counterexample to "graph = GPU win." Chain and grid graphs have many synchronization levels and/or narrow frontiers. Large random graphs can be better because they expose wide frontiers and low depth.

### References

- Breadth-first search: https://en.wikipedia.org/wiki/Breadth-first_search
- Merrill, Garland, Grimshaw, *Scalable GPU Graph Traversal*: https://research.nvidia.com/sites/default/files/pubs/2012-02_Scalable-GPU-Graph/ppo213s-merrill.pdf
- Michael Garland's page for the same BFS work: https://mgarland.org/papers/2012/bfs/

## Connected components

### Problem definition

A connected component is a maximal connected subgraph. For undirected graphs, connected-components algorithms partition vertices into disjoint groups.

### Repository benchmark

- `graph_connected_components`
- Docs: `docs/graph_connected_components.md`, `docs/phase_03_graph_connected_components_scaling.md`, `docs/phase_03_graph_connected_components_non_naive.md`

### GPU lesson

The CPU Union-Find baseline is strong. Naive GPU label propagation can lose because it needs repeated global iterations. The non-naive root-hooking / pointer-jumping variant improves the GPU story on mixed and chain-like cases.

### References

- Connected components: https://en.wikipedia.org/wiki/Component_%28graph_theory%29
- Disjoint-set / Union-Find data structure: https://en.wikipedia.org/wiki/Disjoint-set_data_structure
- Gunrock paper, evaluating BFS, SSSP, CC, and PageRank: https://arxiv.org/pdf/1501.05387

## Weighted shortest paths / SSSP

### Problem definition

The shortest path problem asks for a path minimizing total edge weight. Dijkstra's algorithm solves single-source shortest paths for nonnegative weights.

### Repository benchmark

- `graph_weighted_relaxation`
- Docs: `docs/graph_weighted_relaxation.md`, `docs/phase_03_graph_weighted_relaxation_final_conclusions.md`

### GPU lesson

Weighted shortest path is the strongest negative/nuanced graph example in this repository. CPU Dijkstra wins for chain/grid/layered and small random graphs. GPU global edge relaxation wins only on sufficiently large random graphs. Frontier and delta-style GPU variants were educational but not the recommended winners in this project.

### References

- Shortest path problem: https://en.wikipedia.org/wiki/Shortest_path_problem
- Dijkstra's algorithm: https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm
- Meyer and Sanders, *Delta-stepping: a parallelizable shortest path algorithm*: https://www.sciencedirect.com/science/article/pii/S0196677403000762
- Delta-stepping paper copy: https://ae.iti.kit.edu/sanders/papers/wmain.pdf

## Constraint satisfaction and candidate validation

### Problem definition

A constraint satisfaction problem has variables, domains, and constraints. A candidate assignment is valid when it satisfies the relevant constraints. The repository uses this idea in a practical dispatch form: candidate task-resource assignments are checked against skills, capacity, time windows, distance, forbidden zones, and risk.

### Repository benchmark

- `constraint_network`
- Docs: `docs/constraint_network.md`, `docs/phase_04_constraint_network.md`, `docs/phase_04_constraint_network_problem_visualization.md`

### GPU lesson

Candidate validation is highly GPU-friendly when candidates are independent. This benchmark is a practical positive example after the graph counterexamples.

### References

- Constraint satisfaction problem: https://en.wikipedia.org/wiki/Constraint_satisfaction_problem
- Constraint satisfaction: https://en.wikipedia.org/wiki/Constraint_satisfaction

## Combination finding / candidate enumeration

### Problem definition

A `k`-combination is an unordered selection of `k` elements from a set of `n` elements. The number of possible combinations grows as a binomial coefficient, which quickly creates combinatorial explosion.

### Repository benchmark

- `combination_finder`
- Docs: `docs/phase_04_combination_finder.md`, `docs/phase_04_combination_problem_definitions.md`

### GPU lesson

The GPU can evaluate many combinations independently, but the output must be controlled: count, reduce, or keep summaries rather than copying every valid combination back.

### References

- Combination: https://en.wikipedia.org/wiki/Combination
- Binomial coefficient: https://en.wikipedia.org/wiki/Binomial_coefficient
- Combinatorial explosion: https://en.wikipedia.org/wiki/Combinatorial_explosion
- Brute-force / exhaustive search: https://en.wikipedia.org/wiki/Brute-force_search
- Backtracking: https://en.wikipedia.org/wiki/Backtracking

## Planned / roadmap topics

These topics were part of the original roadmap and are useful for the lecture even if not all are fully implemented yet.

### Assignment preprocessing

Problem: build a large cost matrix, filter top-K feasible candidates per task, then solve or approximate the final assignment.

References:

- Assignment problem: https://en.wikipedia.org/wiki/Assignment_problem
- Hungarian algorithm: https://en.wikipedia.org/wiki/Hungarian_algorithm

### Local-search optimization

Problem: repeatedly evaluate neighboring solutions such as swaps, insertions, or 2-opt moves. GPU can score many candidate moves while CPU controls the sequential search loop.

References:

- Local search: https://en.wikipedia.org/wiki/Local_search_%28optimization%29
- Local search heuristics for multidimensional assignment: https://arxiv.org/abs/0806.3258

### Scenario simulation / robust planning

Problem: evaluate one plan across many possible scenarios. This is often naturally parallel because each scenario can be simulated independently.

References:

- Monte Carlo method: https://en.wikipedia.org/wiki/Monte_Carlo_method

## Summary: what the literature supports in our project

The references support the same conclusion as the benchmark results:

1. Dense independent evaluation is a natural GPU fit.
2. Graph traversal is hard because memory access and work distribution are irregular.
3. Strong CPU baselines such as Union-Find and Dijkstra can beat simple GPU adaptations.
4. GPU graph wins usually require algorithms designed for the hardware, not merely ported loops.
5. Candidate scoring, validation, enumeration, and simulation are often better practical GPU entry points for classical algorithm teams.
