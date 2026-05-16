# Phase 2.3 - Spatial Event Detection

Phase 2.3 adds the third real algorithm benchmark:

```text
spatial_events
```

## Why this phase exists

The earlier Phase 2 examples show:

- `polynomial_batch`: regular arithmetic over many independent values.
- `cost_matrix`: branch-heavy pair scoring over a dense matrix.

`spatial_events` adds a geometry-flavored pairwise workload. It classifies moving track segments against circular alert zones and returns both an event category and a score.

## Files added

```text
include/spatial_events/spatial_events.hpp
src/spatial_events/spatial_events_common.cpp
src/spatial_events/spatial_events_cpu.cpp
src/spatial_events/spatial_events_gpu.cu
tests/test_spatial_events.cpp
apps/export_spatial_events.cpp
scripts/plot_spatial_events.py
docs/spatial_events.md
docs/phase_02_spatial_events.md
```

## Files updated

```text
CMakeLists.txt
README.md
src/common/benchmark_registry.cpp
tests/test_registry.cpp
docs/00_project_overview.md
docs/01_benchmarking_methodology.md
docs/03_results_format.md
docs/phase_01_tests.md
```

## Benchmark structure

The benchmark builds deterministic inputs:

- `TrackSegment`: previous/current point plus synthetic speed.
- `CircularZone`: center, radius, severity.

Then it evaluates every track-zone pair:

```text
track_count × zone_count
```

Event categories:

```text
none
enter
exit
stay_inside
cross_through
```

## CPU vs GPU mapping

CPU:

```text
nested loops over tracks and zones
```

GPU:

```text
one CUDA thread per track-zone pair
```

Both paths use the same `evaluate_track_zone()` semantics.

## Validation

The final dense matrices are validated element by element:

- exact event-code agreement,
- tolerance-based score agreement,
- event counts,
- event-type counts,
- checksum and reference checksum,
- maximum absolute / relative score error.

## Running the phase

```bat
configure_ninja_cuda128.bat
execute_all_tests.bat
```

Direct run:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark spatial_events --preset small --repeat 5 --warmup 1
```


## Visualization path

Phase 2.3 also includes a geometry-focused exporter and Python plot utility. The exporter keeps benchmark timing separate from display work while making the generated data inspectable.

```bat
build-cuda-ninja\export_spatial_events.exe --output-dir results\spatial_events_demo
python scripts\plot_spatial_events.py --input-dir results\spatial_events_demo --show
```

The exporter writes track rows, zone rows, event-code and score matrices, a long-form `triggered_events.csv`, and metadata. The plotter turns that export into:

```text
spatial_input_scene.png
spatial_event_overlay.png
spatial_event_matrix.png
spatial_score_matrix.png
```

A custom readable scene can be created with:

```bat
build-cuda-ninja\export_spatial_events.exe --tracks 48 --zones 16 --output-dir results\spatial_events_48x16
python scripts\plot_spatial_events.py --input-dir results\spatial_events_48x16 --max-overlay-events 300
```
