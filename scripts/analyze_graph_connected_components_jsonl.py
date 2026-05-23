#!/usr/bin/env python3
"""Analyze graph_connected_components JSONL sweep results.

The connected-components benchmark may produce three variants per size point:

    cpu
    gpu
    gpu-non-naive

This analyzer matches them by graph family, label, node count, and edge count;
then reports crossover points for both GPU implementations.
"""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

GRAPH_KIND_ORDER = ["chains", "grid_components", "random_clusters", "mixed"]
GPU_VARIANTS = ["gpu", "gpu-non-naive"]


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
    benchmark: str
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
    component_count: int | None
    largest_component_size: int | None
    mean_out_degree: float | None
    gpu_algorithm: str


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
            if row.get("benchmark") != "graph_connected_components":
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
                raise ValueError(f"Malformed graph_connected_components row on line {line_number}: {row}") from exc
            if repeat <= 0:
                raise ValueError(f"Invalid repeat count on line {line_number}: {repeat}")
            converged_raw = metadata.get("converged")
            if converged_raw is None:
                converged: bool | None = None
            else:
                converged = str(converged_raw).lower() == "true"
            samples.append(
                Sample(
                    benchmark="graph_connected_components",
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
                    component_count=optional_int(metadata.get("component_count")),
                    largest_component_size=optional_int(metadata.get("largest_component_size")),
                    mean_out_degree=optional_float(metadata.get("mean_out_degree")),
                    gpu_algorithm=str(metadata.get("gpu_algorithm", "")),
                )
            )
    return samples


def key_for(sample: Sample) -> tuple[str, str, int, int]:
    return (sample.graph_kind, sample.label, sample.nodes, sample.edges)


def mean_time(samples: list[Sample]) -> float | None:
    valid = [s.per_run_ms for s in samples if s.correct]
    return sum(valid) / len(valid) if valid else None


def first_valid(samples: list[Sample]) -> Sample | None:
    return next((s for s in samples if s.correct), None)


def summarize(samples: Iterable[Sample]) -> list[dict[str, object]]:
    groups: dict[tuple[str, str, int, int], dict[str, list[Sample]]] = defaultdict(lambda: defaultdict(list))
    for sample in samples:
        if sample.variant in {"cpu", "gpu", "gpu-non-naive"}:
            groups[key_for(sample)][sample.variant].append(sample)

    rows: list[dict[str, object]] = []
    for (kind, label, nodes, edges), variants in groups.items():
        cpu = [s for s in variants.get("cpu", []) if s.correct]
        naive = [s for s in variants.get("gpu", []) if s.correct]
        non_naive = [s for s in variants.get("gpu-non-naive", []) if s.correct]
        cpu_ms = mean_time(cpu)
        naive_ms = mean_time(naive)
        non_naive_ms = mean_time(non_naive)
        naive_rep = first_valid(variants.get("gpu", []))
        non_naive_rep = first_valid(variants.get("gpu-non-naive", []))
        representative = first_valid(variants.get("gpu-non-naive", [])) or first_valid(variants.get("gpu", [])) or first_valid(variants.get("cpu", [])) or (variants.get("cpu", []) + variants.get("gpu", []) + variants.get("gpu-non-naive", []))[0]
        naive_speedup = cpu_ms / naive_ms if cpu_ms is not None and naive_ms not in {None, 0.0} else None
        non_naive_speedup = cpu_ms / non_naive_ms if cpu_ms is not None and non_naive_ms not in {None, 0.0} else None
        naive_to_non_naive = naive_ms / non_naive_ms if naive_ms is not None and non_naive_ms not in {None, 0.0} else None
        rows.append(
            {
                "graph_kind": kind,
                "label": label,
                "nodes": nodes,
                "edges": edges,
                "shape": representative.shape,
                "cpu_ms_per_run": cpu_ms,
                # Legacy-compatible name: this is the original naive GPU variant.
                "gpu_naive_ms_per_run": naive_ms,
                "gpu_non_naive_ms_per_run": non_naive_ms,
                "speedup_cpu_over_gpu": naive_speedup,
                "speedup_cpu_over_gpu_naive": naive_speedup,
                "speedup_cpu_over_gpu_non_naive": non_naive_speedup,
                "gpu_naive_to_non_naive_speedup": naive_to_non_naive,
                "gpu_iterations": naive_rep.iterations if naive_rep else None,
                "gpu_naive_iterations": naive_rep.iterations if naive_rep else None,
                "gpu_non_naive_iterations": non_naive_rep.iterations if non_naive_rep else None,
                "gpu_converged": naive_rep.converged if naive_rep else None,
                "gpu_naive_converged": naive_rep.converged if naive_rep else None,
                "gpu_non_naive_converged": non_naive_rep.converged if non_naive_rep else None,
                "component_count": representative.component_count,
                "largest_component_size": representative.largest_component_size,
                "mean_out_degree": representative.mean_out_degree,
                "gpu_edge_iterations": (naive_rep.iterations * edges) if naive_rep and naive_rep.iterations is not None else None,
                "gpu_naive_edge_iterations": (naive_rep.iterations * edges) if naive_rep and naive_rep.iterations is not None else None,
                "gpu_non_naive_edge_iterations": (non_naive_rep.iterations * edges) if non_naive_rep and non_naive_rep.iterations is not None else None,
                "cpu_rows": len(variants.get("cpu", [])),
                "gpu_rows": len(variants.get("gpu", [])),
                "gpu_non_naive_rows": len(variants.get("gpu-non-naive", [])),
                "correct_cpu_rows": len(cpu),
                "correct_gpu_rows": len(naive),
                "correct_gpu_non_naive_rows": len(non_naive),
            }
        )

    def sort_key(row: dict[str, object]) -> tuple[int, int, str]:
        kind = str(row["graph_kind"])
        order = GRAPH_KIND_ORDER.index(kind) if kind in GRAPH_KIND_ORDER else len(GRAPH_KIND_ORDER)
        return (order, int(row["nodes"]), str(row["label"]))

    return sorted(rows, key=sort_key)


def write_csv(rows: list[dict[str, object]], output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / "graph_connected_components_analysis_summary.csv"
    fieldnames = [
        "graph_kind",
        "label",
        "nodes",
        "edges",
        "shape",
        "cpu_ms_per_run",
        "gpu_naive_ms_per_run",
        "gpu_non_naive_ms_per_run",
        "speedup_cpu_over_gpu",
        "speedup_cpu_over_gpu_naive",
        "speedup_cpu_over_gpu_non_naive",
        "gpu_naive_to_non_naive_speedup",
        "gpu_iterations",
        "gpu_naive_iterations",
        "gpu_non_naive_iterations",
        "gpu_converged",
        "gpu_naive_converged",
        "gpu_non_naive_converged",
        "component_count",
        "largest_component_size",
        "mean_out_degree",
        "gpu_edge_iterations",
        "gpu_naive_edge_iterations",
        "gpu_non_naive_edge_iterations",
        "cpu_rows",
        "gpu_rows",
        "gpu_non_naive_rows",
        "correct_cpu_rows",
        "correct_gpu_rows",
        "correct_gpu_non_naive_rows",
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


def write_report(samples: list[Sample], rows: list[dict[str, object]], output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / "graph_connected_components_analysis_report.md"
    invalid = [s for s in samples if s.variant.startswith("gpu") and not s.correct]
    unconverged = [s for s in samples if s.variant.startswith("gpu") and s.converged is False]

    lines: list[str] = []
    lines.append("# Graph Connected Components JSONL Analysis")
    lines.append("")
    lines.append(f"Rows read: {len(samples)}")
    lines.append(f"Matched CPU/GPU size points: {len(rows)}")
    lines.append(f"Invalid GPU rows: {len(invalid)}")
    lines.append(f"Unconverged GPU rows: {len(unconverged)}")
    lines.append("")
    for column, title in [
        ("speedup_cpu_over_gpu_naive", "First naive-GPU crossover by family"),
        ("speedup_cpu_over_gpu_non_naive", "First non-naive-GPU crossover by family"),
    ]:
        lines.append(f"## {title}")
        lines.append("")
        for kind in GRAPH_KIND_ORDER:
            candidates = [r for r in rows if r["graph_kind"] == kind and r.get(column) is not None]
            crossover = next((r for r in candidates if float(r[column]) > 1.0), None)
            if crossover is None:
                lines.append(f"- `{kind}`: no crossover observed in this sweep.")
            else:
                lines.append(
                    f"- `{kind}`: first crossover at `{crossover['label']}` "
                    f"({crossover['nodes']} nodes, {crossover['edges']} edges), "
                    f"speedup={float(crossover[column]):.3f}x."
                )
        lines.append("")
    lines.append("## Summary table")
    lines.append("")
    lines.append("| kind | label | nodes | edges | CPU ms/run | GPU naive ms/run | GPU non-naive ms/run | speedup naive | speedup non-naive | naive/non-naive | it naive | it non-naive |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    for row in rows:
        lines.append(
            f"| {row['graph_kind']} | {row['label']} | {row['nodes']} | {row['edges']} | "
            f"{fmt(row['cpu_ms_per_run'])} | {fmt(row['gpu_naive_ms_per_run'])} | {fmt(row['gpu_non_naive_ms_per_run'])} | "
            f"{fmt(row['speedup_cpu_over_gpu_naive'])} | {fmt(row['speedup_cpu_over_gpu_non_naive'])} | "
            f"{fmt(row['gpu_naive_to_non_naive_speedup'])} | {fmt(row['gpu_naive_iterations'])} | {fmt(row['gpu_non_naive_iterations'])} |"
        )
    lines.append("")
    lines.append("## Interpretation")
    lines.append("")
    lines.append("The CPU baseline is Union-Find. The `gpu` row is the original node-parallel label-propagation baseline.")
    lines.append("The `gpu-non-naive` row uses edge-parallel root hooking plus full pointer-jumping compression.")
    lines.append("The new row is intended to show whether fewer convergence rounds can compensate for the extra work done inside each GPU iteration.")
    lines.append("")
    if invalid:
        lines.append("## Invalid GPU rows")
        lines.append("")
        for sample in invalid[:20]:
            lines.append(f"- {sample.variant} / {sample.graph_kind} / {sample.label}: correct={sample.correct}, converged={sample.converged}")
        if len(invalid) > 20:
            lines.append(f"- ... {len(invalid) - 20} more")
        lines.append("")

    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze graph_connected_components JSONL sweep output.")
    parser.add_argument("--jsonl", required=True)
    parser.add_argument("--output-dir", default="results/graph_connected_components_shape_scale_analysis")
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
