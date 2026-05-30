# GPU decision guide for classical algorithms

Use this checklist when deciding whether to request GPU support for a classical-algorithm workload.

## Quick decision matrix

| Question | GPU signal | CPU signal |
|---|---|---|
| Can the work be split into thousands/millions of independent items? | Yes | No |
| Does each item do enough work to amortize overhead? | Yes | No |
| Can output be reduced instead of copied fully? | Yes | No |
| Does the algorithm need many global synchronization rounds? | No | Yes |
| Is there a very strong CPU baseline? | Maybe | Yes |
| Does data stay resident on GPU across iterations? | Yes | No |
| Is the graph frontier wide and low-depth? | Yes | No |
| Is the graph chain/grid-like with narrow progress? | No | Yes |

## Recommended backend patterns

### CPU-only

Use CPU-only for small workloads, high-diameter graph traversal, priority-queue-heavy weighted shortest path, and cases where the best CPU algorithm is already near-linear and low-overhead.

### GPU-only batch evaluation

Use GPU when the problem is naturally a large batch of independent evaluations: polynomial values, cost matrix cells, spatial event pairs, candidate validation, scenario scoring.

### Hybrid CPU/GPU

Use hybrid when the GPU can reduce a large candidate space before the CPU solver takes over. This is the best pattern for assignment preprocessing, local-search move scoring, and robust-planning evaluation.

## Practical policy

1. Build the CPU baseline first.
2. Add GPU only when the workload has enough parallelism.
3. Measure total GPU time, not only kernel time.
4. Plot speedup against problem anatomy, not only size.
5. Prefer GPU for evaluation/scoring/filtering; keep control-heavy solver logic on CPU unless there is a proven GPU algorithm.
