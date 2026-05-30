# Final benchmark conclusions

This file summarizes the final teaching conclusions from the benchmark lab.

## Overall conclusion

The GPU is most useful in this project as a **batch evaluator** and **search-space reducer**.
It is less consistently useful as a drop-in replacement for classical graph algorithms with strong CPU baselines.

## Strong GPU-friendly workloads

| Workload | Why GPU helps |
|---|---|
| Polynomial batch | One independent evaluation per `x`. |
| Cost matrix | Dense task/resource pair evaluation. |
| Spatial events | Dense track/zone pair evaluation. |
| Constraint validation | One candidate assignment per thread. |
| Combination finder | Large independent candidate space. |
| Assignment preprocessing | Full pair space reduced to top-K candidates. |
| Local-search moves | Many candidate moves scored independently. |
| Scenario simulation | Many independent possible worlds. |

## Nuanced graph results

| Workload | Final lesson |
|---|---|
| BFS | Graph anatomy dominates. Chain/grid are poor; large random graphs can be GPU-friendly. |
| Connected components | Non-naive GPU improves substantially; random/grid can win; mixed anatomy needs care. |
| Weighted shortest path | CPU Dijkstra is hard to beat. GPU global scan wins only on sufficiently large random graphs. Delta/frontier variants were useful counterexamples, not recommended winners. |

## The three hardware answers

### Choose CPU when

- The graph has high diameter and narrow frontiers.
- A strong CPU algorithm exists, such as Dijkstra or Union-Find.
- The problem is small or latency-bound.
- The GPU would require many synchronization rounds.

### Choose GPU when

- There are many independent evaluations.
- The output can be reduced or summarized on-device.
- The workload is large enough to amortize launch and transfer costs.
- Data can remain on the GPU across repeated evaluations.

### Choose hybrid when

- The CPU/solver owns the main control flow.
- The GPU can score/filter/evaluate a large candidate set.
- The GPU reduces the problem before a CPU assignment, search, or optimization stage.

## Most lecture-worthy examples

1. **Scenario simulation** - strongest positive example.
2. **Assignment preprocessing** - best real-world hybrid optimization story.
3. **BFS shape x scale** - best graph-anatomy story.
4. **Weighted relaxation** - best counterexample against naive GPU optimism.
5. **Local-search moves** - useful throughput plateau story.
