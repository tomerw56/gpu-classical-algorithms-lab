#!/usr/bin/env python3
"""Analyze assignment_preprocessing JSONL sweep results."""
from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path


def fnum(v, default=None):
    if v is None or v == "":
        return default
    try:
        return float(v)
    except Exception:
        return default


def inum(v, default=None):
    if v is None or v == "":
        return default
    try:
        return int(v)
    except Exception:
        return default


def load(path: Path):
    rows = []
    for line_no, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not raw.strip():
            continue
        row = json.loads(raw)
        if row.get("benchmark") != "assignment_preprocessing":
            continue
        meta = row.get("metadata") or {}
        size = row.get("input_size") or {}
        repeat = int(row.get("repeat", 1))
        rows.append({
            "variant": row.get("variant"),
            "label": row.get("preset"),
            "tasks": int(size.get("tasks", 0)),
            "resources": int(size.get("resources", 0)),
            "pairs": int(size.get("pairs", 0)),
            "top_k": int(size.get("top_k", 0)),
            "ms_per_run": float(row.get("total_ms", 0.0)) / max(1, repeat),
            "kernel_ms_per_run": float(row.get("kernel_ms", 0.0)) / max(1, repeat),
            "correct": bool(row.get("correct", False)),
            "feasible_pair_count": inum(meta.get("feasible_pair_count"), 0),
            "selected_candidate_count": inum(meta.get("selected_candidate_count"), 0),
            "tasks_with_any_candidate": inum(meta.get("tasks_with_any_candidate"), 0),
            "candidate_reduction_ratio": fnum(meta.get("candidate_reduction_ratio"), 0.0),
            "mean_selected_cost": fnum(meta.get("mean_selected_cost"), 0.0),
            "skill_violations": inum(meta.get("skill_violations"), 0),
            "capacity_violations": inum(meta.get("capacity_violations"), 0),
            "time_violations": inum(meta.get("time_violations"), 0),
            "distance_violations": inum(meta.get("distance_violations"), 0),
            "zone_violations": inum(meta.get("zone_violations"), 0),
            "risk_violations": inum(meta.get("risk_violations"), 0),
        })
    return rows


def summarize(rows):
    groups = defaultdict(dict)
    for r in rows:
        if r["correct"] and r["variant"] in {"cpu", "gpu"}:
            groups[(r["label"], r["tasks"], r["resources"], r["top_k"])][r["variant"]] = r
    out = []
    for (label, tasks, resources, top_k), variants in groups.items():
        cpu = variants.get("cpu")
        gpu = variants.get("gpu")
        rep = gpu or cpu
        speedup = None
        if cpu and gpu and gpu["ms_per_run"] > 0:
            speedup = cpu["ms_per_run"] / gpu["ms_per_run"]
        out.append({
            "label": label,
            "tasks": tasks,
            "resources": resources,
            "pairs": tasks * resources,
            "top_k": top_k,
            "cpu_ms_per_run": cpu["ms_per_run"] if cpu else None,
            "gpu_ms_per_run": gpu["ms_per_run"] if gpu else None,
            "gpu_kernel_ms_per_run": gpu["kernel_ms_per_run"] if gpu else None,
            "speedup_cpu_over_gpu": speedup,
            "feasible_pair_count": rep["feasible_pair_count"],
            "selected_candidate_count": rep["selected_candidate_count"],
            "tasks_with_any_candidate": rep["tasks_with_any_candidate"],
            "candidate_reduction_ratio": rep["candidate_reduction_ratio"],
            "mean_selected_cost": rep["mean_selected_cost"],
            "skill_violations": rep["skill_violations"],
            "capacity_violations": rep["capacity_violations"],
            "time_violations": rep["time_violations"],
            "distance_violations": rep["distance_violations"],
            "zone_violations": rep["zone_violations"],
            "risk_violations": rep["risk_violations"],
        })
    return sorted(out, key=lambda r: (r["pairs"], r["top_k"], r["label"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jsonl", required=True)
    ap.add_argument("--output-dir", default="results/assignment_preprocessing_analysis")
    args = ap.parse_args()
    outdir = Path(args.output_dir)
    outdir.mkdir(parents=True, exist_ok=True)
    rows = summarize(load(Path(args.jsonl)))
    csv_path = outdir / "assignment_preprocessing_analysis_summary.csv"
    if not rows:
        raise SystemExit("No assignment_preprocessing rows found")
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    report = ["# Assignment Preprocessing Analysis", "", f"Matched CPU/GPU points: {len(rows)}", "", "## First GPU crossover", ""]
    cross = next((r for r in rows if r.get("speedup_cpu_over_gpu") and r["speedup_cpu_over_gpu"] > 1.0), None)
    if cross:
        report.append(f"GPU first wins at `{cross['label']}` ({cross['tasks']} tasks, {cross['resources']} resources, {cross['pairs']} pairs), speedup={cross['speedup_cpu_over_gpu']:.3f}x.")
    else:
        report.append("No GPU crossover observed in this sweep.")
    report += ["", "## Summary", "", "| label | tasks | resources | pairs | top_k | CPU ms/run | GPU ms/run | speedup | reduction ratio |", "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for r in rows:
        report.append(f"| {r['label']} | {r['tasks']} | {r['resources']} | {r['pairs']} | {r['top_k']} | {r['cpu_ms_per_run']:.6f} | {r['gpu_ms_per_run']:.6f} | {r['speedup_cpu_over_gpu']:.6f} | {r['candidate_reduction_ratio']:.8f} |")
    report += ["", "## Interpretation", "", "This benchmark is a preprocessing stage rather than a full assignment solver. The GPU is expected to help when the dense task/resource pair evaluation dominates and the output is reduced to top-K candidates per task."]
    (outdir / "assignment_preprocessing_analysis_report.md").write_text("\n".join(report), encoding="utf-8")
    print(f"Wrote summary CSV: {csv_path}")
    print(f"Wrote report: {outdir / 'assignment_preprocessing_analysis_report.md'}")


if __name__ == "__main__":
    main()
