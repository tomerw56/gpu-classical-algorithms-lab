#!/usr/bin/env python3
"""Analyze graph_weighted_relaxation JSONL sweep output.

The weighted graph benchmark compares CPU Dijkstra with two GPU shortest-path
experiments:

* `gpu` - global Bellman-Ford-style edge scans.
* `gpu-frontier` - active-frontier relaxation that only expands nodes whose
  distance changed in the previous round.

This analyzer matches rows by graph family, sweep label, node count, and edge
count, then reports crossover points and derived work metrics.
"""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

GRAPH_KIND_ORDER = ["chain", "grid", "layered", "random"]
GPU_VARIANTS = ["gpu", "gpu-frontier"]


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
    total_ms: float
    per_run_ms: float
    correct: bool
    iterations: int | None
    max_iterations: int | None
    converged: bool | None
    reached_count: int | None
    max_finite_distance: int | None
    mismatch_count: int | None
    mean_out_degree: float | None
    edge_iterations: int | None
    active_frontier_node_visits: int | None
    max_frontier_size: int | None


def load_samples(path: Path) -> list[Sample]:
    samples: list[Sample] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_number, raw in enumerate(handle, start=1):
            line = raw.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"Invalid JSON on line {line_number}: {exc}") from exc
            if row.get("benchmark") != "graph_weighted_relaxation":
                continue
            metadata = row.get("metadata") or {}
            input_size = row.get("input_size") or {}
            try:
                variant = str(row.get("variant", ""))
                label = str(row.get("preset", ""))
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
                    label=label,
                    graph_kind=normalize_kind(str(metadata.get("graph_kind", ""))),
                    shape=str(metadata.get("shape", "")),
                    nodes=nodes,
                    edges=edges,
                    repeat=repeat,
                    total_ms=total_ms,
                    per_run_ms=total_ms / repeat,
                    correct=bool(row.get("correct", False)),
                    iterations=optional_int(metadata.get("iterations")),
                    max_iterations=optional_int(metadata.get("max_iterations")),
                    converged=converged,
                    reached_count=optional_int(metadata.get("reached_count")),
                    max_finite_distance=optional_int(metadata.get("max_finite_distance")),
                    mismatch_count=optional_int(metadata.get("mismatch_count")),
                    mean_out_degree=optional_float(metadata.get("mean_out_degree")),
                    edge_iterations=optional_int(metadata.get("edge_iterations")),
                    active_frontier_node_visits=optional_int(metadata.get("active_frontier_node_visits")),
                    max_frontier_size=optional_int(metadata.get("max_frontier_size")),
                )
            )
    return samples


def key_for(sample: Sample) -> tuple[str, str, int, int]:
    return (sample.graph_kind, sample.label, sample.nodes, sample.edges)


def mean_per_run(samples: list[Sample]) -> float | None:
    valid = [s for s in samples if s.correct]
    return sum(s.per_run_ms for s in valid) / len(valid) if valid else None


def representative(samples: list[Sample]) -> Sample | None:
    valid = [s for s in samples if s.correct]
    return valid[0] if valid else (samples[0] if samples else None)


def summarize(samples: Iterable[Sample]) -> list[dict[str, object]]:
    groups: dict[tuple[str, str, int, int], dict[str, list[Sample]]] = defaultdict(lambda: defaultdict(list))
    for sample in samples:
        if sample.variant in {"cpu", "gpu", "gpu-frontier"}:
            groups[key_for(sample)][sample.variant].append(sample)

    rows: list[dict[str, object]] = []
    for (kind, label, nodes, edges), variants in groups.items():
        cpu = variants.get("cpu", [])
        global_gpu = variants.get("gpu", [])
        frontier_gpu = variants.get("gpu-frontier", [])
        cpu_ms = mean_per_run(cpu)
        global_ms = mean_per_run(global_gpu)
        frontier_ms = mean_per_run(frontier_gpu)
        base_rep = representative(frontier_gpu) or representative(global_gpu) or representative(cpu)
        if base_rep is None:
            continue
        global_rep = representative(global_gpu)
        frontier_rep = representative(frontier_gpu)
        global_speedup = cpu_ms / global_ms if cpu_ms is not None and global_ms not in {None, 0.0} else None
        frontier_speedup = cpu_ms / frontier_ms if cpu_ms is not None and frontier_ms not in {None, 0.0} else None
        global_to_frontier = global_ms / frontier_ms if global_ms is not None and frontier_ms not in {None, 0.0} else None
        frontier_edge_work_estimate = None
        if frontier_rep and frontier_rep.active_frontier_node_visits is not None and base_rep.mean_out_degree is not None:
            frontier_edge_work_estimate = frontier_rep.active_frontier_node_visits * base_rep.mean_out_degree
        rows.append(
            {
                "graph_kind": kind,
                "label": label,
                "nodes": nodes,
                "edges": edges,
                "shape": base_rep.shape,
                "cpu_ms_per_run": cpu_ms,
                "gpu_global_ms_per_run": global_ms,
                "gpu_frontier_ms_per_run": frontier_ms,
                "speedup_cpu_over_gpu_global": global_speedup,
                "speedup_cpu_over_gpu_frontier": frontier_speedup,
                "gpu_global_to_frontier_speedup": global_to_frontier,
                "gpu_global_iterations": global_rep.iterations if global_rep else None,
                "gpu_frontier_iterations": frontier_rep.iterations if frontier_rep else None,
                "gpu_global_converged": global_rep.converged if global_rep else None,
                "gpu_frontier_converged": frontier_rep.converged if frontier_rep else None,
                "gpu_global_edge_iterations": global_rep.edge_iterations if global_rep else None,
                "gpu_frontier_active_node_visits": frontier_rep.active_frontier_node_visits if frontier_rep else None,
                "gpu_frontier_edge_work_estimate": frontier_edge_work_estimate,
                "gpu_frontier_max_frontier_size": frontier_rep.max_frontier_size if frontier_rep else None,
                "reached_count": base_rep.reached_count,
                "max_finite_distance": base_rep.max_finite_distance,
                "mean_out_degree": base_rep.mean_out_degree,
                "global_mismatch_count": global_rep.mismatch_count if global_rep else None,
                "frontier_mismatch_count": frontier_rep.mismatch_count if frontier_rep else None,
                "cpu_rows": len(cpu),
                "gpu_global_rows": len(global_gpu),
                "gpu_frontier_rows": len(frontier_gpu),
                "correct_cpu_rows": len([s for s in cpu if s.correct]),
                "correct_gpu_global_rows": len([s for s in global_gpu if s.correct]),
                "correct_gpu_frontier_rows": len([s for s in frontier_gpu if s.correct]),
            }
        )

    def sort_key(row: dict[str, object]) -> tuple[int, int, str]:
        kind = str(row["graph_kind"])
        order = GRAPH_KIND_ORDER.index(kind) if kind in GRAPH_KIND_ORDER else len(GRAPH_KIND_ORDER)
        return (order, int(row["nodes"]), str(row["label"]))

    return sorted(rows, key=sort_key)


def write_csv(rows: list[dict[str, object]], output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / "graph_weighted_relaxation_analysis_summary.csv"
    fieldnames = [
        "graph_kind", "label", "nodes", "edges", "shape",
        "cpu_ms_per_run", "gpu_global_ms_per_run", "gpu_frontier_ms_per_run",
        "speedup_cpu_over_gpu_global", "speedup_cpu_over_gpu_frontier", "gpu_global_to_frontier_speedup",
        "gpu_global_iterations", "gpu_frontier_iterations", "gpu_global_converged", "gpu_frontier_converged",
        "gpu_global_edge_iterations", "gpu_frontier_active_node_visits", "gpu_frontier_edge_work_estimate",
        "gpu_frontier_max_frontier_size", "reached_count", "max_finite_distance", "mean_out_degree",
        "global_mismatch_count", "frontier_mismatch_count",
        "cpu_rows", "gpu_global_rows", "gpu_frontier_rows", "correct_cpu_rows", "correct_gpu_global_rows", "correct_gpu_frontier_rows",
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


def first_crossover(rows: list[dict[str, object]], kind: str, key: str) -> dict[str, object] | None:
    candidates = [r for r in rows if r["graph_kind"] == kind and r.get(key) is not None]
    return next((r for r in candidates if float(r[key]) > 1.0), None)


def write_report(samples: list[Sample], rows: list[dict[str, object]], output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / "graph_weighted_relaxation_analysis_report.md"
    invalid_global = [s for s in samples if s.variant == "gpu" and not s.correct]
    invalid_frontier = [s for s in samples if s.variant == "gpu-frontier" and not s.correct]

    lines: list[str] = []
    lines.append("# Graph Weighted Relaxation JSONL Analysis")
    lines.append("")
    lines.append(f"Rows read: {len(samples)}")
    lines.append(f"Matched CPU/GPU size points: {len(rows)}")
    lines.append(f"Invalid global-scan GPU rows: {len(invalid_global)}")
    lines.append(f"Invalid frontier GPU rows: {len(invalid_frontier)}")
    lines.append("")
    lines.append("## First GPU crossover by family")
    lines.append("")
    for kind in GRAPH_KIND_ORDER:
        global_cross = first_crossover(rows, kind, "speedup_cpu_over_gpu_global")
        frontier_cross = first_crossover(rows, kind, "speedup_cpu_over_gpu_frontier")
        if global_cross is None:
            lines.append(f"- `{kind}` / global scan: no GPU crossover observed in this sweep.")
        else:
            lines.append(
                f"- `{kind}` / global scan: first crossover at `{global_cross['label']}` "
                f"({global_cross['nodes']} nodes, {global_cross['edges']} edges), "
                f"speedup={float(global_cross['speedup_cpu_over_gpu_global']):.3f}x."
            )
        if frontier_cross is None:
            lines.append(f"- `{kind}` / frontier: no GPU crossover observed in this sweep.")
        else:
            lines.append(
                f"- `{kind}` / frontier: first crossover at `{frontier_cross['label']}` "
                f"({frontier_cross['nodes']} nodes, {frontier_cross['edges']} edges), "
                f"speedup={float(frontier_cross['speedup_cpu_over_gpu_frontier']):.3f}x."
            )
    lines.append("")
    lines.append("## Summary table")
    lines.append("")
    lines.append("| kind | label | nodes | edges | CPU ms/run | GPU global ms/run | GPU frontier ms/run | CPU/global speedup | CPU/frontier speedup | global/frontier speedup | global iters | frontier iters |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for row in rows:
        lines.append(
            f"| {row['graph_kind']} | {row['label']} | {row['nodes']} | {row['edges']} | "
            f"{fmt(row['cpu_ms_per_run'])} | {fmt(row['gpu_global_ms_per_run'])} | {fmt(row['gpu_frontier_ms_per_run'])} | "
            f"{fmt(row['speedup_cpu_over_gpu_global'])} | {fmt(row['speedup_cpu_over_gpu_frontier'])} | "
            f"{fmt(row['gpu_global_to_frontier_speedup'])} | {fmt(row['gpu_global_iterations'])} | {fmt(row['gpu_frontier_iterations'])} |"
        )
    lines.append("")
    lines.append("## Interpretation")
    lines.append("")
    lines.append("The CPU baseline is Dijkstra with a priority queue over positive integer weights.")
    lines.append("The original `gpu` variant is a global Bellman-Ford-style edge scan: every pass scans all edges.")
    lines.append("The new `gpu-frontier` variant only expands nodes whose distance changed and appends improved targets directly into the next active frontier.")
    lines.append("The frontier variant is expected to help high-diameter graph families such as chains and grids because it avoids repeated full-graph scans. It can still lose if per-iteration launch/atomic overhead dominates, or if duplicate frontier entries create extra work.")
    lines.append("")

    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze graph_weighted_relaxation JSONL sweep output.")
    parser.add_argument("--jsonl", required=True)
    parser.add_argument("--output-dir", default="results/graph_weighted_relaxation_shape_scale_analysis")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    jsonl = Path(args.jsonl)
    output_dir = Path(args.output_dir)
    samples = load_samples(jsonl)
    rows = summarize(samples)
    output_dir.mkdir(parents=True, exist_ok=True)
    csv_path = write_csv(rows, output_dir)
    report_path = write_report(samples, rows, output_dir)
    print(f"Wrote summary CSV: {csv_path}")
    print(f"Wrote report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
