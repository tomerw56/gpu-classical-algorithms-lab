#!/usr/bin/env python3
"""Recommend the best backend for graph_weighted_relaxation sweep results.

This script consumes the analysis summary CSV produced by
`analyze_graph_weighted_relaxation_jsonl.py` and writes a small backend-policy
report. It is deliberately descriptive: it does not pretend that a weak GPU
variant is good. It names the fastest measured backend and explains why the
other variants likely lost.
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path

BACKEND_COLUMNS = [
    ("cpu", "cpu_ms_per_run"),
    ("gpu-global", "gpu_global_ms_per_run"),
    ("gpu-frontier", "gpu_frontier_ms_per_run"),
    ("gpu-delta-stepping", "gpu_delta_ms_per_run"),
    ("gpu-delta-light-heavy", "gpu_delta_light_heavy_ms_per_run"),
]


def as_float(value: object) -> float | None:
    if value is None:
        return None
    text = str(value).strip()
    if not text:
        return None
    try:
        return float(text)
    except ValueError:
        return None


def as_int(value: object) -> int | None:
    value_f = as_float(value)
    return None if value_f is None else int(value_f)


def load_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def choose_backend(row: dict[str, str]) -> tuple[str, float | None]:
    candidates: list[tuple[str, float]] = []
    for name, column in BACKEND_COLUMNS:
        value = as_float(row.get(column))
        if value is not None:
            candidates.append((name, value))
    if not candidates:
        return "n/a", None
    return min(candidates, key=lambda item: item[1])


def reason_for(row: dict[str, str], best: str) -> str:
    kind = row.get("graph_kind", "")
    global_iters = as_int(row.get("gpu_global_iterations"))
    frontier_iters = as_int(row.get("gpu_frontier_iterations"))
    frontier_max = as_int(row.get("gpu_frontier_max_frontier_size"))
    nodes = as_int(row.get("nodes"))
    edges = as_int(row.get("edges"))

    if best == "cpu":
        if kind in {"chain", "grid"}:
            return (
                "CPU Dijkstra wins: this family has high effective diameter, "
                "so GPU relaxation needs many synchronization rounds."
            )
        if global_iters is not None and global_iters > 32:
            return (
                "CPU Dijkstra wins: GPU relaxation needed many iterations, "
                "so repeated global work dominated."
            )
        return "CPU Dijkstra wins: graph is not large/parallel enough to amortize GPU overhead."
    if best == "gpu-global":
        return (
            "Global GPU scan wins: enough edge parallelism exists and iteration count is low enough "
            "that full edge scans are cheaper than CPU Dijkstra."
        )
    if best == "gpu-frontier":
        if frontier_max is not None and nodes is not None and frontier_max > nodes:
            return (
                "Frontier GPU wins despite duplicate frontier entries; active relaxation reduced enough work "
                "to beat the other variants."
            )
        return "Frontier GPU wins: the active set stayed useful enough to avoid repeated full edge scans."
    if best == "gpu-delta-stepping":
        return (
            "Delta-stepping-style GPU wins: bucket ordering reduced unnecessary relaxation work enough "
            "to beat CPU Dijkstra and the other GPU variants."
        )
    if best == "gpu-delta-light-heavy":
        return (
            "Light/heavy delta-style GPU wins: separating light bucket-closure edges from heavy outgoing edges "
            "reduced enough unnecessary relaxation work to beat the other backends."
        )
    return "No recommendation available."


def enriched_rows(rows: list[dict[str, str]]) -> list[dict[str, object]]:
    out: list[dict[str, object]] = []
    for row in rows:
        best, best_ms = choose_backend(row)
        global_ms = as_float(row.get("gpu_global_ms_per_run"))
        frontier_ms = as_float(row.get("gpu_frontier_ms_per_run"))
        delta_ms = as_float(row.get("gpu_delta_ms_per_run"))
        light_heavy_ms = as_float(row.get("gpu_delta_light_heavy_ms_per_run"))
        cpu_ms = as_float(row.get("cpu_ms_per_run"))
        out.append(
            {
                "graph_kind": row.get("graph_kind", ""),
                "label": row.get("label", ""),
                "nodes": row.get("nodes", ""),
                "edges": row.get("edges", ""),
                "cpu_ms_per_run": cpu_ms,
                "gpu_global_ms_per_run": global_ms,
                "gpu_frontier_ms_per_run": frontier_ms,
                "gpu_delta_ms_per_run": delta_ms,
                "gpu_delta_light_heavy_ms_per_run": light_heavy_ms,
                "best_backend": best,
                "best_ms_per_run": best_ms,
                "gpu_global_iterations": row.get("gpu_global_iterations", ""),
                "gpu_frontier_iterations": row.get("gpu_frontier_iterations", ""),
                "gpu_delta_iterations": row.get("gpu_delta_iterations", ""),
                "gpu_frontier_max_frontier_size": row.get("gpu_frontier_max_frontier_size", ""),
                "gpu_delta_max_frontier_size": row.get("gpu_delta_max_frontier_size", ""),
                "reason": reason_for(row, best),
            }
        )
    return out


def write_csv(rows: list[dict[str, object]], output_dir: Path) -> Path:
    path = output_dir / "graph_weighted_relaxation_backend_recommendations.csv"
    fieldnames = [
        "graph_kind",
        "label",
        "nodes",
        "edges",
        "cpu_ms_per_run",
        "gpu_global_ms_per_run",
        "gpu_frontier_ms_per_run",
        "gpu_delta_ms_per_run",
        "gpu_delta_light_heavy_ms_per_run",
        "best_backend",
        "best_ms_per_run",
        "gpu_global_iterations",
        "gpu_frontier_iterations",
        "gpu_delta_iterations",
        "gpu_frontier_max_frontier_size",
        "gpu_delta_max_frontier_size",
        "reason",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    return path


def fmt(value: object) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, float):
        return f"{value:.6f}"
    return str(value)


def write_report(rows: list[dict[str, object]], output_dir: Path) -> Path:
    path = output_dir / "graph_weighted_relaxation_backend_recommendations.md"
    lines: list[str] = []
    lines.append("# Graph weighted-relaxation backend recommendations")
    lines.append("")
    lines.append("This report is generated from the measured sweep results. It does not assume that GPU is always preferable.")
    lines.append("")
    lines.append("## Main conclusion")
    lines.append("")
    lines.append("For this benchmark, CPU Dijkstra remains the best backend for high-diameter chain/grid/layered cases in the measured range. The global GPU relaxation becomes useful on random graphs where enough edge parallelism exists and the iteration count remains low. The frontier, delta-stepping-style, and light/heavy delta-style GPU variants are experimental; the recommendation is based on measured results, not assumptions.")
    lines.append("")
    lines.append("## Recommendation table")
    lines.append("")
    lines.append("| kind | label | nodes | edges | best backend | best ms/run | reason |")
    lines.append("|---|---:|---:|---:|---:|---:|---|")
    for row in rows:
        lines.append(
            f"| {row['graph_kind']} | {row['label']} | {row['nodes']} | {row['edges']} | "
            f"{row['best_backend']} | {fmt(row['best_ms_per_run'])} | {row['reason']} |"
        )
    lines.append("")
    lines.append("## Next serious GPU direction")
    lines.append("")
    lines.append("The final weighted-graph GPU attempt in this project is `gpu-delta-light-heavy`. If it does not win, the chapter should be treated as an educational counterexample: simple GPU adaptations of weighted SSSP are not enough. Stronger future work would need production-grade delta stepping with per-bucket deduplication and better device-side scheduling, or structure-aware backends: prefix/scan-style treatment for chain-like graphs, tiled/local methods for grids, and global GPU scans for low-diameter random graphs.")
    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def write_plot(rows: list[dict[str, object]], output_dir: Path) -> Path | None:
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"matplotlib is not available: {exc}")
        return None

    backend_index = {"cpu": 0, "gpu-global": 1, "gpu-frontier": 2, "gpu-delta-stepping": 3, "gpu-delta-light-heavy": 4}
    backend_label = {0: "CPU", 1: "GPU global", 2: "GPU frontier", 3: "GPU delta", 4: "GPU light/heavy"}
    kinds = ["chain", "grid", "layered", "random"]
    fig, ax = plt.subplots(figsize=(10, 5.5))
    plotted = False
    for kind in kinds:
        values = [r for r in rows if r["graph_kind"] == kind and r["best_backend"] in backend_index]
        if not values:
            continue
        values.sort(key=lambda r: int(r["nodes"]))
        plotted = True
        ax.plot(
            [int(r["nodes"]) for r in values],
            [backend_index[str(r["best_backend"])] for r in values],
            marker="o",
            label=kind,
        )
    if not plotted:
        plt.close(fig)
        return None
    ax.set_xscale("log")
    ax.set_yticks([0, 1, 2, 3, 4], [backend_label[0], backend_label[1], backend_label[2], backend_label[3], backend_label[4]])
    ax.set_xlabel("nodes")
    ax.set_ylabel("fastest measured backend")
    ax.set_title("Weighted shortest path: fastest measured backend by graph family")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    path = output_dir / "graph_wr_backend_recommendations.png"
    fig.savefig(path, dpi=150)
    return path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Recommend backend choices from weighted-relaxation analysis CSV.")
    parser.add_argument("--summary-csv", required=True, help="CSV from analyze_graph_weighted_relaxation_jsonl.py")
    parser.add_argument("--output-dir", default="results/graph_weighted_relaxation_backend_recommendations")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    rows = enriched_rows(load_rows(Path(args.summary_csv)))
    csv_path = write_csv(rows, output_dir)
    report_path = write_report(rows, output_dir)
    plot_path = write_plot(rows, output_dir)
    print(f"Wrote backend recommendation CSV: {csv_path}")
    print(f"Wrote backend recommendation report: {report_path}")
    if plot_path:
        print(f"Wrote backend recommendation plot: {plot_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
