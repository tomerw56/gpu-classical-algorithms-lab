# Phase 4.1: Constraint-network candidate validation

This phase starts the combinatorics/optimization section of the project.

The goal is to show a classical algorithmic pattern where the GPU is useful even when the final optimizer remains CPU-side:

```text
generate many candidate assignments
validate and score each candidate independently
send a smaller/better set of candidates to a solver
```

## Implemented benchmark

```text
constraint_network
```

Variants:

```text
cpu  nested candidate loop
gpu  one CUDA thread per candidate
```

## Presets

| preset | tasks | resources | candidates |
|---|---:|---:|---:|
| tiny | 64 | 32 | 4,096 |
| small | 512 | 256 | 262,144 |
| medium | 2,048 | 1,024 | 1,048,576 |
| large | 8,192 | 2,048 | 4,194,304 |

All values can be overridden:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark constraint_network --preset tiny ^
  --set tasks=1000 --set resources=250 --set candidates=2000000
```

## Fine-grained sweep sizes

The canonical runner now uses a denser size sweep than the four built-in presets so the plots can show the CPU/GPU crossover region more clearly.

Default sweep points:

| label | tasks | resources | candidates | repeat |
|---|---:|---:|---:|---:|
| `cn_4k` | 64 | 32 | 4,096 | 5 |
| `cn_16k` | 128 | 64 | 16,384 | 5 |
| `cn_64k` | 256 | 128 | 65,536 | 5 |
| `cn_128k` | 384 | 192 | 131,072 | 5 |
| `cn_256k` | 512 | 256 | 262,144 | 5 |
| `cn_512k` | 1,024 | 512 | 524,288 | 5 |
| `cn_1m` | 2,048 | 1,024 | 1,048,576 | 5 |
| `cn_2m` | 4,096 | 1,024 | 2,097,152 | 3 |
| `cn_4m` | 8,192 | 2,048 | 4,194,304 | 3 |

Optional heavy points are controlled at the top of `execute_constraint_network_all_sweeps_and_analyze.bat`:

```bat
set "INCLUDE_HEAVY_CASES=1"
```

This adds:

| label | tasks | resources | candidates | repeat |
|---|---:|---:|---:|---:|
| `cn_8m` | 16,384 | 4,096 | 8,388,608 | 2 |
| `cn_16m` | 32,768 | 8,192 | 16,777,216 | 1 |

The labels are passed through `--set sweep_label=...`, so the console table, JSONL, reports, and plots use meaningful labels instead of only `tiny`/`small`/`medium`/`large`.

## What to look for

This is expected to behave more like polynomial/cost-matrix/spatial-events than graph BFS:

- tiny: CPU may win
- larger batches: GPU should improve
- very large batches: GPU should usually win if transfer overhead is amortized

## Outputs

The benchmark records:

- `valid_count`
- `invalid_count`
- `total_violations`
- mismatch counts
- checksum and reference checksum
- max absolute/relative penalty error

## Single workflow

```bat
execute_constraint_network_all_sweeps_and_analyze.bat
```

This writes:

```text
results\constraint_network_scale_sweep.jsonl
results\constraint_network_scale_analysis\
results\constraint_network_scale_plots\
```


## Penalty validation tolerance

The categorical results are still checked exactly:

```text
valid flag
violation mask
violation count
```

The floating-point `penalty` field is checked with a combined absolute/relative tolerance. This is intentional: the CPU and CUDA paths both use single-precision arithmetic, but the host compiler and CUDA compiler may make slightly different choices around multiply-add fusion and instruction ordering. Larger candidate sets are more likely to encounter cases where the absolute difference exceeds a tiny fixed threshold even though the categorical constraint decision is identical.

The benchmark records:

```text
max_abs_error
max_rel_error
penalty_abs_tolerance
penalty_rel_tolerance
```

A row is considered correct when categorical fields match exactly and the penalty difference is within the combined tolerance.


## Constraint-network diagnostics

Phase 4.1 includes a richer diagnostic plotting script:

```bat
python scripts\plot_constraint_network_diagnostics.py ^
  --jsonl results\constraint_network_scale_sweep.jsonl ^
  --output-dir results\constraint_network_diagnostics ^
  --show
```

The canonical runner also invokes this script automatically. It visualizes valid/invalid ratios, violation reasons, GPU transfer/kernel/output-copy breakdown, and CPU/GPU penalty validation error.

## Problem-definition visualization

Phase 4.1.4 adds a visualization flow for the generated constraint problem. It answers questions such as:

```text
Which resources can serve task A?
Which resources fail due to skills, capacity, distance, risk, or zone?
Which concrete candidates fail even when the task/resource pair is plausible?
```

Run:

```bat
build-cuda-ninja\export_constraint_network.exe --tasks 32 --resources 16 --candidates 512 --output-dir results\constraint_network_problem_demo
python scripts\plot_constraint_network_problem.py --input-dir results\constraint_network_problem_demo --show
```

The canonical sweep runner also exports and plots one inspectable demo instance automatically.
