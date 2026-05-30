# Phase 4.2: combination finder

`combination_finder` evaluates many unordered candidate groups under simple validity and scoring rules.

## Problem

Given `n` generated items, choose `k` items at a time and evaluate every candidate group.

Each item has:

```text
value
cost
risk
x/y location
category mask
```

A combination is valid when it satisfies all of these rules:

```text
total cost <= budget
total risk <= risk limit
combined category mask covers the required categories
maximum pairwise spread <= spread limit
```

The score is an integer expression combining value, cost, risk, diversity, and compactness.

## CPU baseline

The CPU baseline exhaustively enumerates the combinations and evaluates each group.

```text
for rank in all combinations:
    unrank rank -> item indices
    evaluate group
    update aggregate counters
```

## GPU implementation

The GPU implementation maps candidate combinations directly onto CUDA threads:

```text
one thread = one combination rank
```

Each thread un-ranks its combination, evaluates the constraints and score, then contributes to compact aggregate outputs using atomics.

## Why this is a good GPU example

The evaluation of one group does not depend on the result of another group. That makes it much more GPU-friendly than BFS or weighted shortest path, which have traversal/convergence dependencies.

## Pitfalls

The important pitfall is output size. If millions of combinations are valid, copying all valid groups back to the CPU can dominate runtime and memory.

So this benchmark intentionally returns only aggregate results:

```text
valid_count
invalid_count
best_score
violation-reason counts
checksum
```

This matches a common real-world pattern: use GPU to score or filter a large candidate set, then let a CPU solver inspect a much smaller summary or top-K set.

## Run

Direct benchmark:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark combination_finder --preset small --repeat 5 --warmup 1
```

Full sweep/analyze/plot flow:

```bat
execute_combination_finder_all_sweeps_and_analyze.bat
```

## Related problem definitions

See `docs/phase_04_combination_problem_definitions.md` for verified external sources on combinations, binomial coefficients, combinatorial explosion, brute-force search, backtracking, and CUDA.
