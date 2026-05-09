# Benchmarking Methodology

## Repeat and warmup

Every benchmark supports:

- `--warmup N`: executions not included in the reported time.
- `--repeat N`: executions included in the reported time.

The current infrastructure reports aggregate time over the measured repeats. Later phases may add min/mean/max/p95.

## CPU timing

CPU timing uses `std::chrono::steady_clock`.

## GPU timing

CUDA benchmarks should report:

- Host-to-device copy time.
- Kernel time.
- Device-to-host copy time.
- Total GPU time.

This split is important because a GPU kernel can look impressive while end-to-end performance is worse than CPU due to transfer overhead.

## Correctness

Every CPU/GPU pair should compare results. Floating-point examples should use tolerance-based comparison.

Suggested tolerances:

```text
float:  1e-4 to 1e-5 depending on scale
double: 1e-9 to 1e-12 depending on scale
```

## Input sizes

Use presets:

- `tiny`: sanity tests.
- `small`: fits quick local run.
- `medium`: meaningful benchmark.
- `large`: stresses the implementation.
- `huge`: optional; may require serious memory/time.

## What to plot later

- CPU total time vs input size.
- GPU end-to-end time vs input size.
- GPU kernel-only time vs input size.
- Speedup ratio.
- Break-even point.
- Transfer overhead percentage.
