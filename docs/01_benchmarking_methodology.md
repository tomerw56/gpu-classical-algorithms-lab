# Benchmarking Methodology

## Repeat and warmup

Every benchmark supports:

- `--warmup N`: executions not included in the reported time.
- `--repeat N`: executions included in the reported time.

The current infrastructure reports aggregate time over the measured repeats. Later phases may add min/mean/max/p95.

`execute_all_tests.bat` centralizes these values at the top of the file:

```bat
set "BENCHMARK_PRESET=small"
set "BENCHMARK_REPEAT=5"
set "BENCHMARK_WARMUP=1"
```

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

Every CPU/GPU pair should compare results. Floating-point examples should use tolerance-based comparison. The polynomial benchmark uses element-wise CPU-reference validation and reports `max_abs_error` and `max_rel_error`. The cost-matrix benchmark additionally validates feasibility flags, feasible-pair counts, and branch-heavy cost values. The spatial-event benchmark validates exact event codes, event counts, per-event-type counts, and tolerance-based event scores.

Important rule: `--repeat` is a timing control, not a correctness multiplier. If a benchmark executes the same transform 5 times, the reported checksum should normally describe the final single output, not the sum of 5 checksums. Otherwise CPU and GPU rows may be timing comparable but validating different effective values.

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

## Test coverage

The tests intentionally check benchmark semantics, not raw performance. For example, `test_foundation` verifies that repeat count affects timing but does not alter the meaning of the reported checksum. `test_polynomial` applies the same rule to the first real algorithm benchmark and also checks Horner evaluation against a direct polynomial sum for a representative input. `test_cost_matrix` applies the same repeat/checksum policy to a branch-heavy dense matrix and checks both feasible and infeasible pair construction. `test_spatial_events` applies the same policy to a dense track-zone event matrix and forces `enter`, `exit`, `stay_inside`, `cross_through`, and `none` cases. Future algorithm benchmarks should follow the same pattern: small deterministic correctness cases first, performance measurements second.
