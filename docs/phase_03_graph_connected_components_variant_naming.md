# Phase 3.3.4 - Connected-components variant naming cleanup

The connected-components benchmark now has three conceptual variants:

```text
cpu              CPU Union-Find baseline
gpu-naive        original CUDA label-propagation implementation
gpu-non-naive    root-hooking + pointer-jumping CUDA implementation
```

For backward compatibility, the executable still emits the naive GPU row with the variant name:

```text
gpu
```

The analysis scripts normalize that mentally and in labels to:

```text
gpu-naive
```

Earlier CSV files contained both:

```text
gpu_ms_per_run
gpu_naive_ms_per_run
```

with identical values. That was confusing because `gpu_ms_per_run` was only an alias for the naive implementation, not a separate algorithm. The analysis CSV now removes the generic `gpu_ms_per_run` column and keeps only explicit variant columns:

```text
gpu_naive_ms_per_run
gpu_non_naive_ms_per_run
speedup_cpu_over_gpu_naive
speedup_cpu_over_gpu_non_naive
gpu_naive_to_non_naive_speedup
```

## Reading the result

`gpu_naive_to_non_naive_speedup` is the direct comparison between the two GPU implementations:

```text
naive GPU ms/run / non-naive GPU ms/run
```

So:

```text
> 1.0  non-naive is faster than naive
= 1.0  no difference
< 1.0  naive is faster than non-naive
```
