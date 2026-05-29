# Phase 4.1.4 - Constraint-network problem definition visualization

The earlier constraint-network plots explained performance and aggregate validation results, but they did not show the generated problem itself.
This phase adds a small export/plot flow for the problem definition.

## Exporter

```bat
build-cuda-ninja\export_constraint_network.exe --tasks 32 --resources 16 --candidates 512 --output-dir results\constraint_network_problem_demo
```

The exporter writes:

```text
tasks.csv
resources.csv
pair_compatibility.csv
candidate_evaluations.csv
skill_compatibility_matrix.csv
pair_feasibility_matrix.csv
metadata.json
```

## Plotter

```bat
python scripts\plot_constraint_network_problem.py --input-dir results\constraint_network_problem_demo --show
```

The plotter creates:

```text
constraint_skill_compatibility_matrix.png
constraint_pair_feasibility_matrix.png
constraint_pair_pass_rates.png
constraint_candidate_assignments.png
constraint_candidate_violation_reasons.png
constraint_time_windows.png
constraint_network_problem_report.md
```

## What these plots mean

### Skill compatibility matrix

Rows are tasks and columns are resources. A `1` means the resource has every skill required by the task.
This is the simplest "A can work with B but not C" view.

### Pair feasibility matrix

Rows are tasks and columns are resources. A `1` means the task/resource pair passes all pair-level constraints:

- skill compatibility
- capacity
- time-window overlap
- distance
- forbidden zone
- risk tolerance

This ignores the concrete candidate start time and duration, so it answers:

```text
Can this resource plausibly serve this task at all?
```

### Candidate assignment scatter

This plot shows the actual generated candidate assignments:

```text
candidate = task + resource + start_time + duration
```

A pair that is generally compatible may still fail as a concrete candidate because the proposed start time or duration violates a time window.

## Runner integration

The canonical runner now exports and plots one inspectable problem instance automatically:

```bat
execute_constraint_network_all_sweeps_and_analyze.bat
```

The exported problem goes to:

```text
results\constraint_network_problem_demo
```

and plots go to:

```text
results\constraint_network_problem_plots
```
