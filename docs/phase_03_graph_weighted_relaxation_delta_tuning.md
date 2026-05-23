# Phase 3.4.7 - weighted-relaxation delta tuning

The first `gpu-delta-stepping` experiment used a fixed bucket width:

```text
delta = 16
```

That was too little evidence to conclude whether the idea is bad or merely poorly tuned.
The canonical weighted-relaxation runner now includes an optional delta-tuning step that runs selected graph cases with several bucket widths:

```text
2, 4, 8, 16, 32, 64, 128
```

The generated JSONL is:

```text
results/graph_weighted_relaxation_delta_tuning.jsonl
```

The generated plots are written to:

```text
results/graph_weighted_relaxation_delta_tuning_plots/
```

## Why this matters

The simplified `gpu-delta-stepping` variant is bucketed and active, but it is still an educational approximation rather than a production delta-stepping implementation.
A poor bucket width can cause either:

- too many bucket transitions and scheduler overhead, or
- too little useful ordering, making the algorithm behave like a noisy frontier queue.

The tuning plots compare the delta variant against horizontal CPU, global-GPU, and frontier-GPU reference lines.

## Current expectation

If the delta curve never approaches the global GPU line, the conclusion is not just "bad delta value".
It means this simplified implementation is dominated by bucket scheduling, duplicate frontier entries, or missing production features such as light/heavy edge separation and per-bucket deduplication.
