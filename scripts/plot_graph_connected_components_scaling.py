#!/usr/bin/env python3
"""Plot graph_connected_components shape x scale sweep results.

The sweep can contain three variants for every size point:

    cpu
    gpu              (original naive GPU label propagation)
    gpu-non-naive    (edge-root hooking + full pointer jumping)

The benchmark executable reports total milliseconds across `repeat` measured
runs. This script normalizes every row to average milliseconds per connected-
components run (`total_ms / repeat`) before computing speedups.
"""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

GRAPH_KIND_ORDER = ["chains", "grid_components", "random_clusters", "mixed"]
GRAPH_KIND_LABEL = {
    "chains": "chains",
    "grid_components": "grid components",
    "random_clusters": "random clusters",
    "mixed": "mixed",
}


def normalize_kind(kind: str) -> str:
    if kind in {"chain_components", "chain"}:
        return "chains"
    if kind in {"grids", "grid"}:
        return "grid_components"
    if kind in {"random", "random_sparse"}:
        return "random_clusters"
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
    component_count: int | None
    largest_component_size: int | None
    mean_out_degree: float | None


@dataclass(frozen=True)
class SummaryRow:
    graph_kind: str
    label: str
    shape: str
    nodes: int
    edges: int
    cpu_ms_per_run: float | None
    gpu_naive_ms_per_run: float | None
    gpu_non_naive_ms_per_run: float | None
    speedup_cpu_over_gpu_naive: float | None
    speedup_cpu_over_gpu_non_naive: float | None
    gpu_naive_to_non_naive_speedup: float | None
    gpu_naive_iterations: int | None
    gpu_non_naive_iterations: int | None
    gpu_naive_converged: bool | None
    gpu_non_naive_converged: bool | None
    component_count: int | None
    largest_component_size: int | None
    mean_out_degree: float | None
    gpu_naive_edge_iterations: float | None
    gpu_non_naive_edge_iterations: float | None


def load_samples(path: Path) -> list[Sample]:
    samples: list[Sample] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_number, raw in enumerate(handle, start=1):
            line = raw.strip()
            if not line:
                continue
            row = json.loads(line)
            if row.get("benchmark") != "graph_connected_components":
                continue
            variant = str(row.get("variant", ""))
            if variant not in {"cpu", "gpu", "gpu-non-naive"}:
                continue
            metadata = row.get("metadata") or {}
            input_size = row.get("input_size") or {}
            try:
                nodes = int(input_size["nodes"])
                edges = int(input_size["edges"])
                repeat = int(row.get("repeat", 1))
                total_ms = float(row.get("total_ms", 0.0))
            except (KeyError, TypeError, ValueError) as exc:
                raise ValueError(f"Malformed graph_connected_components row on line {line_number}: {row}") from exc
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
                    component_count=optional_int(metadata.get("component_count")),
                    largest_component_size=optional_int(metadata.get("largest_component_size")),
                    mean_out_degree=optional_float(metadata.get("mean_out_degree")),
                )
            )
    return samples


def mean_time(samples: list[Sample]) -> float | None:
    valid = [s.per_run_ms for s in samples if s.correct]
    return sum(valid) / len(valid) if valid else None


def first_valid(samples: list[Sample]) -> Sample | None:
    return next((s for s in samples if s.correct), None)


def summarize(samples: Iterable[Sample]) -> list[SummaryRow]:
    groups: dict[tuple[str, str, int, int], dict[str, list[Sample]]] = defaultdict(lambda: defaultdict(list))
    for sample in samples:
        if sample.graph_kind in GRAPH_KIND_ORDER:
            groups[(sample.graph_kind, sample.label, sample.nodes, sample.edges)][sample.variant].append(sample)

    rows: list[SummaryRow] = []
    for (kind, label, nodes, edges), variants in groups.items():
        cpu = [s for s in variants.get("cpu", []) if s.correct]
        naive = [s for s in variants.get("gpu", []) if s.correct]
        non_naive = [s for s in variants.get("gpu-non-naive", []) if s.correct]
        cpu_ms = mean_time(cpu)
        naive_ms = mean_time(naive)
        non_naive_ms = mean_time(non_naive)
        naive_speedup = cpu_ms / naive_ms if cpu_ms is not None and naive_ms not in {None, 0.0} else None
        non_naive_speedup = cpu_ms / non_naive_ms if cpu_ms is not None and non_naive_ms not in {None, 0.0} else None
        naive_to_non_naive = naive_ms / non_naive_ms if naive_ms is not None and non_naive_ms not in {None, 0.0} else None
        representative = first_valid(non_naive) or first_valid(naive) or first_valid(cpu) or (variants.get("cpu", []) + variants.get("gpu", []) + variants.get("gpu-non-naive", []))[0]
        naive_rep = first_valid(variants.get("gpu", []))
        non_naive_rep = first_valid(variants.get("gpu-non-naive", []))
        rows.append(
            SummaryRow(
                graph_kind=kind,
                label=label,
                shape=representative.shape,
                nodes=nodes,
                edges=edges,
                cpu_ms_per_run=cpu_ms,
                gpu_naive_ms_per_run=naive_ms,
                gpu_non_naive_ms_per_run=non_naive_ms,
                speedup_cpu_over_gpu_naive=naive_speedup,
                speedup_cpu_over_gpu_non_naive=non_naive_speedup,
                gpu_naive_to_non_naive_speedup=naive_to_non_naive,
                gpu_naive_iterations=naive_rep.iterations if naive_rep else None,
                gpu_non_naive_iterations=non_naive_rep.iterations if non_naive_rep else None,
                gpu_naive_converged=naive_rep.converged if naive_rep else None,
                gpu_non_naive_converged=non_naive_rep.converged if non_naive_rep else None,
                component_count=representative.component_count,
                largest_component_size=representative.largest_component_size,
                mean_out_degree=representative.mean_out_degree,
                gpu_naive_edge_iterations=(edges * naive_rep.iterations) if naive_rep and naive_rep.iterations is not None else None,
                gpu_non_naive_edge_iterations=(edges * non_naive_rep.iterations) if non_naive_rep and non_naive_rep.iterations is not None else None,
            )
        )

    def sort_key(row: SummaryRow) -> tuple[int, int, str]:
        order = GRAPH_KIND_ORDER.index(row.graph_kind) if row.graph_kind in GRAPH_KIND_ORDER else len(GRAPH_KIND_ORDER)
        return (order, row.nodes, row.label)

    return sorted(rows, key=sort_key)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot graph_connected_components scaling sweep results.")
    parser.add_argument("--jsonl", required=True, help="Input JSONL file generated by the CC sweep runner.")
    parser.add_argument("--output-dir", default="results/graph_connected_components_shape_scale_plots")
    parser.add_argument("--show", action="store_true")
    return parser.parse_args()


def write_summary_csv(rows: list[SummaryRow], output_dir: Path) -> Path:
    path = output_dir / "graph_connected_components_shape_scaling_summary.csv"
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "graph_kind", "label", "nodes", "edges", "shape", "cpu_ms_per_run",
            "gpu_naive_ms_per_run", "gpu_non_naive_ms_per_run",
            "speedup_cpu_over_gpu_naive", "speedup_cpu_over_gpu_non_naive",
            "gpu_naive_to_non_naive_speedup",
            "gpu_naive_iterations", "gpu_non_naive_iterations",
            "gpu_naive_converged", "gpu_non_naive_converged",
            "component_count", "largest_component_size", "mean_out_degree",
            "gpu_naive_edge_iterations", "gpu_non_naive_edge_iterations",
        ])
        for row in rows:
            writer.writerow([
                row.graph_kind, row.label, row.nodes, row.edges, row.shape, row.cpu_ms_per_run,
                row.gpu_naive_ms_per_run, row.gpu_non_naive_ms_per_run,
                row.speedup_cpu_over_gpu_naive, row.speedup_cpu_over_gpu_non_naive,
                row.gpu_naive_to_non_naive_speedup,
                row.gpu_naive_iterations, row.gpu_non_naive_iterations,
                row.gpu_naive_converged, row.gpu_non_naive_converged,
                row.component_count, row.largest_component_size, row.mean_out_degree,
                row.gpu_naive_edge_iterations, row.gpu_non_naive_edge_iterations,
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
               series: list[tuple[str, Callable[[SummaryRow], float | None]]], *, log_y: bool = False,
               horizontal_one: bool = False) -> Path | None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"matplotlib is not available: {exc}")
        return None
    group_map = grouped(rows)
    plotted = False
    fig, ax = plt.subplots(figsize=(11, 6))
    for kind in GRAPH_KIND_ORDER:
        for suffix, getter in series:
            values = [row for row in group_map[kind] if getter(row) is not None]
            if not values:
                continue
            plotted = True
            x = [row.nodes for row in values]
            y = [float(getter(row)) for row in values]  # type: ignore[arg-type]
            ax.plot(x, y, marker="o", label=f"{GRAPH_KIND_LABEL.get(kind, kind)} {suffix}")
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
    ax.legend(fontsize="small", ncol=2)
    fig.tight_layout()
    path = output_dir / filename
    fig.savefig(path, dpi=150)
    return path


def plot_timing(rows: list[SummaryRow], output_dir: Path) -> Path | None:
    return plot_lines(
        rows,
        output_dir,
        "graph_cc_shape_scaling_times.png",
        "Connected components CPU/GPU timing by graph family",
        "average ms per connected-components run",
        [
            ("CPU", lambda r: r.cpu_ms_per_run),
            ("GPU naive", lambda r: r.gpu_naive_ms_per_run),
            ("GPU non-naive", lambda r: r.gpu_non_naive_ms_per_run),
        ],
        log_y=True,
    )


def plot_scatter(rows: list[SummaryRow], output_dir: Path, filename: str, title: str, xlabel: str,
                 x_getter: Callable[[SummaryRow], float | None],
                 series: list[tuple[str, Callable[[SummaryRow], float | None]]], *, log_x=False, log_y=False,
                 horizontal_one=False) -> Path | None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"matplotlib is not available: {exc}")
        return None
    fig, ax = plt.subplots(figsize=(9, 6))
    plotted = False
    gr = grouped(rows)
    for kind in GRAPH_KIND_ORDER:
        for suffix, y_getter in series:
            values = [row for row in gr[kind] if x_getter(row) is not None and y_getter(row) is not None]
            if not values:
                continue
            plotted = True
            x = [float(x_getter(r)) for r in values]  # type: ignore[arg-type]
            y = [float(y_getter(r)) for r in values]  # type: ignore[arg-type]
            ax.scatter(x, y, label=f"{GRAPH_KIND_LABEL[kind]} {suffix}")
            for row, xi, yi in zip(values, x, y):
                ax.annotate(str(row.nodes), (xi, yi), fontsize=7, alpha=0.65)
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
    ax.set_ylabel("CPU/GPU speedup")
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize="small", ncol=2)
    fig.tight_layout()
    path = output_dir / filename
    fig.savefig(path, dpi=150)
    return path


def plot_mixed_family_focus(rows: list[SummaryRow], output_dir: Path) -> Path | None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"matplotlib is not available: {exc}")
        return None
    values = [r for r in grouped(rows)["mixed"] if r.cpu_ms_per_run is not None]
    if not values:
        print("Skipping graph_cc_mixed_family_focus.png: no mixed-family rows.")
        return None
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot([r.nodes for r in values], [r.cpu_ms_per_run for r in values], marker="o", label="mixed CPU")
    naive = [r for r in values if r.gpu_naive_ms_per_run is not None]
    if naive:
        ax.plot([r.nodes for r in naive], [r.gpu_naive_ms_per_run for r in naive], marker="s", linestyle="--", label="mixed GPU naive")
        for r in naive:
            if r.gpu_naive_iterations is not None:
                ax.annotate(f"naive it={r.gpu_naive_iterations}", (r.nodes, r.gpu_naive_ms_per_run), fontsize=7, alpha=0.75)
    non_naive = [r for r in values if r.gpu_non_naive_ms_per_run is not None]
    if non_naive:
        ax.plot([r.nodes for r in non_naive], [r.gpu_non_naive_ms_per_run for r in non_naive], marker="^", linestyle=":", label="mixed GPU non-naive")
        for r in non_naive:
            if r.gpu_non_naive_iterations is not None:
                ax.annotate(f"non-naive it={r.gpu_non_naive_iterations}", (r.nodes, r.gpu_non_naive_ms_per_run), fontsize=7, alpha=0.75)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("nodes")
    ax.set_ylabel("average ms per connected-components run")
    ax.set_title("Mixed-family connected components: CPU vs GPU variants")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    path = output_dir / "graph_cc_mixed_family_focus.png"
    fig.savefig(path, dpi=150)
    return path


def print_crossover(rows: list[SummaryRow]) -> None:
    print("\nConnected-components crossover summary:")
    for title, getter in [
        ("GPU naive", lambda r: r.speedup_cpu_over_gpu_naive),
        ("GPU non-naive", lambda r: r.speedup_cpu_over_gpu_non_naive),
    ]:
        print(f"  {title}:")
        for kind in GRAPH_KIND_ORDER:
            candidates = [r for r in grouped(rows)[kind] if getter(r) is not None]
            crossover = next((r for r in candidates if getter(r) and getter(r) > 1.0), None)
            if crossover is None:
                print(f"    {GRAPH_KIND_LABEL[kind]}: no crossover observed")
            else:
                print(
                    f"    {GRAPH_KIND_LABEL[kind]}: first crossover at {crossover.label} "
                    f"({crossover.nodes} nodes, {crossover.edges} edges), speedup={getter(crossover):.3f}x"
                )


def main() -> int:
    args = parse_args()
    jsonl = Path(args.jsonl)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    samples = load_samples(jsonl)
    rows = summarize(samples)
    if not rows:
        raise SystemExit(f"No graph_connected_components rows found in {jsonl}")

    csv_path = write_summary_csv(rows, output_dir)
    print(f"Wrote summary CSV: {csv_path}")

    paths: list[Path] = []
    maybe = plot_timing(rows, output_dir)
    if maybe:
        paths.append(maybe)

    for filename, title, ylabel, series, log_y, h1 in [
        (
            "graph_cc_shape_scaling_speedup.png",
            "Connected components CPU/GPU speedup by graph family",
            "CPU ms/run ÷ GPU ms/run",
            [
                ("naive", lambda r: r.speedup_cpu_over_gpu_naive),
                ("non-naive", lambda r: r.speedup_cpu_over_gpu_non_naive),
            ],
            True,
            True,
        ),
        (
            "graph_cc_gpu_iterations.png",
            "GPU connected-components convergence iterations",
            "GPU iterations",
            [
                ("naive", lambda r: float(r.gpu_naive_iterations) if r.gpu_naive_iterations is not None else None),
                ("non-naive", lambda r: float(r.gpu_non_naive_iterations) if r.gpu_non_naive_iterations is not None else None),
            ],
            True,
            False,
        ),
        (
            "graph_cc_largest_component_size.png",
            "Largest component size by graph family",
            "largest component size",
            [("", lambda r: float(r.largest_component_size) if r.largest_component_size is not None else None)],
            True,
            False,
        ),
        (
            "graph_cc_mean_out_degree.png",
            "Mean out-degree by graph family",
            "mean out-degree",
            [("", lambda r: r.mean_out_degree)],
            False,
            False,
        ),
        (
            "graph_cc_naive_vs_non_naive.png",
            "Naive GPU time ÷ non-naive GPU time",
            "naive GPU ms/run ÷ non-naive GPU ms/run",
            [("", lambda r: r.gpu_naive_to_non_naive_speedup)],
            True,
            True,
        ),
    ]:
        maybe = plot_lines(rows, output_dir, filename, title, ylabel, series, log_y=log_y, horizontal_one=h1)
        if maybe:
            paths.append(maybe)

    for filename, title, xlabel, x_getter, log_x in [
        ("graph_cc_speedup_vs_iterations.png", "Connected components speedup vs GPU convergence iterations", "GPU label-propagation iterations", lambda r: None, False),
        ("graph_cc_speedup_vs_edge_count.png", "Connected components speedup vs edge count", "edges", lambda r: float(r.edges), True),
        ("graph_cc_speedup_vs_edge_iterations.png", "Connected components speedup vs GPU edge-iterations", "edges × GPU iterations", lambda r: None, True),
    ]:
        if filename == "graph_cc_speedup_vs_iterations.png":
            maybe = plot_scatter(
                rows,
                output_dir,
                filename,
                title,
                xlabel,
                lambda r: float(r.gpu_naive_iterations) if r.gpu_naive_iterations is not None else None,
                [("naive", lambda r: r.speedup_cpu_over_gpu_naive)],
                log_x=False,
                log_y=True,
                horizontal_one=True,
            )
            maybe2 = plot_scatter(
                rows,
                output_dir,
                "graph_cc_speedup_vs_iterations_non_naive.png",
                "Connected components speedup vs non-naive GPU iterations",
                "non-naive GPU iterations",
                lambda r: float(r.gpu_non_naive_iterations) if r.gpu_non_naive_iterations is not None else None,
                [("non-naive", lambda r: r.speedup_cpu_over_gpu_non_naive)],
                log_x=False,
                log_y=True,
                horizontal_one=True,
            )
            if maybe:
                paths.append(maybe)
            if maybe2:
                paths.append(maybe2)
            continue
        if filename == "graph_cc_speedup_vs_edge_iterations.png":
            maybe = plot_scatter(
                rows,
                output_dir,
                filename,
                title,
                xlabel,
                lambda r: r.gpu_naive_edge_iterations,
                [("naive", lambda r: r.speedup_cpu_over_gpu_naive)],
                log_x=True,
                log_y=True,
                horizontal_one=True,
            )
            maybe2 = plot_scatter(
                rows,
                output_dir,
                "graph_cc_speedup_vs_edge_iterations_non_naive.png",
                "Connected components speedup vs non-naive GPU edge-iterations",
                "edges × non-naive GPU iterations",
                lambda r: r.gpu_non_naive_edge_iterations,
                [("non-naive", lambda r: r.speedup_cpu_over_gpu_non_naive)],
                log_x=True,
                log_y=True,
                horizontal_one=True,
            )
            if maybe:
                paths.append(maybe)
            if maybe2:
                paths.append(maybe2)
            continue
        maybe = plot_scatter(
            rows,
            output_dir,
            filename,
            title,
            xlabel,
            x_getter,
            [
                ("naive", lambda r: r.speedup_cpu_over_gpu_naive),
                ("non-naive", lambda r: r.speedup_cpu_over_gpu_non_naive),
            ],
            log_x=log_x,
            log_y=True,
            horizontal_one=True,
        )
        if maybe:
            paths.append(maybe)

    maybe = plot_mixed_family_focus(rows, output_dir)
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
