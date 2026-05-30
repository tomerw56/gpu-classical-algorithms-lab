# Phase 4.5.3 - scenario feasibility calibration, second pass

The first feasibility-calibration patch clarified the semantics of `correct`, but user results still showed every scenario as infeasible. That meant the benchmark was a strong GPU-throughput example but still weak as a robust-planning demo.

This patch makes the generated plan and uncertainty model less degenerate:

- base task demand is slightly lower,
- resource capacity and speed are more forgiving,
- the base assignment prefers resources that can absorb ordinary demand spikes,
- normal delay/risk multipliers are milder,
- severe delay/risk scenarios still occur periodically.

The goal is not to make the plan always feasible. The goal is to produce a useful mix:

```text
implementation correctness: CPU/GPU agreement
robustness quality: feasible_ratio, violation mix, robustness_score
```

The analyzer still warns when a sweep has feasible ratio exactly 0.0 or 1.0 across all points.
