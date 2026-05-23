#!/usr/bin/env python3
"""Plot graph_weighted_relaxation shape x scale sweep results.

Rows are normalized to average milliseconds per run (`total_ms / repeat`).
The plotter includes the final weighted-graph attempt, `gpu-delta-light-heavy`.
"""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

GRAPH_KIND_ORDER = ["chain", "grid", "layered", "random"]
VARIANT_ORDER = ["cpu", "gpu", "gpu-frontier", "gpu-delta-stepping", "gpu-delta-light-heavy"]
VARIANT_LABEL = {
    "cpu": "CPU Dijkstra",
    "gpu": "GPU global",
    "gpu-frontier": "GPU frontier",
    "gpu-delta-stepping": "GPU delta",
    "gpu-delta-light-heavy": "GPU delta light/heavy",
}


def normalize_kind(kind: str) -> str:
    if kind in {"chains", "chain_components"}:
        return "chain"
    if kind in {"grids", "grid_components"}:
        return "grid"
    if kind in {"random_sparse", "random_clusters"}:
        return "random"
    return kind


@dataclass(frozen=True)
class Sample:
    variant: str
    label: str
    graph_kind: str
    nodes: int
    edges: int
    repeat: int
    per_run_ms: float
    correct: bool
    iterations: int | None
    edge_iterations: int | None
    active_frontier_node_visits: int | None
    max_frontier_size: int | None
    mean_out_degree: float | None


def to_int(value: object) -> int | None:
    try:
        if value is None or value == "":
            return None
        return int(value)
    except (TypeError, ValueError):
        return None


def to_float(value: object) -> float | None:
    try:
        if value is None or value == "":
            return None
        return float(value)
    except (TypeError, ValueError):
        return None


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
            if variant not in VARIANT_ORDER:
                continue
            metadata = row.get("metadata") or {}
            input_size = row.get("input_size") or {}
            repeat = int(row.get("repeat", 1))
            total_ms = float(row.get("total_ms", 0.0))
            samples.append(Sample(
                variant=variant,
                label=str(row.get("preset", "")),
                graph_kind=normalize_kind(str(metadata.get("graph_kind", ""))),
                nodes=int(input_size["nodes"]),
                edges=int(input_size["edges"]),
                repeat=repeat,
                per_run_ms=total_ms / repeat,
                correct=bool(row.get("correct", False)),
                iterations=to_int(metadata.get("iterations")),
                edge_iterations=to_int(metadata.get("edge_iterations")),
                active_frontier_node_visits=to_int(metadata.get("active_frontier_node_visits")),
                max_frontier_size=to_int(metadata.get("max_frontier_size")),
                mean_out_degree=to_float(metadata.get("mean_out_degree")),
            ))
    return samples


def grouped(samples: list[Sample]):
    groups: dict[tuple[str, str, int, int], dict[str, list[Sample]]] = defaultdict(lambda: defaultdict(list))
    for sample in samples:
        if sample.graph_kind in GRAPH_KIND_ORDER and sample.correct:
            groups[(sample.graph_kind, sample.label, sample.nodes, sample.edges)][sample.variant].append(sample)
    rows = []
    for (kind, label, nodes, edges), variants in groups.items():
        row = {"graph_kind": kind, "label": label, "nodes": nodes, "edges": edges}
        for variant in VARIANT_ORDER:
            vals = variants.get(variant, [])
            if vals:
                row[f"{variant}_ms"] = sum(v.per_run_ms for v in vals) / len(vals)
                row[f"{variant}_iters"] = vals[0].iterations
                if vals[0].active_frontier_node_visits is not None and vals[0].mean_out_degree is not None:
                    row[f"{variant}_work"] = vals[0].active_frontier_node_visits * vals[0].mean_out_degree
                elif vals[0].edge_iterations is not None:
                    row[f"{variant}_work"] = vals[0].edge_iterations
        cpu = row.get("cpu_ms")
        for variant in VARIANT_ORDER[1:]:
            ms = row.get(f"{variant}_ms")
            if isinstance(cpu, float) and isinstance(ms, float) and ms != 0:
                row[f"speedup_{variant}"] = cpu / ms
        rows.append(row)
    return sorted(rows, key=lambda r: (GRAPH_KIND_ORDER.index(r["graph_kind"]), r["nodes"], r["label"]))


def by_kind(rows):
    out = {kind: [] for kind in GRAPH_KIND_ORDER}
    for row in rows:
        out[row["graph_kind"]].append(row)
    for vals in out.values():
        vals.sort(key=lambda r: r["nodes"])
    return out


def plot_times(rows, output_dir: Path):
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(12, 7))
    plotted = False
    for kind, vals in by_kind(rows).items():
        for variant in VARIANT_ORDER:
            pts = [r for r in vals if f"{variant}_ms" in r]
            if not pts:
                continue
            plotted = True
            ax.plot([r["nodes"] for r in pts], [r[f"{variant}_ms"] for r in pts], marker="o", label=f"{kind} {VARIANT_LABEL[variant]}")
    if not plotted:
        return None
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("nodes")
    ax.set_ylabel("average ms per run")
    ax.set_title("Weighted shortest path: CPU/GPU timing by graph family")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(ncol=2, fontsize="x-small")
    fig.tight_layout()
    path = output_dir / "graph_wr_shape_scaling_times.png"
    fig.savefig(path, dpi=150)
    return path


def plot_speedup(rows, output_dir: Path):
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(11, 7))
    plotted = False
    for kind, vals in by_kind(rows).items():
        for variant in VARIANT_ORDER[1:]:
            pts = [r for r in vals if f"speedup_{variant}" in r]
            if not pts:
                continue
            plotted = True
            ax.plot([r["nodes"] for r in pts], [r[f"speedup_{variant}"] for r in pts], marker="o", label=f"{kind} {VARIANT_LABEL[variant]}")
    if not plotted:
        return None
    ax.axhline(1.0, linestyle="--", linewidth=1)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("nodes")
    ax.set_ylabel("CPU ms/run ÷ GPU ms/run")
    ax.set_title("Weighted shortest path: CPU/GPU speedup")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(ncol=2, fontsize="x-small")
    fig.tight_layout()
    path = output_dir / "graph_wr_shape_scaling_speedup.png"
    fig.savefig(path, dpi=150)
    return path


def plot_light_heavy_comparison(rows, output_dir: Path):
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(10, 6))
    plotted = False
    for kind, vals in by_kind(rows).items():
        pts = [r for r in vals if "gpu_ms" in r and "gpu-delta-light-heavy_ms" in r]
        if not pts:
            continue
        plotted = True
        ax.plot([r["nodes"] for r in pts], [r["gpu_ms"] / r["gpu-delta-light-heavy_ms"] for r in pts], marker="o", label=kind)
    if not plotted:
        return None
    ax.axhline(1.0, linestyle="--", linewidth=1)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("nodes")
    ax.set_ylabel("GPU global ms/run ÷ GPU light/heavy ms/run")
    ax.set_title("Weighted shortest path: global GPU vs light/heavy delta attempt")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    path = output_dir / "graph_wr_global_to_light_heavy_speedup.png"
    fig.savefig(path, dpi=150)
    return path


def plot_work(rows, output_dir: Path):
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(10, 6))
    plotted = False
    for kind, vals in by_kind(rows).items():
        pts = [r for r in vals if "gpu-delta-light-heavy_work" in r and "speedup_gpu-delta-light-heavy" in r]
        if not pts:
            continue
        plotted = True
        ax.scatter([r["gpu-delta-light-heavy_work"] for r in pts], [r["speedup_gpu-delta-light-heavy"] for r in pts], label=kind)
        for r in pts:
            ax.annotate(str(r["nodes"]), (r["gpu-delta-light-heavy_work"], r["speedup_gpu-delta-light-heavy"]), fontsize=7, alpha=0.7)
    if not plotted:
        return None
    ax.axhline(1.0, linestyle="--", linewidth=1)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("estimated light/heavy active work")
    ax.set_ylabel("CPU/GPU light-heavy speedup")
    ax.set_title("Weighted shortest path: light/heavy delta work estimate vs speedup")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    path = output_dir / "graph_wr_light_heavy_speedup_vs_work.png"
    fig.savefig(path, dpi=150)
    return path


def print_summary(rows):
    print("\nWeighted-relaxation crossover summary:")
    for kind, vals in by_kind(rows).items():
        for variant in VARIANT_ORDER[1:]:
            cross = next((r for r in vals if r.get(f"speedup_{variant}", 0) > 1.0), None)
            name = VARIANT_LABEL[variant]
            if cross:
                print(f"  {kind} / {name}: first crossover at {cross['label']} ({cross['nodes']} nodes), speedup={cross[f'speedup_{variant}']:.3f}x")
            else:
                print(f"  {kind} / {name}: no crossover observed")


def parse_args():
    parser = argparse.ArgumentParser(description="Plot graph_weighted_relaxation scaling results.")
    parser.add_argument("--jsonl", required=True)
    parser.add_argument("--output-dir", default="results/graph_weighted_relaxation_shape_scale_plots")
    parser.add_argument("--show", action="store_true")
    return parser.parse_args()


def main():
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    rows = grouped(load_samples(Path(args.jsonl)))
    if not rows:
        raise SystemExit("No graph_weighted_relaxation rows found")
    paths = []
    for fn in [plot_times, plot_speedup, plot_light_heavy_comparison, plot_work]:
        path = fn(rows, output_dir)
        if path:
            paths.append(path)
    print_summary(rows)
    for p in paths:
        print(f"Wrote plot: {p}")
    if args.show:
        import matplotlib.pyplot as plt
        plt.show()


if __name__ == "__main__":
    main()
