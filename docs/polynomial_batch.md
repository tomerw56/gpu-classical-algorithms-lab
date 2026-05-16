# Polynomial Batch Benchmark

## Problem

This benchmark evaluates a degree-15 polynomial with 16 coefficients over many `x` values.

The motivating shape is:

```text
P(x) = a0 + a1*x + a2*x^2 + ... + a15*x^15
```

For each output element, the benchmark evaluates one polynomial value independently. That makes the workload naturally parallel:

```text
CPU: one loop over all x values
GPU: one CUDA thread per x value
```

## Input domain

The benchmark intentionally uses `x` values spaced by `100`, matching the original motivating question:

```text
100, 200, 300, ...
```

To keep a degree-15 benchmark numerically controlled across large presets, the domain cycles every 4096 elements:

```text
100, 200, ..., 409600, 100, 200, ...
```

This preserves the important algorithmic shape — stride-100 polynomial evaluation — while avoiding the benchmark becoming dominated by runaway polynomial magnitudes.

The coefficients are scaled so that the polynomial remains finite and meaningful on that domain. Internally they represent an alternating series in `(x * 1e-5)`, expressed as an ordinary polynomial in `x`.

## CPU implementation

The CPU implementation uses Horner's method:

```cpp
value = a15;
for (degree = 14; degree >= 0; --degree)
{
    value = value * x + a[degree];
}
```

Horner's method is used because it:

- avoids separately computing `x^2`, `x^3`, ..., `x^15`;
- performs a compact chain of multiply-add operations;
- matches the arithmetic structure used by the GPU kernel.

## GPU implementation

The CUDA version uses:

- one thread per `x` value;
- the same Horner evaluation order;
- 16 coefficients stored in CUDA constant memory;
- CUDA events to report:
  - coefficient host-to-device transfer time;
  - kernel time;
  - device-to-host output transfer time.

The benchmark result reports:

```text
h2d_ms + kernel_ms + d2h_ms = total_ms
```

Allocation time is intentionally not included in `total_ms`; the benchmark focuses on evaluation and transfer cost.

## Why GPU may help

Polynomial evaluation is a strong early GPU example because:

- every output value is independent;
- every thread executes essentially the same instruction pattern;
- memory access is simple and contiguous;
- arithmetic density is higher than the Phase 1 smoke transform.

The expected trend is:

```text
small inputs: CPU may be competitive
larger inputs: GPU kernel time should scale favorably
end-to-end speedup depends on transfer cost
```

## Pitfalls

This benchmark deliberately highlights several issues that matter in real GPU work:

### 1. Kernel-only timing can be misleading

The GPU kernel may be much faster than the CPU loop, while the total GPU path is less dramatic once output transfer is included.

### 2. Numeric choices matter

A degree-15 polynomial can overflow or become poorly conditioned if `x` grows without control. The benchmark uses a bounded stride-100 domain and scaled coefficients to keep the comparison about CPU-vs-GPU execution, not accidental floating-point explosions.

### 3. CPU and GPU floating-point arithmetic may differ slightly

The GPU validation is element-wise and tolerance-based. It compares each final GPU output against the CPU Horner evaluator and records:

```text
max_abs_error
max_rel_error
checksum
reference_checksum
```

### 4. Repeats are timing only

`--repeat` controls how many measured executions are timed. Reported checksums and validation metadata describe the final single output vector, not a repeated accumulation.

## Presets

| Preset | Values |
|---|---:|
| `tiny` | 1,024 |
| `small` | 250,000 |
| `medium` | 1,000,000 |
| `large` | 5,000,000 |

## Example commands

Run CPU and GPU variants:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark polynomial_batch --preset small --repeat 5 --warmup 1
```

Write JSONL results:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark polynomial_batch --preset medium --repeat 5 --warmup 1 --output results\polynomial_medium.jsonl
```

Run through the shared batch tool:

```bat
execute_all_tests.bat
```

Because the batch runner defaults to `BENCHMARK_NAME=all`, it automatically includes `polynomial_batch`.

## Correctness policy

The GPU version copies the final output vector back to the host and compares each element against the CPU Horner reference for the same index.

A result is considered correct only when:

- each output is finite;
- each absolute error is within tolerance;
- the final metadata is internally consistent.
