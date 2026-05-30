#!/usr/bin/env python3
"""Analyze combination_finder JSONL sweep output."""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path


def load_rows(path: Path) -> list[dict]:
    rows = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            row = json.loads(line)
            if row.get("benchmark") == "combination_finder":
                rows.append(row)
    return rows


def summarize(rows: list[dict]) -> list[dict]:
    groups: dict[tuple[str, int, int, int], dict[str, list[dict]]] = defaultdict(lambda: defaultdict(list))
    for row in rows:
        inp = row.get("input_size", {})
        key = (str(row.get("preset", "")), int(inp.get("items", 0)), int(inp.get("k", 0)), int(inp.get("combinations", 0)))
        groups[key][str(row.get("variant", ""))].append(row)

    out = []
    for (label, items, k, combos), variants in groups.items():
        cpu = [r for r in variants.get("cpu", []) if r.get("correct")]
        gpu = [r for r in variants.get("gpu", []) if r.get("correct")]
        cpu_ms = sum(float(r.get("total_ms", 0.0)) / max(1, int(r.get("repeat", 1))) for r in cpu) / len(cpu) if cpu else None
        gpu_ms = sum(float(r.get("total_ms", 0.0)) / max(1, int(r.get("repeat", 1))) for r in gpu) / len(gpu) if gpu else None
        speedup = cpu_ms / gpu_ms if cpu_ms is not None and gpu_ms not in (None, 0.0) else None
        rep = (gpu or cpu or variants.get("cpu", []) or variants.get("gpu", []))[0]
        meta = rep.get("metadata", {}) or {}
        valid = int(meta.get("valid_count", 0)) if meta.get("valid_count") is not None else 0
        invalid = int(meta.get("invalid_count", 0)) if meta.get("invalid_count") is not None else max(0, combos - valid)
        out.append({
            "label": label,
            "items": items,
            "k": k,
            "combinations": combos,
            "cpu_ms_per_run": cpu_ms,
            "gpu_ms_per_run": gpu_ms,
            "speedup_cpu_over_gpu": speedup,
            "valid_count": valid,
            "invalid_count": invalid,
            "valid_ratio": valid / combos if combos else 0.0,
            "budget_violations": int(meta.get("budget_violations", 0) or 0),
            "risk_violations": int(meta.get("risk_violations", 0) or 0),
            "coverage_violations": int(meta.get("coverage_violations", 0) or 0),
            "spread_violations": int(meta.get("spread_violations", 0) or 0),
            "best_score": int(meta.get("best_score", 0) or 0),
        })
    return sorted(out, key=lambda r: (r["combinations"], r["label"]))


def write_outputs(summary: list[dict], output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    csv_path = output_dir / "combination_finder_analysis_summary.csv"
    fields = ["label", "items", "k", "combinations", "cpu_ms_per_run", "gpu_ms_per_run", "speedup_cpu_over_gpu", "valid_count", "invalid_count", "valid_ratio", "budget_violations", "risk_violations", "coverage_violations", "spread_violations", "best_score"]
    with csv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in summary:
            writer.writerow(row)

    lines = ["# Combination Finder JSONL Analysis", "", f"Matched CPU/GPU size points: {len(summary)}", "", "## First GPU crossover", ""]
    crossover = next((r for r in summary if r["speedup_cpu_over_gpu"] is not None and r["speedup_cpu_over_gpu"] > 1.0), None)
    if crossover:
        lines.append(f"GPU first crosses over at `{crossover['label']}` with {crossover['combinations']} combinations, speedup={crossover['speedup_cpu_over_gpu']:.3f}x.")
    else:
        lines.append("No GPU crossover observed in this sweep.")
    lines.extend(["", "## Summary table", "", "| label | items | k | combinations | CPU ms/run | GPU ms/run | speedup | valid ratio |", "|---|---:|---:|---:|---:|---:|---:|---:|"])
    for r in summary:
        def f(v):
            if v is None:
                return "n/a"
            if isinstance(v, float):
                return f"{v:.6f}"
            return str(v)
        lines.append(f"| {r['label']} | {r['items']} | {r['k']} | {r['combinations']} | {f(r['cpu_ms_per_run'])} | {f(r['gpu_ms_per_run'])} | {f(r['speedup_cpu_over_gpu'])} | {f(r['valid_ratio'])} |")
    (output_dir / "combination_finder_analysis_report.md").write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {csv_path}")
    print(f"Wrote {output_dir / 'combination_finder_analysis_report.md'}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--jsonl", required=True)
    parser.add_argument("--output-dir", default="results/combination_finder_analysis")
    args = parser.parse_args()
    rows = load_rows(Path(args.jsonl))
    summary = summarize(rows)
    write_outputs(summary, Path(args.output_dir))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
