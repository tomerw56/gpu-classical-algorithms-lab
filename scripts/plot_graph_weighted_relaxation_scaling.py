#!/usr/bin/env python3
"""Plot graph_weighted_relaxation shape x scale sweep results.

The benchmark reports CPU Dijkstra plus two GPU variants:

* `gpu` - global Bellman-Ford-style edge scan.
* `gpu-frontier` - active-frontier relaxation.
"""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

GRAPH_KIND_ORDER = ["chain", "grid", "layered", "random"]
GRAPH_KIND_LABEL = {
    "chain": "chain",
    "grid": "grid",
    "layered": "layered",
    "random": "random",
}


def normalize_kind(kind: str) -> str:
    if kind in {"chains", "chain_components"}:
        return "chain"
    if kind in {"grids", "grid_components"}:
        return "grid"
    if kind in {"random_sparse", "random_clusters"}:
        return "random"
    return kind


def optional_int(value: object) -> int | None:
    if value is None or value == "":
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def optional_float(value: object) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


@dataclass(frozen=True)
class Sample:
    variant: str
    label: str
    graph_kind: str
    shape: str
    nodes: int
    edges: int
    repeat: int
    per_run_ms: float
    correct: bool
    iterations: int | None
    max_iterations: int | None
    converged: bool | None
    reached_count: int | None
    max_finite_distance: int | None
    mean_out_degree: float | None
    edge_iterations: int | None
    active_frontier_node_visits: int | None
    max_frontier_size: int | None


@dataclass(frozen=True)
class SummaryRow:
    graph_kind: str
    label: str
    shape: str
    nodes: int
    edges: int
    cpu_ms_per_run: float | None
    gpu_global_ms_per_run: float | None
    gpu_frontier_ms_per_run: float | None
    speedup_cpu_over_gpu_global: float | None
    speedup_cpu_over_gpu_frontier: float | None
    gpu_global_to_frontier_speedup: float | None
    gpu_global_iterations: int | None
    gpu_frontier_iterations: int | None
    gpu_global_converged: bool | None
    gpu_frontier_converged: bool | None
    gpu_global_edge_iterations: float | None
    gpu_frontier_active_node_visits: float | None
    gpu_frontier_edge_work_estimate: float | None
    gpu_frontier_max_frontier_size: int | None
    reached_count: int | None
    max_finite_distance: int | None
    mean_out_degree: float | None


def load_samples(path: Path) -> list[Sample]:
    samples: list[Sample] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_number, raw in enumerate(handle, start=1):
            line = raw.strip()
            if not line:
                continue
            row = json.loads(line)
            if row.get("benchmark") != "graph_weighted_relaxation":
                continue
            variant = str(row.get("variant", ""))
            if variant not in {"cpu", "gpu", "gpu-frontier"}:
                continue
            metadata = row.get("metadata") or {}
            input_size = row.get("input_size") or {}
            try:
                nodes = int(input_size["nodes"])
                edges = int(input_size["edges"])
                repeat = int(row.get("repeat", 1))
                total_ms = float(row.get("total_ms", 0.0))
            except (KeyError, TypeError, ValueError) as exc:
                raise ValueError(f"Malformed graph_weighted_relaxation row on line {line_number}: {row}") from exc
            if repeat <= 0:
                raise ValueError(f"Invalid repeat count on line {line_number}: {repeat}")
            converged_raw = metadata.get("converged")
            converged = None if converged_raw is None else str(converged_raw).lower() == "true"
            samples.append(
                Sample(
                    variant=variant,
                    label=str(row.get("preset", "")),
                    graph_kind=normalize_kind(str(metadata.get("graph_kind", ""))),
                    shape=str(metadata.get("shape", "")),
                    nodes=nodes,
                    edges=edges,
                    repeat=repeat,
                    per_run_ms=total_ms / repeat,
                    correct=bool(row.get("correct", False)),
                    iterations=optional_int(metadata.get("iterations")),
                    max_iterations=optional_int(metadata.get("max_iterations")),
                    converged=converged,
                    reached_count=optional_int(metadata.get("reached_count")),
                    max_finite_distance=optional_int(metadata.get("max_finite_distance")),
                    mean_out_degree=optional_float(metadata.get("mean_out_degree")),
                    edge_iterations=optional_int(metadata.get("edge_iterations")),
                    active_frontier_node_visits=optional_int(metadata.get("active_frontier_node_visits")),
                    max_frontier_size=optional_int(metadata.get("max_frontier_size")),
                )
            )
    return samples


def representative(samples: list[Sample]) -> Sample | None:
    valid = [s for s in samples if s.correct]
    return valid[0] if valid else (samples[0] if samples else None)


def mean_per_run(samples: list[Sample]) -> float | None:
    valid = [s for s in samples if s.correct]
    return sum(s.per_run_ms for s in valid) / len(valid) if valid else None


def summarize(samples: Iterable[Sample]) -> list[SummaryRow]:
    groups: dict[tuple[str, str, int, int], dict[str, list[Sample]]] = defaultdict(lambda: defaultdict(list))
    for sample in samples:
        if sample.graph_kind in GRAPH_KIND_ORDER:
            groups[(sample.graph_kind, sample.label, sample.nodes, sample.edges)][sample.variant].append(sample)

    rows: list[SummaryRow] = []
    for (kind, label, nodes, edges), variants in groups.items():
        cpu = variants.get("cpu", [])
        global_gpu = variants.get("gpu", [])
        frontier_gpu = variants.get("gpu-frontier", [])
        cpu_ms = mean_per_run(cpu)
        global_ms = mean_per_run(global_gpu)
        frontier_ms = mean_per_run(frontier_gpu)
        base = representative(frontier_gpu) or representative(global_gpu) or representative(cpu)
        if base is None:
            continue
        global_rep = representative(global_gpu)
        frontier_rep = representative(frontier_gpu)
        global_speedup = cpu_ms / global_ms if cpu_ms is not None and global_ms not in {None, 0.0} else None
        frontier_speedup = cpu_ms / frontier_ms if cpu_ms is not None and frontier_ms not in {None, 0.0} else None
        global_to_frontier = global_ms / frontier_ms if global_ms is not None and frontier_ms not in {None, 0.0} else None
        frontier_edge_work = None
        if frontier_rep and frontier_rep.active_frontier_node_visits is not None and base.mean_out_degree is not None:
            frontier_edge_work = frontier_rep.active_frontier_node_visits * base.mean_out_degree
        rows.append(
            SummaryRow(
                graph_kind=kind,
                label=label,
                shape=base.shape,
                nodes=nodes,
                edges=edges,
                cpu_ms_per_run=cpu_ms,
                gpu_global_ms_per_run=global_ms,
                gpu_frontier_ms_per_run=frontier_ms,
                speedup_cpu_over_gpu_global=global_speedup,
                speedup_cpu_over_gpu_frontier=frontier_speedup,
                gpu_global_to_frontier_speedup=global_to_frontier,
                gpu_global_iterations=global_rep.iterations if global_rep else None,
                gpu_frontier_iterations=frontier_rep.iterations if frontier_rep else None,
                gpu_global_converged=global_rep.converged if global_rep else None,
                gpu_frontier_converged=frontier_rep.converged if frontier_rep else None,
                gpu_global_edge_iterations=global_rep.edge_iterations if global_rep else None,
                gpu_frontier_active_node_visits=frontier_rep.active_frontier_node_visits if frontier_rep else None,
                gpu_frontier_edge_work_estimate=frontier_edge_work,
                gpu_frontier_max_frontier_size=frontier_rep.max_frontier_size if frontier_rep else None,
                reached_count=base.reached_count,
                max_finite_distance=base.max_finite_distance,
                mean_out_degree=base.mean_out_degree,
            )
        )

    def sort_key(row: SummaryRow) -> tuple[int, int, str]:
        order = GRAPH_KIND_ORDER.index(row.graph_kind) if row.graph_kind in GRAPH_KIND_ORDER else len(GRAPH_KIND_ORDER)
        return (order, row.nodes, row.label)

    return sorted(rows, key=sort_key)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot graph_weighted_relaxation scaling sweep results.")
    parser.add_argument("--jsonl", required=True, help="Input JSONL file generated by the weighted sweep runner.")
    parser.add_argument("--output-dir", default="results/graph_weighted_relaxation_shape_scale_plots")
    parser.add_argument("--show", action="store_true")
    return parser.parse_args()


def write_summary_csv(rows: list[SummaryRow], output_dir: Path) -> Path:
    path = output_dir / "graph_weighted_relaxation_shape_scaling_summary.csv"
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "graph_kind", "label", "nodes", "edges", "shape",
            "cpu_ms_per_run", "gpu_global_ms_per_run", "gpu_frontier_ms_per_run",
            "speedup_cpu_over_gpu_global", "speedup_cpu_over_gpu_frontier", "gpu_global_to_frontier_speedup",
            "gpu_global_iterations", "gpu_frontier_iterations", "gpu_global_converged", "gpu_frontier_converged",
            "gpu_global_edge_iterations", "gpu_frontier_active_node_visits", "gpu_frontier_edge_work_estimate",
            "gpu_frontier_max_frontier_size", "reached_count", "max_finite_distance", "mean_out_degree",
        ])
        for row in rows:
            writer.writerow([
                row.graph_kind, row.label, row.nodes, row.edges, row.shape,
                row.cpu_ms_per_run, row.gpu_global_ms_per_run, row.gpu_frontier_ms_per_run,
                row.speedup_cpu_over_gpu_global, row.speedup_cpu_over_gpu_frontier, row.gpu_global_to_frontier_speedup,
                row.gpu_global_iterations, row.gpu_frontier_iterations, row.gpu_global_converged, row.gpu_frontier_converged,
                row.gpu_global_edge_iterations, row.gpu_frontier_active_node_visits, row.gpu_frontier_edge_work_estimate,
                row.gpu_frontier_max_frontier_size, row.reached_count, row.max_finite_distance, row.mean_out_degree,
            ])
    return path


def grouped(rows: list[SummaryRow]) -> dict[str, list[SummaryRow]]:
    out: dict[str, list[SummaryRow]] = {kind: [] for kind in GRAPH_KIND_ORDER}
    for row in rows:
        if row.graph_kind in out:
            out[row.graph_kind].append(row)
    for values in out.values():
        values.sort(key=lambda r: r.nodes)
    return out


def plot_lines(rows: list[SummaryRow], output_dir: Path, filename: str, title: str, ylabel: str,
               y_getter: Callable[[SummaryRow], float | None], *, log_y: bool = False,
               horizontal_one: bool = False) -> Path | None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"matplotlib is not available: {exc}")
        return None
    plotted = False
    fig, ax = plt.subplots(figsize=(10, 6))
    for kind, values in grouped(rows).items():
        values = [row for row in values if y_getter(row) is not None]
        if not values:
            continue
        plotted = True
        ax.plot([row.nodes for row in values], [float(y_getter(row)) for row in values], marker="o", label=GRAPH_KIND_LABEL[kind])  # type: ignore[arg-type]
    if not plotted:
        print(f"Skipping {filename}: no rows had the requested metric.")
        plt.close(fig)
        return None
    ax.set_xscale("log")
    if log_y:
        ax.set_yscale("log")
    if horizontal_one:
        ax.axhline(1.0, linestyle="--", linewidth=1)
    ax.set_xlabel("nodes")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    path = output_dir / filename
    fig.savefig(path, dpi=150)
    return path


def plot_timing(rows: list[SummaryRow], output_dir: Path) -> Path | None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"matplotlib is not available: {exc}")
        return None
    fig, ax = plt.subplots(figsize=(12, 7))
    plotted = False
    for kind, values in grouped(rows).items():
        if any(r.cpu_ms_per_run is not None for r in values):
            plotted = True
            ax.plot([r.nodes for r in values if r.cpu_ms_per_run is not None], [r.cpu_ms_per_run for r in values if r.cpu_ms_per_run is not None], marker="o", linestyle="-", label=f"{GRAPH_KIND_LABEL[kind]} CPU Dijkstra")
        if any(r.gpu_global_ms_per_run is not None for r in values):
            plotted = True
            ax.plot([r.nodes for r in values if r.gpu_global_ms_per_run is not None], [r.gpu_global_ms_per_run for r in values if r.gpu_global_ms_per_run is not None], marker="s", linestyle="--", label=f"{GRAPH_KIND_LABEL[kind]} GPU global")
        if any(r.gpu_frontier_ms_per_run is not None for r in values):
            plotted = True
            ax.plot([r.nodes for r in values if r.gpu_frontier_ms_per_run is not None], [r.gpu_frontier_ms_per_run for r in values if r.gpu_frontier_ms_per_run is not None], marker="^", linestyle=":", label=f"{GRAPH_KIND_LABEL[kind]} GPU frontier")
    if not plotted:
        print("Skipping timing plot: no timing rows found.")
        plt.close(fig)
        return None
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("nodes")
    ax.set_ylabel("average ms per weighted shortest-path run")
    ax.set_title("Weighted graph CPU / GPU-global / GPU-frontier timing")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(ncol=2, fontsize="small")
    fig.tight_layout()
    path = output_dir / "graph_wr_shape_scaling_times.png"
    fig.savefig(path, dpi=150)
    return path


def plot_scatter(rows: list[SummaryRow], output_dir: Path, filename: str, title: str, xlabel: str,
                 x_getter: Callable[[SummaryRow], float | None], ylabel: str,
                 y_getter: Callable[[SummaryRow], float | None], *, log_x=False, log_y=False,
                 horizontal_one=False) -> Path | None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"matplotlib is not available: {exc}")
        return None
    fig, ax = plt.subplots(figsize=(9, 6))
    plotted = False
    for kind, values in grouped(rows).items():
        values = [row for row in values if x_getter(row) is not None and y_getter(row) is not None]
        if not values:
            continue
        plotted = True
        x = [float(x_getter(r)) for r in values]  # type: ignore[arg-type]
        y = [float(y_getter(r)) for r in values]  # type: ignore[arg-type]
        ax.scatter(x, y, label=GRAPH_KIND_LABEL[kind])
        for row, xi, yi in zip(values, x, y):
            ax.annotate(str(row.nodes), (xi, yi), fontsize=7, alpha=0.7)
    if not plotted:
        print(f"Skipping {filename}: insufficient x/y metrics.")
        plt.close(fig)
        return None
    if log_x:
        ax.set_xscale("log")
    if log_y:
        ax.set_yscale("log")
    if horizontal_one:
        ax.axhline(1.0, linestyle="--", linewidth=1)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    path = output_dir / filename
    fig.savefig(path, dpi=150)
    return path


def print_crossover(rows: list[SummaryRow]) -> None:
    print("\nWeighted-relaxation crossover summary:")
    for kind in GRAPH_KIND_ORDER:
        candidates = grouped(rows)[kind]
        global_cross = next((r for r in candidates if r.speedup_cpu_over_gpu_global and r.speedup_cpu_over_gpu_global > 1.0), None)
        frontier_cross = next((r for r in candidates if r.speedup_cpu_over_gpu_frontier and r.speedup_cpu_over_gpu_frontier > 1.0), None)
        if global_cross is None:
            print(f"  {GRAPH_KIND_LABEL[kind]} / global: no GPU crossover observed")
        else:
            print(f"  {GRAPH_KIND_LABEL[kind]} / global: first crossover at {global_cross.label}, speedup={global_cross.speedup_cpu_over_gpu_global:.3f}x")
        if frontier_cross is None:
            print(f"  {GRAPH_KIND_LABEL[kind]} / frontier: no GPU crossover observed")
        else:
            print(f"  {GRAPH_KIND_LABEL[kind]} / frontier: first crossover at {frontier_cross.label}, speedup={frontier_cross.speedup_cpu_over_gpu_frontier:.3f}x")


def main() -> int:
    args = parse_args()
    jsonl = Path(args.jsonl)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    samples = load_samples(jsonl)
    rows = summarize(samples)
    if not rows:
        raise SystemExit(f"No graph_weighted_relaxation rows found in {jsonl}")

    csv_path = write_summary_csv(rows, output_dir)
    print(f"Wrote summary CSV: {csv_path}")

    paths: list[Path] = []
    for maybe in [
        plot_timing(rows, output_dir),
        plot_lines(rows, output_dir, "graph_wr_global_speedup.png", "Weighted graph CPU/GPU-global speedup", "CPU ms/run ÷ GPU-global ms/run", lambda r: r.speedup_cpu_over_gpu_global, log_y=True, horizontal_one=True),
        plot_lines(rows, output_dir, "graph_wr_frontier_speedup.png", "Weighted graph CPU/GPU-frontier speedup", "CPU ms/run ÷ GPU-frontier ms/run", lambda r: r.speedup_cpu_over_gpu_frontier, log_y=True, horizontal_one=True),
        plot_lines(rows, output_dir, "graph_wr_global_to_frontier_speedup.png", "GPU-global vs GPU-frontier weighted relaxation", "GPU-global ms/run ÷ GPU-frontier ms/run", lambda r: r.gpu_global_to_frontier_speedup, log_y=True, horizontal_one=True),
        plot_lines(rows, output_dir, "graph_wr_global_iterations.png", "GPU-global weighted-relaxation iterations", "global-scan iterations", lambda r: float(r.gpu_global_iterations) if r.gpu_global_iterations is not None else None, log_y=True),
        plot_lines(rows, output_dir, "graph_wr_frontier_iterations.png", "GPU-frontier weighted-relaxation iterations", "frontier iterations", lambda r: float(r.gpu_frontier_iterations) if r.gpu_frontier_iterations is not None else None, log_y=True),
        plot_lines(rows, output_dir, "graph_wr_frontier_max_size.png", "GPU-frontier maximum active frontier size", "max frontier size", lambda r: float(r.gpu_frontier_max_frontier_size) if r.gpu_frontier_max_frontier_size is not None else None, log_y=True),
        plot_scatter(rows, output_dir, "graph_wr_global_speedup_vs_edge_iterations.png", "GPU-global speedup vs edge-iterations", "edges × global iterations", lambda r: r.gpu_global_edge_iterations, "CPU/GPU-global speedup", lambda r: r.speedup_cpu_over_gpu_global, log_x=True, log_y=True, horizontal_one=True),
        plot_scatter(rows, output_dir, "graph_wr_frontier_speedup_vs_work_estimate.png", "GPU-frontier speedup vs estimated active edge work", "active node visits × mean out-degree", lambda r: r.gpu_frontier_edge_work_estimate, "CPU/GPU-frontier speedup", lambda r: r.speedup_cpu_over_gpu_frontier, log_x=True, log_y=True, horizontal_one=True),
    ]:
        if maybe:
            paths.append(maybe)

    print_crossover(rows)
    for path in paths:
        print(f"Wrote plot: {path}")

    if args.show:
        try:
            import matplotlib.pyplot as plt
            plt.show()
        except ImportError:
            pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
