# Phase 3.4.2 retrospective: why the first frontier attempt was worse

The first `gpu-frontier` implementation was correct but slower than the global edge-scan variant on the measured sweep. The analysis report showed that the frontier row was usually slower than the global GPU row, even on graph families where the frontier idea should have helped.

The reason was an implementation mistake in the *shape of the work*, not in shortest-path correctness:

```text
old frontier prototype:
    expand current active frontier
    mark improved target nodes in a node-sized flag array
    scan all nodes to compact flags into the next frontier
```

That final compaction pass scanned `node_count` entries every iteration. For high-diameter graphs such as chains and grids, that meant the frontier method still paid large per-level overhead, even when the actual active frontier was tiny.

The patched frontier method now uses direct append:

```text
patched frontier method:
    expand current active frontier
    when a target distance improves:
        append target directly to the next frontier with atomicAdd
```

This removes the full-node compaction pass. The new implementation may produce duplicate frontier entries, so it remains educational rather than production-grade. A production implementation would use more advanced queue management, delta-stepping buckets, warp/block-level aggregation, or library implementations.

## Expected interpretation

After this patch, `gpu-frontier` should be judged empirically:

- It may improve chain/grid/layered cases because it avoids full edge scans and full node compaction.
- It may still lose to `gpu` on random graphs because the active frontier can become a large fraction of the graph and duplicate entries/atomic appends add overhead.
- It may still lose to CPU Dijkstra on small or high-diameter graphs because kernel-launch synchronization remains expensive.

The important lesson is that a GPU-friendly idea must be implemented with GPU-friendly primitives. Avoiding full edge scans is not enough if the replacement scans all nodes every iteration.


## Weighted-relaxation backend policy

See `phase_03_graph_weighted_relaxation_backend_policy.md` for the current conclusion on CPU Dijkstra vs global GPU relaxation vs active-frontier GPU relaxation, including why the frontier method improved but still usually loses.
