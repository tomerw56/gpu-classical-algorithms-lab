#!/usr/bin/env python3
"""Analyze constraint_network JSONL sweep results."""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path


def optional_int(value: object) -> int | None:
    try:
        return int(value) if value is not None and value != "" else None
    except (TypeError, ValueError):
        return None


def optional_float(value: object) -> float | None:
    try:
        return float(value) if value is not None and value != "" else None
    except (TypeError, ValueError):
        return None


def load_rows(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, start=1):
            line = raw.strip()
            if not line:
                continue
            row = json.loads(line)
            if row.get("benchmark") != "constraint_network":
                continue
            input_size = row.get("input_size") or {}
            metadata = row.get("metadata") or {}
            repeat = int(row.get("repeat", 1))
            rows.append({
                "variant": str(row.get("variant", "")),
                "label": str(row.get("preset", "")),
                "tasks": int(input_size.get("tasks", 0)),
                "resources": int(input_size.get("resources", 0)),
                "candidates": int(input_size.get("candidates", 0)),
                "repeat": repeat,
                "ms_per_run": float(row.get("total_ms", 0.0)) / repeat,
                "correct": bool(row.get("correct", False)),
                "valid_count": optional_int(metadata.get("valid_count")),
                "invalid_count": optional_int(metadata.get("invalid_count")),
                "total_violations": optional_int(metadata.get("total_violations")),
                "skill_violations": optional_int(metadata.get("skill_violations")),
                "capacity_violations": optional_int(metadata.get("capacity_violations")),
                "task_window_violations": optional_int(metadata.get("task_window_violations")),
                "resource_window_violations": optional_int(metadata.get("resource_window_violations")),
                "distance_violations": optional_int(metadata.get("distance_violations")),
                "forbidden_zone_violations": optional_int(metadata.get("forbidden_zone_violations")),
                "risk_violations": optional_int(metadata.get("risk_violations")),
                "max_abs_error": optional_float(metadata.get("max_abs_error")),
            })
    return rows


def summarize(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    groups: dict[tuple[int, int, int, str], dict[str, list[dict[str, object]]]] = defaultdict(lambda: defaultdict(list))
    for row in rows:
        groups[(int(row["tasks"]), int(row["resources"]), int(row["candidates"]), str(row["label"]))][str(row["variant"])].append(row)

    out: list[dict[str, object]] = []
    for (tasks, resources, candidates, label), variants in groups.items():
        cpu = [r for r in variants.get("cpu", []) if r["correct"]]
        gpu = [r for r in variants.get("gpu", []) if r["correct"]]
        cpu_ms = sum(float(r["ms_per_run"]) for r in cpu) / len(cpu) if cpu else None
        gpu_ms = sum(float(r["ms_per_run"]) for r in gpu) / len(gpu) if gpu else None
        speedup = cpu_ms / gpu_ms if cpu_ms is not None and gpu_ms not in {None, 0.0} else None
        representative = (gpu or cpu or variants.get("cpu", []) or variants.get("gpu", []))[0]
        valid_count = representative.get("valid_count")
        total_violations = representative.get("total_violations")
        valid_ratio = (int(valid_count) / candidates) if valid_count is not None and candidates else None
        violations_per_candidate = (int(total_violations) / candidates) if total_violations is not None and candidates else None
        violation_reason_counts = {
            "skill_violations": representative.get("skill_violations"),
            "capacity_violations": representative.get("capacity_violations"),
            "task_window_violations": representative.get("task_window_violations"),
            "resource_window_violations": representative.get("resource_window_violations"),
            "distance_violations": representative.get("distance_violations"),
            "forbidden_zone_violations": representative.get("forbidden_zone_violations"),
            "risk_violations": representative.get("risk_violations"),
        }
        out.append({
            "label": label,
            "tasks": tasks,
            "resources": resources,
            "candidates": candidates,
            "cpu_ms_per_run": cpu_ms,
            "gpu_ms_per_run": gpu_ms,
            "speedup_cpu_over_gpu": speedup,
            "valid_count": valid_count,
            "valid_ratio": valid_ratio,
            "total_violations": total_violations,
            "violations_per_candidate": violations_per_candidate,
            **violation_reason_counts,
            "correct_cpu_rows": len(cpu),
            "correct_gpu_rows": len(gpu),
        })
    return sorted(out, key=lambda r: int(r["candidates"]))


def fmt(value: object) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, float):
        return f"{value:.6f}"
    return str(value)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--jsonl", required=True)
    parser.add_argument("--output-dir", default="results/constraint_network_scale_analysis")
    args = parser.parse_args()

    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    rows = summarize(load_rows(Path(args.jsonl)))

    csv_path = output_dir / "constraint_network_analysis_summary.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()) if rows else ["label"])
        writer.writeheader()
        writer.writerows(rows)

    report_path = output_dir / "constraint_network_analysis_report.md"
    lines = ["# Constraint Network JSONL Analysis", "", f"Matched CPU/GPU size points: {len(rows)}", ""]
    lines += ["## First GPU crossover", ""]
    crossover = next((r for r in rows if r.get("speedup_cpu_over_gpu") is not None and float(r["speedup_cpu_over_gpu"]) > 1.0), None)
    if crossover:
        lines.append(f"GPU first crosses over at `{crossover['label']}` ({crossover['candidates']} candidates), speedup={float(crossover['speedup_cpu_over_gpu']):.3f}x.")
    else:
        lines.append("No GPU crossover observed in this sweep.")
    lines += ["", "## Dominant violation reason", ""]
    reason_fields = [
        ("skill_violations", "skill"),
        ("capacity_violations", "capacity"),
        ("task_window_violations", "task window"),
        ("resource_window_violations", "resource window"),
        ("distance_violations", "distance"),
        ("forbidden_zone_violations", "forbidden zone"),
        ("risk_violations", "risk"),
    ]
    if rows:
        last = rows[-1]
        dominant = max(reason_fields, key=lambda item: int(last.get(item[0]) or 0))
        lines.append(f"At the largest point, the most common violation reason is `{dominant[1]}` with {last.get(dominant[0])} hits.")
    lines += ["", "## Summary table", "", "| label | candidates | CPU ms/run | GPU ms/run | speedup | valid ratio | violations/candidate |", "|---|---:|---:|---:|---:|---:|---:|"]
    for row in rows:
        lines.append(f"| {row['label']} | {row['candidates']} | {fmt(row['cpu_ms_per_run'])} | {fmt(row['gpu_ms_per_run'])} | {fmt(row['speedup_cpu_over_gpu'])} | {fmt(row['valid_ratio'])} | {fmt(row['violations_per_candidate'])} |")
    lines += ["", "## Interpretation", "", "This benchmark is intentionally GPU-friendly: each candidate assignment can be evaluated independently. The GPU should improve as the candidate count grows enough to amortize transfer and launch overhead."]
    report_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote summary CSV: {csv_path}")
    print(f"Wrote report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
