# Cost Matrix Benchmark

## Problem

`cost_matrix` models the common pre-processing step in assignment, dispatch, and resource-allocation systems:

```text
cost[task][resource]
```

Each cell answers two questions:

1. Is this task/resource pair feasible?
2. If feasible, what is its cost?

The benchmark uses deterministic synthetic `Task` and `Resource` arrays and computes a dense matrix of cells.

## Synthetic input generation

### `make_resources(resource_count)`

`make_resources()` creates deterministic benchmark inputs rather than random inputs. That matters for repeatable CPU/GPU validation and for comparing timing runs over time.

Each resource receives:

- an `(x, y)` location,
- a load ratio,
- a speed,
- an `available_at` time,
- a three-bit `skill_mask`.

For resource index `j`, the implementation uses fixed arithmetic patterns:

```cpp
resource.x = ((j * 29 + 7) % 1600) * 0.42f;
resource.y = ((j * 43 + 5) % 1600) * 0.38f;
resource.load = ((j * 11) % 101) / 100.0f;
resource.speed = 0.85f + (j % 6) * 0.18f;
resource.available_at = ((j * 23) % 90) * 2.0f;
```

Skills are deliberately spread across eight skill types:

```cpp
skill0 = j % 8;
skill1 = (j + 3) % 8;
skill2 = (j + 5) % 8;
resource.skill_mask = (1u << skill0) | (1u << skill1) | (1u << skill2);
```

A few concrete examples:

| Resource index | Load | Speed | Available at | Skills | Skill mask |
|---:|---:|---:|---:|---|---:|
| `0` | `0.00` | `0.85` | `0.0` | `{0, 3, 5}` | `41` |
| `1` | `0.11` | `1.03` | `46.0` | `{1, 4, 6}` | `82` |
| `2` | `0.22` | `1.21` | `92.0` | `{2, 5, 7}` | `164` |

The examples help explain why some task/resource pairs pass skill compatibility and others are rejected immediately.

### `make_tasks(task_count)`

`make_tasks()` follows the same deterministic style. Every task receives a location, deadline, priority, one required skill bit, and an urgency flag. For example:

```cpp
task.required_skill = 1u << (i % 8);
task.urgent = (i % 6) == 0 ? 1 : 0;
```

That means task `0` is urgent and requires skill `0`, task `1` requires skill `1`, and so on.

## Dense matrix creation and storage

The benchmark logically builds:

```text
cost[task_index][resource_index]
feasible[task_index][resource_index]
```

For compact storage and a GPU-friendly output layout, both matrices are stored as flat row-major arrays:

```text
flat_index = task_index * resource_count + resource_index
```

Example: with `2` tasks and `3` resources, the flat storage order is:

| Flat index | Logical cell |
|---:|---|
| `0` | `task 0 / resource 0` |
| `1` | `task 0 / resource 1` |
| `2` | `task 0 / resource 2` |
| `3` | `task 1 / resource 0` |
| `4` | `task 1 / resource 1` |
| `5` | `task 1 / resource 2` |

Another example: with `4` resources, cell `(task=2, resource=3)` is stored at:

```text
2 * 4 + 3 = 11
```

The CPU implementation reaches that index through nested loops. The CUDA kernel starts from a thread-owned `flat_index` and reconstructs:

```text
task_index = flat_index / resource_count
resource_index = flat_index - task_index * resource_count
```

That mapping is the core of the one-thread-per-cell GPU design.

## Worked pair examples

### Example 1: skill mismatch becomes infeasible

Suppose a task requires only skill `7`:

```text
task.required_skill = 1u << 7
```

and a resource supports only skill `1`:

```text
resource.skill_mask = 1u << 1
```

Then:

```text
(resource.skill_mask & task.required_skill) == 0
```

so the pair is rejected immediately:

```text
feasible = 0
cost = 1.0e30
```

### Example 2: a deliberately feasible pair

The unit test creates a pair where:

- the resource skill mask matches the task requirement,
- task and resource share the same location,
- `available_at = 0`,
- `speed = 1`,
- `load = 0.1`,
- the deadline is set far in the future.

That removes the major rejection paths, so `evaluate_pair()` produces:

```text
feasible = 1
cost < 1.0e30
```

## Cost model

Each pair evaluates several branches:

- Required skill must match the resource skill mask.
- Urgent tasks use a tighter dispatch-radius threshold.
- Excessive lateness rejects the pair.
- Urgent and non-urgent tasks use different lateness penalties.
- High resource load adds nonlinear tiered penalties.
- A small location-dependent “zone proxy” penalty simulates extra business logic.

Infeasible cells are represented by:

```text
feasible = 0
cost = 1.0e30
```

## CPU implementation

The CPU baseline uses nested loops:

```text
for each task:
    for each resource:
        evaluate_pair(task, resource)
```

This is intentionally straightforward and easy to inspect.

## GPU implementation

The CUDA version maps one matrix cell to one thread:

```text
thread i -> task/resource pair i
```

The kernel computes the flattened matrix index, reconstructs task/resource indices, evaluates the same branch-heavy cost function, and writes:

- `costs[cell]`
- `feasible[cell]`

## Why GPU might help

A large cost matrix is naturally data-parallel:

- Every cell can be evaluated independently.
- Large task/resource sets produce millions of pair evaluations.
- A single GPU launch can cover the full dense matrix.

This is one of the most realistic non-ML GPU opportunities for classical algorithm teams.

## Why GPU might struggle

This benchmark is intentionally not “perfect GPU food.” It includes branch-heavy logic:

- Compatibility rejection.
- Distance-limit rejection.
- Lateness rejection.
- Urgency-dependent penalties.
- Load-tier penalties.

Different threads in a warp may follow different branches, causing **branch divergence**. This makes the benchmark useful for teaching both strengths and limitations.

The output matrix can also become large. For the `large` preset, the benchmark writes more than sixteen million cells, so memory traffic and device-to-host transfer matter.

## Presets

| Preset | Tasks | Resources | Cells |
|---|---:|---:|---:|
| `tiny` | 64 | 64 | 4,096 |
| `small` | 512 | 512 | 262,144 |
| `medium` | 2,048 | 2,048 | 4,194,304 |
| `large` | 4,096 | 4,096 | 16,777,216 |

## Correctness policy

Validation compares the final matrix element-by-element against the CPU reference cost function:

- Feasibility flags must match.
- Costs must agree within a floating-point tolerance.
- Metadata records:
  - `checksum`
  - `reference_checksum`
  - `feasible_count`
  - `reference_feasible_count`
  - `feasibility_mismatches`
  - `max_abs_error`
  - `max_rel_error`

Benchmark repeats affect timing only. They do **not** multiply the final checksum.

## Visualization utility

The benchmark executable is intentionally focused on timing and validation. For visual inspection, the project also provides a separate exporter:

```bat
build-cuda-ninja\export_cost_matrix.exe --preset tiny --output-dir results\cost_matrix_tiny
```

The exporter writes:

```text
costs.csv       raw numeric benchmark costs, including the infeasible sentinel
feasible.csv    dense 0/1 feasibility mask
tasks.csv       generated task inputs
resources.csv   generated resource inputs
metadata.json   matrix shape and export summary
```

That separation matters: exporting CSV files is useful for debugging, but it would contaminate benchmark timing if it happened inside `gpu_algobench`.

A stronger plotting utility lives in:

```text
scripts/plot_cost_matrix.py
```

It consumes the exported CSV files and creates:

```text
cost_matrix_heatmap.png
feasibility_mask.png
cost_matrix_surface.png
```

Usage:

```bat
python scripts\plot_cost_matrix.py --input-dir results\cost_matrix_tiny --show
```

For a custom inspectable shape:

```bat
build-cuda-ninja\export_cost_matrix.exe --tasks 96 --resources 128 --output-dir results\cost_matrix_96x128
python scripts\plot_cost_matrix.py --input-dir results\cost_matrix_96x128 --max-surface-dim 160
```

The Python script masks infeasible cells before plotting the 2D cost heatmap, so the `1.0e30` sentinel does not flatten the visual scale. For the 3D surface only, infeasible cells are rendered on a zero-cost floor; otherwise a sparse NaN-masked matrix can draw almost no surface quads. The 3D surface is downsampled automatically when the exported matrix is dense; this keeps surface rendering readable and bounded.

## Run

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark cost_matrix --preset small --repeat 5 --warmup 1
```

Try progressively larger cases:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark cost_matrix --preset medium --repeat 3 --warmup 1
build-cuda-ninja\gpu_algobench.exe --benchmark cost_matrix --preset large --repeat 2 --warmup 1
```

## Expected lesson

The cost matrix should show a practical GPU win at sufficiently large sizes, but the speedup will not be as clean as the polynomial benchmark because the cost logic includes branch divergence and large memory writes.
