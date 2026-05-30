# Phase 4.2 problem definitions and verified sources

This page gives the source-backed problem definitions behind the `combination_finder` benchmark.
The benchmark is intentionally simple: enumerate many unordered candidate groups, evaluate each group independently, and keep compact aggregate outputs such as valid count and best score.

## Combination / k-combination

A **combination** is a selection of items where order does not matter. In this project, a candidate group such as `{item 2, item 7, item 19}` is the same group regardless of the order in which those indices are written.

For a set of `n` items, the number of unordered groups of size `k` is commonly written as:

```text
C(n, k) = n choose k
```

The Wikipedia pages for [Combination](https://en.wikipedia.org/wiki/Combination) and [Binomial coefficient](https://en.wikipedia.org/wiki/Binomial_coefficient) both describe the same count: the number of ways to choose `k` elements from an `n`-element set.

## Combinatorial explosion

The benchmark intentionally shows **combinatorial explosion**: increasing `n` or `k` causes the number of groups to grow very quickly.
The [Combinatorial explosion](https://en.wikipedia.org/wiki/Combinatorial_explosion) page defines this as rapid growth in problem complexity due to the combinatorial dependence on input, constraints, or bounds.

Examples:

```text
C(64, 3)   =      41,664
C(192, 3)  =   1,161,280
C(384, 3)  =   9,342,784
C(128, 4)  =  10,668,000
C(512, 3)  =  22,238,720
```

This growth is exactly why the GPU can be attractive: every group can be evaluated independently.
It is also exactly why output control matters: copying every valid group back to the CPU can be more expensive than the computation.

## Exhaustive / brute-force search

`combination_finder` is a controlled exhaustive search benchmark. It enumerates all candidate groups of a chosen size and checks each group against a scoring/validity function.
The [Brute-force search](https://en.wikipedia.org/wiki/Brute-force_search) page describes exhaustive search as systematically checking candidates, and notes that brute-force methods are generally practical only for limited problem sizes or when heuristics reduce the candidate set.

In our project, the point is not to recommend brute force as a final solver for every real problem. The point is to benchmark a common substep:

```text
generate many candidate groups
score or reject each group
keep compact summary outputs
```

## Backtracking and pruning

The benchmark also contrasts with smarter CPU-side approaches such as backtracking. The [Backtracking](https://en.wikipedia.org/wiki/Backtracking) page describes incrementally building candidates and abandoning candidates as soon as they cannot become valid solutions.

That is important for the lecture:

```text
GPU exhaustive evaluation:
    good when many independent candidates survive long enough to evaluate

CPU/backtracking/branch-and-bound:
    good when pruning can eliminate most candidates early
```

So this benchmark is not saying “always enumerate everything on GPU.” It is saying “when a workflow already has many independent candidate evaluations, GPU throughput can be useful.”

## CUDA relevance

The official [CUDA C++ Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/) describes CUDA as NVIDIA's parallel computing platform/programming model for writing programs that execute on GPUs. In this benchmark, that model maps naturally to:

```text
one CUDA thread = one candidate combination
```

The benchmark intentionally returns compact aggregate values:

```text
valid_count
invalid_count
violation counts
best_score
checksum
```

rather than returning every valid combination. This keeps the example focused on throughput and avoids an output explosion.
