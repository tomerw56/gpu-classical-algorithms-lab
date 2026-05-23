# Phase 3.4.6 - Weighted relaxation: delta scheduler fix

The first `gpu-delta-stepping` experiment could report `correct=no` on high-diameter cases such as `chain_256` and stop the sweep after the first command.

The issue was not the distance relaxation formula. It was the scheduler guard.

The delta-style implementation has two kinds of loop steps:

1. **Expansion steps** - process the active bucket/frontier and relax outgoing edges.
2. **Bucket-selection steps** - move deferred future nodes into the next active bucket.

Only expansion steps correspond to useful relaxation iterations. The previous outer loop used the normal relaxation `max_iterations` as the total scheduler loop bound. For chain-like graphs, many vertices may fall into distinct distance buckets. That means the algorithm can need roughly one bucket-selection step per useful expansion step.

For example, a chain with `N` nodes may need up to `N` useful expansions, but also many future-bucket selection/partition steps. Using only `N` total scheduler steps could stop before all queued future buckets were processed.

The fix keeps the reported convergence metric as the useful expansion iteration count, but gives the bucket scheduler a larger operation budget:

```text
max_scheduler_steps = max(max_iterations * 4, node_count * 4, 64)
```

This keeps `gpu-delta-stepping` from failing early on chain/grid-style cases while still protecting the benchmark from accidental infinite loops.

The console result table was also widened so the long variant name `gpu-delta-stepping` no longer runs into the preset column.
