#!/usr/bin/env python3
"""Analyze Graph BFS JSONL files and diagnose missing/empty plot data.

This tool is intentionally separate from the plotter. Its job is to answer:

- Did the JSONL file actually contain graph_bfs rows?
- Did it contain matched CPU/GPU pairs?
- Which graph kinds and sizes were present?
- Which frontier-anatomy metrics were present exactly in metadata?
- Which metrics can be derived as fallback estimates from older JSONL files?
- Why would a plot such as "useful work per synchronization point" be empty?

It writes a CSV summary and a Markdown report that are easier to inspect than
raw JSONL. The plotter can still be used afterwards to generate PNG files.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from statistics import mean
from typing import Any

GRAPH_KIND_ORDER = ["chain", "grid", "layered", "random"]
EXACT_FRONTIER_KEYS = [
    "frontier_level_count",
    "max_frontier_size",
    "mean_frontier_size",
    "reached_edge_visits",
    "max_frontier_edge_visits",
    "mean_frontier_edge_visits",
    "mean_reached_out_degree",
    "frontier_width_to_depth",
]
FALLBACK_SOURCE_KEYS = ["max_distance", "reached_count", "mean_out_degree", "edge_count"]


@dataclass(frozen=True)
class ParsedRow:
    line_number: int
    variant: str
    graph_kind: str
    nodes: int
    edges: int
    shape: str
    preset: str
    repeat: int
    total_ms: float
    per_run_ms: float
    metadata: dict[str, Any]


@dataclass(frozen=True)
class SummaryRow:
    graph_kind: str
    nodes: int
    edges: int
    shape: str
    preset: str
    cpu_ms_per_run: float | None
    gpu_ms_per_run: float | None
    speedup_cpu_over_gpu: float | None
    max_distance: int | None
    reached_count: int | None
    frontier_level_count: float | None
    max_frontier_size: float | None
    mean_frontier_size: float | None
    reached_edge_visits: float | None
    max_frontier_edge_visits: float | None
    mean_frontier_edge_visits: float | None
    mean_reached_out_degree: float | None
    frontier_width_to_depth: float | None
    frontier_metric_source: str
    cpu_rows: int
    gpu_rows: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Analyze graph_bfs JSONL output and diagnose empty plots.")
    parser.add_argument("--jsonl", required=True, help="Input JSONL result file")
    parser.add_argument(
        "--output-dir",
        default="results/graph_bfs_jsonl_analysis",
        help="Output folder for CSV and Markdown analysis report",
    )
    parser.add_argument(
        "--require-exact-frontier",
        action="store_true",
        help="Return exit code 2 if exact Phase 3.2.4 frontier metrics are missing.",
    )
    return parser.parse_args()


def _as_int(value: Any) -> int | None:
    if value is None or value == "":
        return None
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return None


def _as_float(value: Any) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def normalize_graph_kind(value: Any) -> str:
    kind = str(value or "")
    if kind == "random_sparse":
        return "random"
    return kind


def derive_metrics(metadata: dict[str, Any], nodes: int, edges: int) -> tuple[dict[str, float | int | None], str]:
    """Return exact metrics when present, otherwise fallback estimates.

    The source string is one of:
    - exact: at least one Phase 3.2.4 exact frontier metric is present.
    - fallback_estimate: derived from older fields such as max_distance and reached_count.
    - unavailable: insufficient data.
    """

    exact_present = any(key in metadata and metadata.get(key) not in (None, "") for key in EXACT_FRONTIER_KEYS)

    max_distance = _as_int(metadata.get("max_distance"))
    reached_count = _as_int(metadata.get("reached_count"))

    frontier_level_count: float | None = _as_float(metadata.get("frontier_level_count"))
    if frontier_level_count is None and max_distance is not None and max_distance >= 0:
        frontier_level_count = float(max_distance + 1)

    mean_reached_out_degree = _as_float(metadata.get("mean_reached_out_degree"))
    if mean_reached_out_degree is None:
        mean_reached_out_degree = _as_float(metadata.get("mean_out_degree"))
    if mean_reached_out_degree is None and nodes > 0:
        mean_reached_out_degree = float(edges) / float(nodes)

    reached_edge_visits: float | None = _as_float(metadata.get("reached_edge_visits"))
    if reached_edge_visits is None and reached_count is not None and mean_reached_out_degree is not None:
        reached_edge_visits = float(reached_count) * float(mean_reached_out_degree)

    mean_frontier_size: float | None = _as_float(metadata.get("mean_frontier_size"))
    if mean_frontier_size is None and reached_count is not None and frontier_level_count and frontier_level_count > 0:
        mean_frontier_size = float(reached_count) / float(frontier_level_count)

    mean_frontier_edge_visits: float | None = _as_float(metadata.get("mean_frontier_edge_visits"))
    if mean_frontier_edge_visits is None and reached_edge_visits is not None and frontier_level_count and frontier_level_count > 0:
        mean_frontier_edge_visits = float(reached_edge_visits) / float(frontier_level_count)

    frontier_width_to_depth: float | None = _as_float(metadata.get("frontier_width_to_depth"))
    if frontier_width_to_depth is None and mean_frontier_size is not None and frontier_level_count and frontier_level_count > 0:
        frontier_width_to_depth = mean_frontier_size / float(frontier_level_count)

    metrics: dict[str, float | int | None] = {
        "frontier_level_count": frontier_level_count,
        "max_frontier_size": _as_float(metadata.get("max_frontier_size")),
        "mean_frontier_size": mean_frontier_size,
        "reached_edge_visits": reached_edge_visits,
        "max_frontier_edge_visits": _as_float(metadata.get("max_frontier_edge_visits")),
        "mean_frontier_edge_visits": mean_frontier_edge_visits,
        "mean_reached_out_degree": mean_reached_out_degree,
        "frontier_width_to_depth": frontier_width_to_depth,
    }

    if exact_present:
        return metrics, "exact"
    if mean_frontier_edge_visits is not None or mean_frontier_size is not None:
        return metrics, "fallback_estimate"
    return metrics, "unavailable"


def load_rows(jsonl_path: Path) -> tuple[list[ParsedRow], Counter[str], Counter[str], set[str]]:
    rows: list[ParsedRow] = []
    benchmarks: Counter[str] = Counter()
    variants: Counter[str] = Counter()
    metadata_keys: set[str] = set()

    with jsonl_path.open("r", encoding="utf-8") as handle:
        for line_number, raw_line in enumerate(handle, start=1):
            line = raw_line.strip()
            if not line:
                continue
            data = json.loads(line)
            benchmarks[str(data.get("benchmark", ""))] += 1
            variant = str(data.get("variant", ""))
            variants[variant] += 1

            if data.get("benchmark") != "graph_bfs":
                continue
            if not data.get("correct", False):
                continue
            if variant not in {"cpu", "gpu"}:
                continue

            input_size = data.get("input_size") or {}
            metadata = data.get("metadata") or {}
            metadata_keys.update(str(k) for k in metadata.keys())
            graph_kind = normalize_graph_kind(metadata.get("graph_kind"))
            if graph_kind not in GRAPH_KIND_ORDER:
                continue

            nodes = int(input_size.get("nodes"))
            edges = int(input_size.get("edges"))
            repeat = int(data.get("repeat", 1))
            total_ms = float(data.get("total_ms", 0.0))
            per_run_ms = total_ms / float(repeat if repeat > 0 else 1)
            rows.append(
                ParsedRow(
                    line_number=line_number,
                    variant=variant,
                    graph_kind=graph_kind,
                    nodes=nodes,
                    edges=edges,
                    shape=str(metadata.get("shape", "")),
                    preset=str(data.get("preset", "")),
                    repeat=repeat,
                    total_ms=total_ms,
                    per_run_ms=per_run_ms,
                    metadata=metadata,
                )
            )

    return rows, benchmarks, variants, metadata_keys


def summarize(rows: list[ParsedRow]) -> list[SummaryRow]:
    grouped: dict[tuple[str, int, int, str], dict[str, list[ParsedRow]]] = defaultdict(lambda: {"cpu": [], "gpu": []})
    for row in rows:
        grouped[(row.graph_kind, row.nodes, row.edges, row.shape)][row.variant].append(row)

    output: list[SummaryRow] = []
    for (graph_kind, nodes, edges, shape), variants in grouped.items():
        cpu = variants["cpu"]
        gpu = variants["gpu"]
        cpu_ms = mean([row.per_run_ms for row in cpu]) if cpu else None
        gpu_ms = mean([row.per_run_ms for row in gpu]) if gpu else None
        speedup = cpu_ms / gpu_ms if cpu_ms is not None and gpu_ms is not None and gpu_ms > 0 else None
        representative = cpu[0] if cpu else (gpu[0] if gpu else None)
        metrics: dict[str, float | int | None] = {}
        source = "unavailable"
        if representative is not None:
            metrics, source = derive_metrics(representative.metadata, representative.nodes, representative.edges)
        output.append(
            SummaryRow(
                graph_kind=graph_kind,
                nodes=nodes,
                edges=edges,
                shape=shape,
                preset=representative.preset if representative else "",
                cpu_ms_per_run=cpu_ms,
                gpu_ms_per_run=gpu_ms,
                speedup_cpu_over_gpu=speedup,
                max_distance=_as_int(representative.metadata.get("max_distance")) if representative else None,
                reached_count=_as_int(representative.metadata.get("reached_count")) if representative else None,
                frontier_level_count=metrics.get("frontier_level_count"),
                max_frontier_size=metrics.get("max_frontier_size"),
                mean_frontier_size=metrics.get("mean_frontier_size"),
                reached_edge_visits=metrics.get("reached_edge_visits"),
                max_frontier_edge_visits=metrics.get("max_frontier_edge_visits"),
                mean_frontier_edge_visits=metrics.get("mean_frontier_edge_visits"),
                mean_reached_out_degree=metrics.get("mean_reached_out_degree"),
                frontier_width_to_depth=metrics.get("frontier_width_to_depth"),
                frontier_metric_source=source,
                cpu_rows=len(cpu),
                gpu_rows=len(gpu),
            )
        )

    output.sort(key=lambda row: (GRAPH_KIND_ORDER.index(row.graph_kind), row.nodes, row.edges, row.shape))
    return output


def fmt(value: float | int | None) -> str:
    if value is None:
        return ""
    if isinstance(value, int):
        return str(value)
    if abs(value) >= 1000.0:
        return f"{value:.3f}"
    return f"{value:.9f}"


def write_csv(rows: list[SummaryRow], output_dir: Path) -> Path:
    path = output_dir / "graph_bfs_jsonl_analysis_summary.csv"
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow([
            "graph_kind", "nodes", "edges", "shape", "preset", "max_distance", "reached_count",
            "frontier_level_count", "max_frontier_size", "mean_frontier_size", "reached_edge_visits",
            "max_frontier_edge_visits", "mean_frontier_edge_visits", "mean_reached_out_degree",
            "frontier_width_to_depth", "frontier_metric_source", "cpu_ms_per_run", "gpu_ms_per_run",
            "speedup_cpu_over_gpu", "cpu_rows", "gpu_rows",
        ])
        for row in rows:
            writer.writerow([
                row.graph_kind, row.nodes, row.edges, row.shape, row.preset,
                fmt(row.max_distance), fmt(row.reached_count), fmt(row.frontier_level_count),
                fmt(row.max_frontier_size), fmt(row.mean_frontier_size), fmt(row.reached_edge_visits),
                fmt(row.max_frontier_edge_visits), fmt(row.mean_frontier_edge_visits),
                fmt(row.mean_reached_out_degree), fmt(row.frontier_width_to_depth), row.frontier_metric_source,
                fmt(row.cpu_ms_per_run), fmt(row.gpu_ms_per_run), fmt(row.speedup_cpu_over_gpu),
                row.cpu_rows, row.gpu_rows,
            ])
    return path


def build_diagnosis(rows: list[SummaryRow], metadata_keys: set[str]) -> list[str]:
    messages: list[str] = []
    if not rows:
        messages.append("No matched, correct graph_bfs rows were found. Timing and anatomy plots will be empty.")
        return messages

    paired = [row for row in rows if row.cpu_ms_per_run is not None and row.gpu_ms_per_run is not None]
    if not paired:
        messages.append("No CPU/GPU pairs were found for the same graph size and shape. Speedup plots will be empty.")
    else:
        messages.append(f"Found {len(paired)} matched CPU/GPU graph_bfs size points.")

    exact_keys_present = sorted(key for key in EXACT_FRONTIER_KEYS if key in metadata_keys)
    if exact_keys_present:
        messages.append("Exact frontier-anatomy metadata exists: " + ", ".join(exact_keys_present))
    else:
        messages.append(
            "Exact Phase 3.2.4 frontier-anatomy metadata is missing. "
            "This usually means the executable that generated the JSONL was built before the frontier-anatomy patch, "
            "or the project was not rebuilt after applying it."
        )

    fallback_rows = [row for row in rows if row.frontier_metric_source == "fallback_estimate"]
    if fallback_rows:
        messages.append(
            f"Fallback frontier-work estimates were derived for {len(fallback_rows)} points. "
            "These are sufficient for the 'useful work per synchronization point' plot, but they are estimates."
        )

    if not any(row.mean_frontier_edge_visits is not None for row in rows):
        messages.append(
            "No exact or derived mean_frontier_edge_visits values are available. "
            "The useful-work plot will be empty. Required fallback inputs are max_distance, reached_count, and mean_out_degree or edge_count."
        )
    else:
        messages.append("The useful-work metric is available, so graph_bfs_shape_scaling_mean_edges_per_level.png should not be empty.")

    if not any(row.max_frontier_size is not None for row in rows):
        messages.append(
            "max_frontier_size is unavailable. The max-frontier plot is expected to be skipped or empty for old JSONL files. "
            "This metric cannot be reconstructed exactly from the older result files."
        )

    kinds = sorted({row.graph_kind for row in rows}, key=lambda k: GRAPH_KIND_ORDER.index(k))
    messages.append("Graph kinds present: " + ", ".join(kinds))
    if kinds == ["layered"]:
        messages.append("This is a layered-only sweep. Shape-comparison plots will show only the layered series, not all four graph kinds.")

    return messages


def write_report(
    jsonl_path: Path,
    output_dir: Path,
    rows: list[SummaryRow],
    benchmarks: Counter[str],
    variants: Counter[str],
    metadata_keys: set[str],
    csv_path: Path,
) -> Path:
    report_path = output_dir / "graph_bfs_jsonl_analysis_report.md"
    diagnosis = build_diagnosis(rows, metadata_keys)
    with report_path.open("w", encoding="utf-8") as handle:
        handle.write("# Graph BFS JSONL Analysis Report\n\n")
        handle.write(f"Input file: `{jsonl_path}`\n\n")
        handle.write("## Row inventory\n\n")
        handle.write(f"Benchmarks: `{dict(benchmarks)}`\n\n")
        handle.write(f"Variants: `{dict(variants)}`\n\n")
        handle.write(f"Matched graph_bfs size points: `{len(rows)}`\n\n")
        handle.write("## Metadata keys found\n\n")
        if metadata_keys:
            handle.write(", ".join(f"`{key}`" for key in sorted(metadata_keys)))
        else:
            handle.write("No metadata keys found.")
        handle.write("\n\n")
        handle.write("## Diagnosis\n\n")
        for message in diagnosis:
            handle.write(f"- {message}\n")
        handle.write("\n")
        handle.write("## Summary table\n\n")
        handle.write("| kind | nodes | edges | depth | metric source | mean edges / level | CPU ms/run | GPU ms/run | speedup |\n")
        handle.write("|---|---:|---:|---:|---|---:|---:|---:|---:|\n")
        for row in rows:
            handle.write(
                f"| {row.graph_kind} | {row.nodes} | {row.edges} | {fmt(row.max_distance) or 'n/a'} | "
                f"{row.frontier_metric_source} | {fmt(row.mean_frontier_edge_visits) or 'n/a'} | "
                f"{fmt(row.cpu_ms_per_run) or 'n/a'} | {fmt(row.gpu_ms_per_run) or 'n/a'} | "
                f"{fmt(row.speedup_cpu_over_gpu) or 'n/a'} |\n"
            )
        handle.write("\n")
        handle.write(f"CSV summary: `{csv_path}`\n")
    return report_path


def print_console(rows: list[SummaryRow], diagnosis: list[str], csv_path: Path, report_path: Path) -> None:
    print("Graph BFS JSONL analysis")
    print("=========================")
    for message in diagnosis:
        print(f"- {message}")
    print()
    print("kind       nodes        edges         depth   source             mean_edges/level   CPU ms/run   GPU ms/run   speedup")
    print("----------------------------------------------------------------------------------------------------------------")
    for row in rows:
        speedup = "n/a" if row.speedup_cpu_over_gpu is None else f"{row.speedup_cpu_over_gpu:.4f}x"
        print(
            f"{row.graph_kind:<10} {row.nodes:<12d} {row.edges:<13d} "
            f"{fmt(row.max_distance) or 'n/a':>7} {row.frontier_metric_source:<18} "
            f"{fmt(row.mean_frontier_edge_visits) or 'n/a':>17} "
            f"{fmt(row.cpu_ms_per_run) or 'n/a':>12} {fmt(row.gpu_ms_per_run) or 'n/a':>12} {speedup:>10}"
        )
    print()
    print(f"Wrote CSV summary: {csv_path}")
    print(f"Wrote Markdown report: {report_path}")


def main() -> int:
    args = parse_args()
    jsonl_path = Path(args.jsonl)
    if not jsonl_path.exists():
        raise FileNotFoundError(f"JSONL file does not exist: {jsonl_path}")

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    rows, benchmarks, variants, metadata_keys = load_rows(jsonl_path)
    summary = summarize(rows)
    csv_path = write_csv(summary, output_dir)
    report_path = write_report(jsonl_path, output_dir, summary, benchmarks, variants, metadata_keys, csv_path)
    diagnosis = build_diagnosis(summary, metadata_keys)
    print_console(summary, diagnosis, csv_path, report_path)

    exact_missing = not any(key in metadata_keys for key in EXACT_FRONTIER_KEYS)
    if args.require_exact_frontier and exact_missing:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
