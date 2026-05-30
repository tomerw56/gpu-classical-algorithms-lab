# Phase 4.5.1 - scenario simulation sweep trim

The original scenario-simulation runner included a very large stress point:

```text
sc_16m: tasks=256, resources=128, scenarios=16,777,216
```

That case was removed from the default project flow because it took too long to run and did not add enough explanatory value. The scenario-simulation phase is meant to demonstrate the GPU throughput trend and robust-planning evaluation pattern, not to turn the default sweep into a long stress test.

The canonical runner now stops at:

```text
sc_4m
```

by default.

An optional stress point remains behind:

```bat
set "INCLUDE_HEAVY_CASES=1"
```

which adds:

```text
sc_8m
```

This is enough to check the larger-scenario behavior without making the default workflow painfully slow.

## Recommended usage

For normal lecture/demo work:

```bat
set "INCLUDE_HEAVY_CASES=0"
```

For a deeper local benchmark run:

```bat
set "INCLUDE_HEAVY_CASES=1"
```

Avoid adding back `sc_16m` unless the goal is specifically a long stress benchmark.
