# Spatial Event Detection Benchmark

The `spatial_events` benchmark models a common geospatial / tracking workload:

```text
for every moving track segment
    for every event zone
        classify the spatial relationship
```

The current benchmark uses **track segment × circular zone** pairs. Each pair emits:

- `none`
- `enter`
- `exit`
- `stay_inside`
- `cross_through`

It also emits a small synthetic `score` so the benchmark validates both:

- exact event-category agreement,
- floating-point score agreement.

## Why this is a GPU candidate

The dense pairwise matrix is independent by cell:

```text
event[track_index][zone_index]
```

That maps naturally to:

```text
one CUDA thread = one track-zone pair
```

This resembles practical workloads such as:

- fence / restricted-zone monitoring,
- map-based alerting,
- trajectory-vs-hazard scanning,
- batch spatial rule evaluation,
- prefiltering candidate interactions before a CPU decision layer.

## CPU implementation

The CPU path uses nested loops:

```text
for track in tracks:
    for zone in zones:
        evaluate_track_zone(track, zone)
```

It materializes two row-major dense matrices:

- `event_codes`
- `scores`

Storage layout:

```text
flat_index = track_index * zone_count + zone_index
```

## GPU implementation

The CUDA path launches one thread per dense matrix cell. Each thread reconstructs:

```text
track_index = flat_index / zone_count
zone_index  = flat_index % zone_count
```

and calls the same shared inline evaluator used by the CPU path.

## Pairwise geometry model

For a track segment:

```text
(x0, y0) -> (x1, y1)
```

and a circular zone:

```text
center = (zx, zy)
radius = r
```

classification is:

| Previous point | Current point | Segment intersects zone | Event |
|---|---|---|---|
| outside | outside | no | `none` |
| outside | inside | - | `enter` |
| inside | outside | - | `exit` |
| inside | inside | - | `stay_inside` |
| outside | outside | yes | `cross_through` |

The `cross_through` case uses a closest-point projection from the zone center to the track segment, then compares squared distance to squared radius. This avoids square roots in the pair evaluator.

## Synthetic score

The event score is deliberately simple. It is not meant to be a physical model; it gives us a floating-point value to validate beside the categorical event code.

Conceptually:

```text
score = severity * event_type_weight(speed)
```

Different event classes use different weights. `none` has score `0`.

## Presets

| Preset | Tracks | Zones | Cells |
|---|---:|---:|---:|
| `tiny` | 64 | 32 | 2,048 |
| `small` | 512 | 256 | 131,072 |
| `medium` | 4,096 | 512 | 2,097,152 |
| `large` | 8,192 | 1,024 | 8,388,608 |

## Validation

Validation checks:

- event-code matrix equality,
- score matrix tolerance,
- total event count,
- count per event class,
- score checksum,
- max absolute score error,
- max relative score error.

`--repeat` controls timing only. The checksum describes the final single output matrix, not an accumulation across repeats.

## Expected strengths

- Very clear pairwise parallelism.
- Dense independent cells.
- Mixed categorical + floating-point validation.
- Realistic branch structure without requiring a full spatial-indexing subsystem.

## Expected pitfalls

- Dense pairwise evaluation grows as `tracks × zones`.
- If almost every pair is `none`, a production implementation might prefer spatial indexing before dense evaluation.
- Branching across event classes may reduce warp uniformity.
- Device-to-host transfer becomes meaningful because both event-code and score matrices are materialized.
- This benchmark measures brute-force dense pair evaluation, not an indexed GIS engine.

## Visualization exporter

The benchmark path is designed for timing. For explanation and debugging, the repo also contains a dedicated exporter:

```bat
build-cuda-ninja\export_spatial_events.exe --output-dir results\spatial_events_demo
```

The exporter defaults to an inspectable custom display shape of:

```text
32 tracks × 12 zones
```

It writes:

```text
tracks.csv
zones.csv
event_codes.csv
scores.csv
triggered_events.csv
metadata.json
```

The matrix files use the same row-major convention as the benchmark:

```text
flat_index = track_index * zone_count + zone_index
```

`triggered_events.csv` is a long-form view containing only non-`none` pairs. It is intended for overlays and quick auditing.

Plot the export with:

```bat
python scripts\plot_spatial_events.py --input-dir results\spatial_events_demo --show
```

The plot utility generates:

```text
spatial_input_scene.png
spatial_event_overlay.png
spatial_event_matrix.png
spatial_score_matrix.png
```

The four plots answer different questions:

| Plot | Purpose |
|---|---|
| `spatial_input_scene.png` | Shows the raw track segments and circular zones before classification. |
| `spatial_event_overlay.png` | Highlights triggered track-zone pairs and event classes. |
| `spatial_event_matrix.png` | Shows the dense categorical result matrix. |
| `spatial_score_matrix.png` | Shows the event-score matrix with no-event cells masked. |

For a custom readable display scene:

```bat
build-cuda-ninja\export_spatial_events.exe --tracks 48 --zones 16 --output-dir results\spatial_events_48x16
python scripts\plot_spatial_events.py --input-dir results\spatial_events_48x16 --max-overlay-events 300
```

For the full benchmark-shaped tiny preset:

```bat
build-cuda-ninja\export_spatial_events.exe --preset tiny --output-dir results\spatial_events_tiny_preset
python scripts\plot_spatial_events.py --input-dir results\spatial_events_tiny_preset
```

The default custom scene is preferable for teaching; preset exports are more useful when inspecting matrix density.

## Run

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark spatial_events --preset small --repeat 5 --warmup 1
```

A larger run:

```bat
build-cuda-ninja\gpu_algobench.exe --benchmark spatial_events --preset medium --repeat 3 --warmup 1
```
