# Phase 3.4.8 - Final weighted SSSP attempt: light/heavy delta-style GPU

This phase adds the final weighted-shortest-path GPU experiment in this project:

```text
gpu-delta-light-heavy
```

The goal is to test a closer delta-stepping idea than the earlier simplified
bucket queue. In classic delta stepping, edges are separated into:

```text
light edges: weight <= delta
heavy edges: weight >  delta
```

The idea is:

1. Work on one distance bucket at a time.
2. Repeatedly relax light edges inside the current bucket until the bucket is closed.
3. Only then relax heavy outgoing edges from the nodes settled in that bucket.
4. Move to the next non-empty future bucket.

This is still an educational implementation. It is not a production graph library implementation.
It still uses simple append-frontiers, atomics, host-side bucket checks, and no aggressive
per-bucket deduplication.

## Why this is the last weighted attempt

The weighted graph results so far are already useful:

- CPU Dijkstra is very strong on chain/grid/layered cases.
- The global GPU scan can win on large low-diameter random graphs.
- Frontier and simple delta variants reduce some waste but introduce queue and scheduling overhead.
- The light/heavy delta variant is the last attempt before we mark this chapter as a counterexample.

If `gpu-delta-light-heavy` does not beat the simpler methods, the conclusion is not failure.
The conclusion is:

```text
Weighted shortest path needs serious GPU graph algorithms, not small CUDA adaptations.
```

Future production directions would include:

- real device-side bucket queues,
- per-bucket deduplication,
- separate CSR structures for light and heavy edges,
- better load balancing by degree,
- fewer host synchronizations,
- or using a mature graph library such as cuGraph.

## New benchmark row

Each weighted-relaxation run now emits:

```text
cpu
gpu
gpu-frontier
gpu-delta-stepping
gpu-delta-light-heavy
```

The analyzer and backend recommender include all five backends.
