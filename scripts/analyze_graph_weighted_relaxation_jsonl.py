#!/usr/bin/env python3
"""Analyze graph_weighted_relaxation JSONL sweep output.

The weighted graph benchmark compares CPU Dijkstra with several GPU weighted
shortest-path experiments:

* `gpu` - global Bellman-Ford-style full edge scans.
* `gpu-frontier` - active-frontier relaxation.
* `gpu-delta-stepping` - simplified bucketed active relaxation.
* `gpu-delta-light-heavy` - final delta-style attempt with light/heavy edge separation.

The analyzer matches rows by graph family, sweep label, node count, and edge
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
VARIANT_ORDER = ["cpu", "gpu", "gpu-frontier", "gpu-delta-stepping", "gpu-delta-light-heavy"]
VARIANT_TO_COLUMN = {
    "gpu": "global",
    "gpu-frontier": "frontier",
    "gpu-delta-stepping": "delta",
    "gpu-delta-light-heavy": "delta_light_heavy",
}
VARIANT_LABEL = {
    "gpu": "global scan",
    "gpu-frontier": "frontier",
    "gpu-delta-stepping": "delta-stepping",
    "gpu-delta-light-heavy": "delta light/heavy",
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
            row = json.loads(line)
            if row.get("benchmark") != "graph_weighted_relaxation":
                continue
            variant = str(row.get("variant", ""))
            if variant not in VARIANT_ORDER:
                continue
            metadata = row.get("metadata") or {}
            input_size = row.get("input_size") or {}
            try:
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
            samples.append(Sample(
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
            ))
    return samples


def key_for(sample: Sample) -> tuple[str, str, int, int]:
    return (sample.graph_kind, sample.label, sample.nodes, sample.edges)


def valid(samples: list[Sample]) -> list[Sample]:
    return [s for s in samples if s.correct]


def mean_per_run(samples: list[Sample]) -> float | None:
    good = valid(samples)
    return sum(s.per_run_ms for s in good) / len(good) if good else None


def representative(samples: list[Sample]) -> Sample | None:
    good = valid(samples)
    return good[0] if good else (samples[0] if samples else None)


def summarize(samples: Iterable[Sample]) -> list[dict[str, object]]:
    groups: dict[tuple[str, str, int, int], dict[str, list[Sample]]] = defaultdict(lambda: defaultdict(list))
    for sample in samples:
        if sample.graph_kind in GRAPH_KIND_ORDER:
            groups[key_for(sample)][sample.variant].append(sample)

    rows: list[dict[str, object]] = []
    for (kind, label, nodes, edges), variants in groups.items():
        cpu_samples = variants.get("cpu", [])
        cpu_ms = mean_per_run(cpu_samples)
        reps = {variant: representative(variants.get(variant, [])) for variant in VARIANT_ORDER}
        base = next((reps[v] for v in ["cpu", "gpu", "gpu-frontier", "gpu-delta-stepping", "gpu-delta-light-heavy"] if reps[v] is not None), None)
        if base is None:
            continue
        row: dict[str, object] = {
            "graph_kind": kind,
            "label": label,
            "nodes": nodes,
            "edges": edges,
            "shape": base.shape,
            "cpu_ms_per_run": cpu_ms,
            "reached_count": base.reached_count,
            "max_finite_distance": base.max_finite_distance,
            "mean_out_degree": base.mean_out_degree,
            "cpu_rows": len(cpu_samples),
            "correct_cpu_rows": len(valid(cpu_samples)),
        }
        for variant, suffix in VARIANT_TO_COLUMN.items():
            samples_v = variants.get(variant, [])
            rep = reps[variant]
            ms = mean_per_run(samples_v)
            speedup = cpu_ms / ms if cpu_ms is not None and ms not in {None, 0.0} else None
            work_estimate = None
            if rep and rep.active_frontier_node_visits is not None and base.mean_out_degree is not None:
                work_estimate = rep.active_frontier_node_visits * base.mean_out_degree
            row[f"gpu_{suffix}_ms_per_run"] = ms
            row[f"speedup_cpu_over_gpu_{suffix}"] = speedup
            row[f"gpu_{suffix}_iterations"] = rep.iterations if rep else None
            row[f"gpu_{suffix}_converged"] = rep.converged if rep else None
            row[f"gpu_{suffix}_edge_iterations"] = rep.edge_iterations if rep else None
            row[f"gpu_{suffix}_active_node_visits"] = rep.active_frontier_node_visits if rep else None
            row[f"gpu_{suffix}_edge_work_estimate"] = work_estimate
            row[f"gpu_{suffix}_max_frontier_size"] = rep.max_frontier_size if rep else None
            row[f"{suffix}_mismatch_count"] = rep.mismatch_count if rep else None
            row[f"gpu_{suffix}_rows"] = len(samples_v)
            row[f"correct_gpu_{suffix}_rows"] = len(valid(samples_v))
        # pairwise speedups against the original global scan
        global_ms = row.get("gpu_global_ms_per_run")
        for suffix in ["frontier", "delta", "delta_light_heavy"]:
            other_ms = row.get(f"gpu_{suffix}_ms_per_run")
            row[f"gpu_global_to_{suffix}_speedup"] = global_ms / other_ms if isinstance(global_ms, float) and isinstance(other_ms, float) and other_ms != 0.0 else None
        rows.append(row)

    def sort_key(row: dict[str, object]) -> tuple[int, int, str]:
        return (GRAPH_KIND_ORDER.index(str(row["graph_kind"])), int(row["nodes"]), str(row["label"]))
    return sorted(rows, key=sort_key)


def fieldnames() -> list[str]:
    fields = ["graph_kind", "label", "nodes", "edges", "shape", "cpu_ms_per_run"]
    for suffix in VARIANT_TO_COLUMN.values():
        fields += [f"gpu_{suffix}_ms_per_run", f"speedup_cpu_over_gpu_{suffix}", f"gpu_{suffix}_iterations", f"gpu_{suffix}_converged"]
    fields += [
        "gpu_global_to_frontier_speedup", "gpu_global_to_delta_speedup", "gpu_global_to_delta_light_heavy_speedup",
        "gpu_global_edge_iterations", "gpu_frontier_active_node_visits", "gpu_delta_active_node_visits", "gpu_delta_light_heavy_active_node_visits",
        "gpu_frontier_edge_work_estimate", "gpu_delta_edge_work_estimate", "gpu_delta_light_heavy_edge_work_estimate",
        "gpu_frontier_max_frontier_size", "gpu_delta_max_frontier_size", "gpu_delta_light_heavy_max_frontier_size",
        "reached_count", "max_finite_distance", "mean_out_degree",
        "global_mismatch_count", "frontier_mismatch_count", "delta_mismatch_count", "delta_light_heavy_mismatch_count",
        "cpu_rows", "gpu_global_rows", "gpu_frontier_rows", "gpu_delta_rows", "gpu_delta_light_heavy_rows",
        "correct_cpu_rows", "correct_gpu_global_rows", "correct_gpu_frontier_rows", "correct_gpu_delta_rows", "correct_gpu_delta_light_heavy_rows",
    ]
    return fields


def write_csv(rows: list[dict[str, object]], output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / "graph_weighted_relaxation_analysis_summary.csv"
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames(), extrasaction="ignore")
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
    path = output_dir / "graph_weighted_relaxation_analysis_report.md"
    lines: list[str] = []
    lines.append("# Graph Weighted Relaxation JSONL Analysis")
    lines.append("")
    lines.append(f"Rows read: {len(samples)}")
    lines.append(f"Matched CPU/GPU size points: {len(rows)}")
    for variant, suffix in VARIANT_TO_COLUMN.items():
        invalid = [s for s in samples if s.variant == variant and not s.correct]
        lines.append(f"Invalid {VARIANT_LABEL[variant]} GPU rows: {len(invalid)}")
    lines.append("")
    lines.append("## First GPU crossover by family")
    lines.append("")
    for kind in GRAPH_KIND_ORDER:
        for variant, suffix in VARIANT_TO_COLUMN.items():
            cross = first_crossover(rows, kind, f"speedup_cpu_over_gpu_{suffix}")
            label = VARIANT_LABEL[variant]
            if cross is None:
                lines.append(f"- `{kind}` / {label}: no GPU crossover observed in this sweep.")
            else:
                lines.append(f"- `{kind}` / {label}: first crossover at `{cross['label']}` ({cross['nodes']} nodes, {cross['edges']} edges), speedup={float(cross[f'speedup_cpu_over_gpu_{suffix}']):.3f}x.")
    lines.append("")
    lines.append("## Summary table")
    lines.append("")
    lines.append("| kind | label | nodes | edges | CPU | global | frontier | delta | light/heavy | CPU/global | CPU/frontier | CPU/delta | CPU/light-heavy | iters global/frontier/delta/light-heavy |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for row in rows:
        lines.append(
            f"| {row['graph_kind']} | {row['label']} | {row['nodes']} | {row['edges']} | "
            f"{fmt(row.get('cpu_ms_per_run'))} | {fmt(row.get('gpu_global_ms_per_run'))} | {fmt(row.get('gpu_frontier_ms_per_run'))} | {fmt(row.get('gpu_delta_ms_per_run'))} | {fmt(row.get('gpu_delta_light_heavy_ms_per_run'))} | "
            f"{fmt(row.get('speedup_cpu_over_gpu_global'))} | {fmt(row.get('speedup_cpu_over_gpu_frontier'))} | {fmt(row.get('speedup_cpu_over_gpu_delta'))} | {fmt(row.get('speedup_cpu_over_gpu_delta_light_heavy'))} | "
            f"{fmt(row.get('gpu_global_iterations'))}/{fmt(row.get('gpu_frontier_iterations'))}/{fmt(row.get('gpu_delta_iterations'))}/{fmt(row.get('gpu_delta_light_heavy_iterations'))} |"
        )
    lines.append("")
    lines.append("## Interpretation")
    lines.append("")
    lines.append("The CPU baseline is Dijkstra with a priority queue over positive integer weights.")
    lines.append("The original `gpu` variant is a global Bellman-Ford-style edge scan: every pass scans all edges.")
    lines.append("`gpu-frontier` and `gpu-delta-stepping` are active-set/bucket experiments.")
    lines.append("`gpu-delta-light-heavy` is the final weighted-graph attempt in this project: it closes a bucket using light edges before relaxing heavy edges out of that bucket.")
    lines.append("If the light/heavy variant still does not win, the weighted SSSP chapter should be treated as a valuable counterexample: simple GPU adaptations are not enough to beat a strong CPU Dijkstra baseline except for low-diameter random graphs where the global GPU scan has enough parallel work.")
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
    print(f"Wrote summary CSV: {write_csv(rows, output_dir)}")
    print(f"Wrote report: {write_report(samples, rows, output_dir)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
