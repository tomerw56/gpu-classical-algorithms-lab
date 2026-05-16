#!/usr/bin/env python3
"""Visualize exported spatial-event inputs and outputs.

Expected input directory contents are produced by the C++ helper:

    export_spatial_events --output-dir results/spatial_events_demo

The script creates:

- spatial_input_scene.png
- spatial_event_overlay.png
- spatial_event_matrix.png
- spatial_score_matrix.png

The exporter writes a deliberately small custom display shape by default. Use
benchmark presets only when the matrix view matters more than scene readability.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle


@dataclass(frozen=True)
class Track:
    track_index: int
    x0: float
    y0: float
    x1: float
    y1: float
    speed: float


@dataclass(frozen=True)
class Zone:
    zone_index: int
    x: float
    y: float
    radius: float
    severity: float


@dataclass(frozen=True)
class TriggeredEvent:
    track_index: int
    zone_index: int
    event_code: int
    event_name: str
    score: float


EVENT_LABELS = {
    0: "none",
    1: "enter",
    2: "exit",
    3: "stay_inside",
    4: "cross_through",
}

EVENT_MARKERS = {
    "enter": ">",
    "exit": "<",
    "stay_inside": "o",
    "cross_through": "X",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot exported spatial-event CSV files.")
    parser.add_argument(
        "--input-dir",
        type=Path,
        required=True,
        help="Directory containing tracks.csv, zones.csv, and spatial event matrices.",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Directory for PNG files. Default: <input-dir>/plots.",
    )
    parser.add_argument(
        "--max-overlay-events",
        type=int,
        default=400,
        help="Maximum triggered track-zone pairs drawn on the overlay plot.",
    )
    parser.add_argument(
        "--max-matrix-dim",
        type=int,
        default=240,
        help="Maximum rows/columns used in matrix heatmaps after regular downsampling.",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Open plot windows in addition to writing PNG files.",
    )
    return parser.parse_args()


def load_metadata(input_dir: Path) -> dict[str, object]:
    path = input_dir / "metadata.json"
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_tracks(path: Path) -> list[Track]:
    if not path.exists():
        raise FileNotFoundError(f"required track file is missing: {path}")

    tracks: list[Track] = []
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            tracks.append(
                Track(
                    track_index=int(row["track_index"]),
                    x0=float(row["x0"]),
                    y0=float(row["y0"]),
                    x1=float(row["x1"]),
                    y1=float(row["y1"]),
                    speed=float(row["speed"]),
                )
            )
    return tracks


def load_zones(path: Path) -> list[Zone]:
    if not path.exists():
        raise FileNotFoundError(f"required zone file is missing: {path}")

    zones: list[Zone] = []
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            zones.append(
                Zone(
                    zone_index=int(row["zone_index"]),
                    x=float(row["x"]),
                    y=float(row["y"]),
                    radius=float(row["radius"]),
                    severity=float(row["severity"]),
                )
            )
    return zones


def load_events(path: Path) -> list[TriggeredEvent]:
    if not path.exists():
        return []

    events: list[TriggeredEvent] = []
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            events.append(
                TriggeredEvent(
                    track_index=int(row["track_index"]),
                    zone_index=int(row["zone_index"]),
                    event_code=int(row["event_code"]),
                    event_name=row["event_name"],
                    score=float(row["score"]),
                )
            )
    return events


def load_matrix(path: Path, dtype: type[np.floating] | type[np.integer]) -> np.ndarray:
    if not path.exists():
        raise FileNotFoundError(f"required matrix file is missing: {path}")

    matrix = np.loadtxt(path, delimiter=",", dtype=dtype)
    if matrix.ndim == 1:
        matrix = matrix.reshape(1, -1)
    return matrix


def ensure_output_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def scene_bounds(tracks: list[Track], zones: list[Zone]) -> tuple[float, float, float, float]:
    x_values: list[float] = []
    y_values: list[float] = []

    for track in tracks:
        x_values.extend([track.x0, track.x1])
        y_values.extend([track.y0, track.y1])

    for zone in zones:
        x_values.extend([zone.x - zone.radius, zone.x + zone.radius])
        y_values.extend([zone.y - zone.radius, zone.y + zone.radius])

    if not x_values or not y_values:
        return 0.0, 1.0, 0.0, 1.0

    min_x, max_x = min(x_values), max(x_values)
    min_y, max_y = min(y_values), max(y_values)
    pad_x = max(1.0, (max_x - min_x) * 0.05)
    pad_y = max(1.0, (max_y - min_y) * 0.05)
    return min_x - pad_x, max_x + pad_x, min_y - pad_y, max_y + pad_y


def annotate_if_small(ax: plt.Axes, tracks: list[Track], zones: list[Zone]) -> None:
    if len(tracks) <= 40:
        for track in tracks:
            ax.text(track.x1, track.y1, f"T{track.track_index}", fontsize=7)

    if len(zones) <= 30:
        for zone in zones:
            ax.text(zone.x, zone.y, f"Z{zone.zone_index}", fontsize=8, ha="center", va="center")


def draw_base_scene(ax: plt.Axes, tracks: list[Track], zones: list[Zone], *, faint_tracks: bool) -> None:
    for zone in zones:
        circle = Circle((zone.x, zone.y), zone.radius, fill=False, linewidth=1.3, alpha=0.7)
        ax.add_patch(circle)
        ax.scatter([zone.x], [zone.y], marker="+", s=20)

    for track in tracks:
        alpha = 0.25 if faint_tracks else 0.75
        linewidth = 0.8 if faint_tracks else 1.1
        ax.plot([track.x0, track.x1], [track.y0, track.y1], linewidth=linewidth, alpha=alpha)
        if not faint_tracks:
            ax.scatter([track.x0], [track.y0], marker=".", s=12)
            ax.scatter([track.x1], [track.y1], marker="^", s=18)

    ax.set_aspect("equal", adjustable="box")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    min_x, max_x, min_y, max_y = scene_bounds(tracks, zones)
    ax.set_xlim(min_x, max_x)
    ax.set_ylim(min_y, max_y)
    ax.grid(alpha=0.25)


def title_suffix(metadata: dict[str, object], tracks: list[Track], zones: list[Zone]) -> str:
    track_count = metadata.get("track_count", len(tracks))
    zone_count = metadata.get("zone_count", len(zones))
    shape_source = metadata.get("shape_source")
    preset = metadata.get("preset")

    if shape_source == "preset":
        return f" ({preset}: {track_count} tracks × {zone_count} zones)"
    return f" ({track_count} tracks × {zone_count} zones)"


def save_input_scene(tracks: list[Track], zones: list[Zone], metadata: dict[str, object], out_dir: Path) -> Path:
    fig, ax = plt.subplots(figsize=(12, 8))
    draw_base_scene(ax, tracks, zones, faint_tracks=False)
    annotate_if_small(ax, tracks, zones)
    ax.set_title(f"Spatial event input scene{title_suffix(metadata, tracks, zones)}")
    fig.tight_layout()

    output = out_dir / "spatial_input_scene.png"
    fig.savefig(output, dpi=180)
    return output


def best_event_per_track(events: Iterable[TriggeredEvent]) -> dict[int, TriggeredEvent]:
    best: dict[int, TriggeredEvent] = {}
    for event in events:
        current = best.get(event.track_index)
        if current is None or event.score > current.score:
            best[event.track_index] = event
    return best


def save_event_overlay(
    tracks: list[Track],
    zones: list[Zone],
    events: list[TriggeredEvent],
    metadata: dict[str, object],
    out_dir: Path,
    max_overlay_events: int,
) -> Path:
    if max_overlay_events <= 0:
        raise ValueError("--max-overlay-events must be positive")

    tracks_by_id = {track.track_index: track for track in tracks}
    zones_by_id = {zone.zone_index: zone for zone in zones}
    selected_events = sorted(events, key=lambda event: event.score, reverse=True)[:max_overlay_events]

    fig, ax = plt.subplots(figsize=(12, 8))
    draw_base_scene(ax, tracks, zones, faint_tracks=True)

    legend_drawn: set[str] = set()
    for event in selected_events:
        track = tracks_by_id.get(event.track_index)
        zone = zones_by_id.get(event.zone_index)
        if track is None or zone is None:
            continue

        label = event.event_name if event.event_name not in legend_drawn else None
        ax.plot([track.x0, track.x1], [track.y0, track.y1], linewidth=1.8, alpha=0.95, label=label)
        marker = EVENT_MARKERS.get(event.event_name, "o")
        ax.scatter([zone.x], [zone.y], marker=marker, s=36, alpha=0.9)
        legend_drawn.add(event.event_name)

    ax.set_title(
        f"Spatial event overlay{title_suffix(metadata, tracks, zones)}\n"
        f"showing {len(selected_events)} of {len(events)} triggered track-zone pairs"
    )
    if legend_drawn:
        ax.legend(loc="best")
    fig.tight_layout()

    output = out_dir / "spatial_event_overlay.png"
    fig.savefig(output, dpi=180)
    return output


def regular_downsample(matrix: np.ndarray, max_dim: int) -> tuple[np.ndarray, int, int]:
    if max_dim <= 0:
        raise ValueError("--max-matrix-dim must be positive")

    rows, cols = matrix.shape
    row_stride = max(1, math.ceil(rows / max_dim))
    col_stride = max(1, math.ceil(cols / max_dim))
    return matrix[::row_stride, ::col_stride], row_stride, col_stride


def save_event_matrix(event_codes: np.ndarray, metadata: dict[str, object], out_dir: Path, max_matrix_dim: int) -> Path:
    matrix, row_stride, col_stride = regular_downsample(event_codes, max_matrix_dim)

    fig, ax = plt.subplots(figsize=(11, 7))
    image = ax.imshow(matrix, aspect="auto", interpolation="nearest", vmin=0, vmax=4)
    ax.set_title(
        f"Spatial event-code matrix\n"
        f"rows x{row_stride}, columns x{col_stride}; 0 none, 1 enter, 2 exit, 3 stay, 4 cross"
    )
    ax.set_xlabel("zone index")
    ax.set_ylabel("track index")
    colorbar = fig.colorbar(image, ax=ax, ticks=[0, 1, 2, 3, 4])
    colorbar.ax.set_yticklabels([EVENT_LABELS[i] for i in range(5)])
    colorbar.set_label("event class")
    fig.tight_layout()

    output = out_dir / "spatial_event_matrix.png"
    fig.savefig(output, dpi=180)
    return output


def save_score_matrix(scores: np.ndarray, metadata: dict[str, object], out_dir: Path, max_matrix_dim: int) -> Path:
    matrix, row_stride, col_stride = regular_downsample(scores, max_matrix_dim)
    masked = np.ma.masked_where(matrix <= 0.0, matrix)

    fig, ax = plt.subplots(figsize=(11, 7))
    image = ax.imshow(masked, aspect="auto", interpolation="nearest")
    ax.set_title(
        f"Spatial event score matrix\n"
        f"rows x{row_stride}, columns x{col_stride}; none cells are masked"
    )
    ax.set_xlabel("zone index")
    ax.set_ylabel("track index")
    colorbar = fig.colorbar(image, ax=ax)
    colorbar.set_label("event score")
    fig.tight_layout()

    output = out_dir / "spatial_score_matrix.png"
    fig.savefig(output, dpi=180)
    return output


def print_event_summary(events: list[TriggeredEvent], metadata: dict[str, object]) -> None:
    counts: dict[str, int] = defaultdict(int)
    for event in events:
        counts[event.event_name] += 1

    print("Spatial-event visualization summary:")
    print(f"  tracks: {metadata.get('track_count', 'unknown')}")
    print(f"  zones: {metadata.get('zone_count', 'unknown')}")
    print(f"  triggered pairs: {len(events)}")
    for event_name in ("enter", "exit", "stay_inside", "cross_through"):
        print(f"  {event_name}: {counts.get(event_name, 0)}")


def main() -> int:
    args = parse_args()
    input_dir = args.input_dir
    out_dir = args.out_dir or (input_dir / "plots")

    tracks = load_tracks(input_dir / "tracks.csv")
    zones = load_zones(input_dir / "zones.csv")
    events = load_events(input_dir / "triggered_events.csv")
    event_codes = load_matrix(input_dir / "event_codes.csv", np.int64)
    scores = load_matrix(input_dir / "scores.csv", np.float64)
    metadata = load_metadata(input_dir)

    if event_codes.shape != scores.shape:
        raise ValueError(f"shape mismatch: event_codes={event_codes.shape}, scores={scores.shape}")

    expected_shape = (len(tracks), len(zones))
    if event_codes.shape != expected_shape:
        raise ValueError(f"matrix shape {event_codes.shape} does not match tracks/zones {expected_shape}")

    ensure_output_dir(out_dir)
    outputs = [
        save_input_scene(tracks, zones, metadata, out_dir),
        save_event_overlay(tracks, zones, events, metadata, out_dir, args.max_overlay_events),
        save_event_matrix(event_codes, metadata, out_dir, args.max_matrix_dim),
        save_score_matrix(scores, metadata, out_dir, args.max_matrix_dim),
    ]

    print_event_summary(events, metadata)
    print("Generated spatial-event visualization files:")
    for output in outputs:
        print(f"  {output}")

    if args.show:
        plt.show()
    else:
        plt.close("all")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
