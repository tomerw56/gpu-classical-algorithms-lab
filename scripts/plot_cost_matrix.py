#!/usr/bin/env python3
"""Visualize exported cost-matrix data.

Expected input directory contents are produced by the C++ helper:

    export_cost_matrix --preset tiny --output-dir results/cost_matrix_tiny

The script creates:

- cost_matrix_heatmap.png
- feasibility_mask.png
- cost_matrix_surface.png

The 3D surface is downsampled automatically for dense matrices so it remains
inspectable and does not spend most of its time rendering millions of polygons.
For continuity, infeasible cells are rendered at a zero-cost floor in the surface
view only; the heatmap keeps them masked.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot exported cost-matrix CSV files.")
    parser.add_argument(
        "--input-dir",
        type=Path,
        required=True,
        help="Directory containing costs.csv and feasible.csv from export_cost_matrix.",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Directory for PNG files. Default: <input-dir>/plots.",
    )
    parser.add_argument(
        "--max-surface-dim",
        type=int,
        default=180,
        help="Maximum rows/columns used in the 3D surface after regular downsampling.",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Open plot windows in addition to writing PNG files.",
    )
    return parser.parse_args()


def load_matrix(path: Path, dtype: type[np.floating] | type[np.integer]) -> np.ndarray:
    if not path.exists():
        raise FileNotFoundError(f"required matrix file is missing: {path}")

    matrix = np.loadtxt(path, delimiter=",", dtype=dtype)
    if matrix.ndim == 1:
        matrix = matrix.reshape(1, -1)
    return matrix


def load_metadata(input_dir: Path) -> dict[str, object]:
    metadata_path = input_dir / "metadata.json"
    if not metadata_path.exists():
        return {}

    with metadata_path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def ensure_output_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def masked_costs(costs: np.ndarray, feasible: np.ndarray) -> np.ma.MaskedArray:
    if costs.shape != feasible.shape:
        raise ValueError(f"shape mismatch: costs={costs.shape}, feasible={feasible.shape}")

    # The exporter writes the raw benchmark cost matrix, including the numeric
    # infeasible sentinel. For plots, masking infeasible cells is more useful than
    # letting the sentinel dominate every color scale.
    return np.ma.masked_where(feasible <= 0, costs)


def save_cost_heatmap(cost_values: np.ma.MaskedArray, out_dir: Path, title_suffix: str) -> Path:
    fig, ax = plt.subplots(figsize=(11, 7))
    image = ax.imshow(cost_values, aspect="auto", interpolation="nearest")
    ax.set_title(f"Cost matrix heatmap{title_suffix}")
    ax.set_xlabel("resource index")
    ax.set_ylabel("task index")
    colorbar = fig.colorbar(image, ax=ax)
    colorbar.set_label("feasible pair cost")
    fig.tight_layout()

    output = out_dir / "cost_matrix_heatmap.png"
    fig.savefig(output, dpi=180)
    return output


def save_feasibility_mask(feasible: np.ndarray, out_dir: Path, title_suffix: str) -> Path:
    fig, ax = plt.subplots(figsize=(11, 7))
    image = ax.imshow(feasible, aspect="auto", interpolation="nearest")
    ax.set_title(f"Feasibility mask{title_suffix}")
    ax.set_xlabel("resource index")
    ax.set_ylabel("task index")
    colorbar = fig.colorbar(image, ax=ax)
    colorbar.set_label("0 = infeasible, 1 = feasible")
    fig.tight_layout()

    output = out_dir / "feasibility_mask.png"
    fig.savefig(output, dpi=180)
    return output


def regular_downsample(matrix: np.ma.MaskedArray, max_dim: int) -> tuple[np.ma.MaskedArray, int, int]:
    if max_dim <= 0:
        raise ValueError("--max-surface-dim must be positive")

    rows, cols = matrix.shape
    row_stride = max(1, math.ceil(rows / max_dim))
    col_stride = max(1, math.ceil(cols / max_dim))
    return matrix[::row_stride, ::col_stride], row_stride, col_stride


def save_surface(cost_values: np.ma.MaskedArray, out_dir: Path, title_suffix: str, max_surface_dim: int) -> Path:
    surface_values, row_stride, col_stride = regular_downsample(cost_values, max_surface_dim)

    # plot_surface needs contiguous neighboring cells to form visible quads.
    # A sparse NaN-masked matrix can therefore look almost blank. For the surface
    # view only, place infeasible cells on a zero-cost floor so the cost peaks remain
    # visible while the dedicated heatmap still preserves the proper mask.
    surface_z = surface_values.filled(0.0)

    rows, cols = surface_z.shape
    resource_axis = np.arange(cols, dtype=np.float64) * col_stride
    task_axis = np.arange(rows, dtype=np.float64) * row_stride
    x_grid, y_grid = np.meshgrid(resource_axis, task_axis)

    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(projection="3d")
    surface = ax.plot_surface(
        x_grid,
        y_grid,
        surface_z,
        cmap="viridis",
        linewidth=0,
        antialiased=True,
    )
    ax.set_title(
        f"Cost matrix surface{title_suffix}\n"
        f"surface stride: rows x{row_stride}, columns x{col_stride}"
    )
    ax.set_xlabel("resource index")
    ax.set_ylabel("task index")
    ax.set_zlabel("feasible pair cost")
    fig.colorbar(surface, ax=ax, shrink=0.65, pad=0.1, label="cost")
    fig.tight_layout()

    output = out_dir / "cost_matrix_surface.png"
    fig.savefig(output, dpi=180)
    return output


def format_title_suffix(metadata: dict[str, object], costs: np.ndarray) -> str:
    tasks = metadata.get("task_count", costs.shape[0])
    resources = metadata.get("resource_count", costs.shape[1])
    preset = metadata.get("preset")

    if preset is None:
        return f" ({tasks} tasks × {resources} resources)"
    return f" ({preset}: {tasks} tasks × {resources} resources)"


def main() -> int:
    args = parse_args()
    input_dir = args.input_dir
    out_dir = args.out_dir or (input_dir / "plots")

    costs = load_matrix(input_dir / "costs.csv", np.float64)
    feasible = load_matrix(input_dir / "feasible.csv", np.int64)
    metadata = load_metadata(input_dir)
    plotted_costs = masked_costs(costs, feasible)
    title_suffix = format_title_suffix(metadata, costs)

    ensure_output_dir(out_dir)
    outputs = [
        save_cost_heatmap(plotted_costs, out_dir, title_suffix),
        save_feasibility_mask(feasible, out_dir, title_suffix),
        save_surface(plotted_costs, out_dir, title_suffix, args.max_surface_dim),
    ]

    print("Generated cost-matrix visualization files:")
    for output in outputs:
        print(f"  {output}")

    if args.show:
        plt.show()
    else:
        plt.close("all")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
