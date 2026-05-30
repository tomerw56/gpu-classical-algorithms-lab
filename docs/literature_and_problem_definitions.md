# Literature and problem definitions

This file is the source-backed glossary for the GPU Classical Algorithms Lab. It defines the featured problem families and explains why each one was included in the benchmark suite.

The references are intentionally practical and mostly introductory: they are meant to orient engineers preparing the lecture, not to be an exhaustive survey.

## CUDA / GPU programming model

### Problem definition

CUDA exposes a model where many lightweight GPU threads execute kernels over data-parallel work. The key design question in this project is whether a classical workload can be expressed as many independent or loosely coupled work items with limited synchronization and controlled output size.

### Repository relevance

All GPU variants in the project are CUDA implementations. The timing methodology separates total GPU time from kernel time where possible.

### References

- CUDA C++ Programming Guide: https://docs.nvidia.com/cuda/cuda-programming-guide/index.html

## Polynomial evaluation

### Problem definition

Evaluating a polynomial for many independent input values is a dense data-parallel workload. Horner's method is a standard way to evaluate polynomials efficiently by rewriting nested multiplications/additions.

### Repository benchmark

- `polynomial_batch`
- Docs: `docs/polynomial_batch.md`, `docs/phase_02_polynomial_batch.md`

### GPU lesson

This is a clean positive GPU case: one independent polynomial evaluation per `x` value.

### References

- Horner's method: https://en.wikipedia.org/wiki/Horner%27s_method
- Polynomial: https://en.wikipedia.org/wiki/Polynomial

## Cost matrix / assignment-style scoring

### Problem definition

Many optimization systems compute a task/resource cost matrix before solving a downstream assignment or scheduling problem. Each cell represents the score or cost of pairing one task with one resource.

### Repository benchmarks

- `cost_matrix`
- `assignment_preprocessing`
- Docs: `docs/cost_matrix.md`, `docs/assignment_preprocessing.md`, `docs/phase_04_assignment_preprocessing.md`

### GPU lesson

Dense task/resource cell scoring is a strong GPU fit. The assignment-preprocessing phase adds a practical reduction: keep only top-K feasible resources per task instead of handing the full dense matrix to a CPU solver.

### References

- Assignment problem: https://en.wikipedia.org/wiki/Assignment_problem
- Hungarian algorithm: https://en.wikipedia.org/wiki/Hungarian_algorithm
- CP-Algorithms, Hungarian algorithm: https://cp-algorithms.com/graph/hungarian-algorithm.html
- Selection algorithm / top-k concept: https://en.wikipedia.org/wiki/Selection_algorithm

## Spatial event detection

### Problem definition

Spatial event detection tests geometric relationships such as whether a track segment enters, exits, stays inside, or crosses through a zone.

### Repository benchmark

- `spatial_events`
- Docs: `docs/spatial_events.md`, `docs/phase_02_spatial_events.md`

### GPU lesson

The benchmark is a dense pairwise geometry problem: many track/zone pairs can be evaluated independently.

### References

- Computational geometry: https://en.wikipedia.org/wiki/Computational_geometry
- Line segment intersection: https://en.wikipedia.org/wiki/Line_segment_intersection

## CSR graph representation

### Problem definition

Compressed Sparse Row (CSR) stores sparse adjacency data using row offsets and column indices. It is common for sparse matrices and graph adjacency structures.

### Repository benchmark/support

- `graph_foundation`
- Docs: `docs/graph_foundation.md`, `docs/phase_03_graph_foundation.md`

### GPU lesson

CSR is compact and GPU-friendly for sequential adjacency ranges, but graph algorithms still depend heavily on frontier width, degree distribution, memory locality, and synchronization.

### References

- Sparse matrix: https://en.wikipedia.org/wiki/Sparse_matrix
- Compressed sparse row: https://en.wikipedia.org/wiki/Sparse_matrix#Compressed_sparse_row_(CSR,_CRS_or_Yale_format)

## BFS / reachability

### Problem definition

Breadth-first search explores a graph in levels from a source vertex. It is central to reachability, shortest paths in unweighted graphs, and graph traversal.

### Repository benchmark

- `graph_bfs`
- Docs: `docs/graph_bfs.md`, `docs/phase_03_graph_bfs_shape_scaling.md`, `docs/phase_03_graph_bfs_frontier_anatomy.md`

### GPU lesson

Graph size alone is insufficient. Chain/grid graphs expose little useful parallel frontier work; large random graphs expose broad frontiers and can be GPU-friendly.

### References

- Breadth-first search: https://en.wikipedia.org/wiki/Breadth-first_search
- Merrill, Garland, Grimshaw, scalable GPU graph traversal: https://research.nvidia.com/publication/2012-02_scalable-gpu-graph-traversal
- Beamer et al., direction-optimizing BFS: https://scottbeamer.net/pubs/beamer-sc2012.pdf

## Connected components / Union-Find

### Problem definition

Connected components partition a graph into groups of mutually reachable vertices. Union-Find / Disjoint Set Union is a classic CPU-friendly structure for this task.

### Repository benchmark

- `graph_connected_components`
- Docs: `docs/graph_connected_components.md`, `docs/phase_03_graph_connected_components_non_naive.md`

### GPU lesson

The naive GPU label-propagation variant is useful but not enough. The non-naive root-hooking and pointer-jumping variant is much stronger, especially on mixed/chain-like anatomy, but graph structure still matters.

### References

- Connected component: https://en.wikipedia.org/wiki/Component_(graph_theory)
- Disjoint-set data structure / Union-Find: https://en.wikipedia.org/wiki/Disjoint-set_data_structure
- ECL-CC GPU connected components paper: https://userweb.cs.txstate.edu/~mb92/papers/hpdc18.pdf

## Weighted shortest paths / Dijkstra / delta-stepping

### Problem definition

Weighted shortest path finds minimum-cost paths from a source to graph vertices. Dijkstra's algorithm is a strong CPU baseline for non-negative weights. Delta-stepping is a parallel shortest-path approach based on distance buckets and light/heavy edge treatment.

### Repository benchmark

- `graph_weighted_relaxation`
- Docs: `docs/graph_weighted_relaxation.md`, `docs/phase_03_graph_weighted_relaxation_final_conclusions.md`

### GPU lesson

This is the strongest counterexample in the project. CPU Dijkstra remains best for chain/grid/layered/small random graphs. GPU global relaxation wins only for sufficiently large low-diameter random graphs. Frontier, delta, and light/heavy delta variants were useful educational attempts, but not recommended winners in this implementation.

### References

- Dijkstra's algorithm: https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm
- Bellman-Ford algorithm: https://en.wikipedia.org/wiki/Bellman%E2%80%93Ford_algorithm
- Delta-stepping: https://en.wikipedia.org/wiki/Parallel_single-source_shortest_path_algorithm#Delta_stepping_algorithm
- Original delta-stepping paper: https://www.sciencedirect.com/science/article/pii/S0196677403000762

## Constraint satisfaction / candidate validation

### Problem definition

Constraint satisfaction asks whether assignments satisfy rules such as skill compatibility, capacity, time windows, distance, forbidden zones, and risk tolerance.

### Repository benchmark

- `constraint_network`
- Docs: `docs/constraint_network.md`, `docs/phase_04_constraint_network.md`, `docs/phase_04_constraint_network_problem_visualization.md`

### GPU lesson

Candidate validation is a strong GPU-support workload: each candidate can be evaluated independently, and outputs can be summarized as validity counts, violation masks, and penalties.

### References

- Constraint satisfaction problem: https://en.wikipedia.org/wiki/Constraint_satisfaction_problem
- Constraint programming: https://en.wikipedia.org/wiki/Constraint_programming

## Combination finding / exhaustive enumeration

### Problem definition

A k-combination is a selection of k elements from n elements without regard to order. The number of possible combinations grows as a binomial coefficient, creating combinatorial explosion.

### Repository benchmark

- `combination_finder`
- Docs: `docs/phase_04_combination_finder.md`, `docs/phase_04_combination_problem_definitions.md`

### GPU lesson

The GPU can evaluate many combinations independently, but output must be controlled: count, reduce, or keep summaries rather than copying every valid combination back.

### References

- Combination: https://en.wikipedia.org/wiki/Combination
- Binomial coefficient: https://en.wikipedia.org/wiki/Binomial_coefficient
- Combinatorial explosion: https://en.wikipedia.org/wiki/Combinatorial_explosion
- Brute-force / exhaustive search: https://en.wikipedia.org/wiki/Brute-force_search
- Backtracking: https://en.wikipedia.org/wiki/Backtracking

## Local-search move evaluation

### Problem definition

Local search moves from one candidate solution to neighboring solutions by applying local changes. In many solvers, a major cost is evaluating a large neighborhood of possible moves.

### Repository benchmark

- `local_search_moves`
- Docs: `docs/local_search_moves.md`, `docs/phase_04_local_search_moves.md`, `docs/phase_04_local_search_moves_speedup_plateau.md`

### GPU lesson

The benchmark focuses on move scoring, not a full metaheuristic. GPU speedup rises until launch/copy/reduction overhead is amortized, then stabilizes at a steady throughput ratio.

### References

- Local search in optimization: https://en.wikipedia.org/wiki/Local_search_(optimization)
- 2-opt: https://en.wikipedia.org/wiki/2-opt
- Local search heuristics for multidimensional assignment: https://arxiv.org/abs/0806.3258

## Scenario simulation / robust planning

### Problem definition

Scenario simulation evaluates one plan under many possible worlds: resource failure, demand spikes, delays, cost/risk changes, and weather-like penalties. It is related to Monte Carlo simulation and optimization under uncertainty.

### Repository benchmark

- `scenario_simulation`
- Docs: `docs/scenario_simulation.md`, `docs/phase_04_scenario_simulation.md`, `docs/phase_04_scenario_simulation_feasibility_calibration_v2.md`

### GPU lesson

This is one of the strongest positive GPU examples: many independent scenarios map directly to GPU threads, and the calibrated model now separates CPU/GPU correctness from robust-plan feasibility.

### References

- Monte Carlo method: https://en.wikipedia.org/wiki/Monte_Carlo_method
- Robust optimization: https://en.wikipedia.org/wiki/Robust_optimization
- Stochastic programming: https://en.wikipedia.org/wiki/Stochastic_programming

## Final lecture package

The final lecture package does not introduce a new mathematical problem. It organizes the completed problem families into a teaching sequence:

1. dense independent workloads where GPU helps immediately,
2. graph workloads where anatomy and CPU baselines matter,
3. optimization-support workloads where GPU acts as a scorer, validator, reducer, or simulator.

Use these files together:

- `docs/lecture_packaging.md`
- `docs/live_demo_script.md`
- `docs/final_benchmark_conclusions.md`
- `docs/gpu_decision_guide.md`
- `docs/final_report_outline.md`

## Summary: what the literature and benchmarks support

1. Dense independent evaluation is the cleanest GPU fit.
2. Graph traversal and weighted shortest path are difficult because of irregular memory access, synchronization, and strong CPU baselines.
3. GPU graph wins usually require algorithms designed for the hardware, not merely ported loops.
4. Candidate scoring, validation, enumeration, local move evaluation, and scenario simulation are often better practical GPU entry points for classical algorithm teams.
5. The recommended real-world pattern is often hybrid: CPU controls the solver, GPU evaluates or reduces large candidate spaces.
