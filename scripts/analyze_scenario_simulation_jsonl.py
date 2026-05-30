#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path


def fnum(v):
    if v is None or v == "":
        return None
    return float(v)


def inum(v):
    if v is None or v == "":
        return None
    return int(v)


def load_rows(path: Path):
    rows = []
    with path.open("r", encoding="utf-8") as handle:
        for raw in handle:
            raw = raw.strip()
            if not raw:
                continue
            row = json.loads(raw)
            if row.get("benchmark") != "scenario_simulation":
                continue
            meta = row.get("metadata") or {}
            size = row.get("input_size") or {}
            repeat = int(row.get("repeat", 1))
            rows.append({
                "variant": row.get("variant", ""),
                "label": row.get("preset", ""),
                "tasks": int(size.get("tasks", 0)),
                "resources": int(size.get("resources", 0)),
                "scenarios": int(size.get("scenarios", 0)),
                "repeat": repeat,
                "ms_per_run": float(row.get("total_ms", 0.0)) / max(1, repeat),
                "kernel_ms_per_run": float(row.get("kernel_ms", 0.0)) / max(1, repeat),
                "h2d_ms": float(row.get("h2d_ms", 0.0)),
                "d2h_ms": float(row.get("d2h_ms", 0.0)),
                "correct": bool(row.get("correct", False)),
                "correctness_semantics": meta.get("correctness_semantics"),
                "feasibility_status": meta.get("feasibility_status"),
                "feasible_scenarios": inum(meta.get("feasible_scenarios")),
                "infeasible_scenarios": inum(meta.get("infeasible_scenarios")),
                "failure_scenarios": inum(meta.get("failure_scenarios")),
                "capacity_scenarios": inum(meta.get("capacity_scenarios")),
                "risk_scenarios": inum(meta.get("risk_scenarios")),
                "delay_scenarios": inum(meta.get("delay_scenarios")),
                "total_violations": inum(meta.get("total_violations")),
                "mean_score": fnum(meta.get("mean_score")),
                "best_score": fnum(meta.get("best_score")),
                "worst_score": fnum(meta.get("worst_score")),
                "robustness_score": fnum(meta.get("robustness_score")),
                "max_abs_error": fnum(meta.get("max_abs_error")),
                "max_rel_error": fnum(meta.get("max_rel_error")),
            })
    return rows


def summarize(rows):
    groups = defaultdict(dict)
    for row in rows:
        groups[(row["label"], row["tasks"], row["resources"], row["scenarios"])][row["variant"]] = row
    out = []
    for (label, tasks, resources, scenarios), variants in groups.items():
        cpu = variants.get("cpu")
        gpu = variants.get("gpu")
        speedup = None
        if cpu and gpu and gpu["ms_per_run"] > 0 and cpu["correct"] and gpu["correct"]:
            speedup = cpu["ms_per_run"] / gpu["ms_per_run"]
        rep = gpu or cpu or {}
        feasible = rep.get("feasible_scenarios") or 0
        infeasible = rep.get("infeasible_scenarios") or 0
        total = feasible + infeasible
        out.append({
            "label": label,
            "tasks": tasks,
            "resources": resources,
            "scenarios": scenarios,
            "cpu_ms_per_run": cpu["ms_per_run"] if cpu else None,
            "gpu_ms_per_run": gpu["ms_per_run"] if gpu else None,
            "gpu_kernel_ms_per_run": gpu["kernel_ms_per_run"] if gpu else None,
            "speedup_cpu_over_gpu": speedup,
            "feasible_ratio": feasible / total if total else None,
            "infeasible_ratio": infeasible / total if total else None,
            "feasibility_status": rep.get("feasibility_status"),
            "correctness_semantics": rep.get("correctness_semantics"),
            "failure_scenarios": rep.get("failure_scenarios"),
            "capacity_scenarios": rep.get("capacity_scenarios"),
            "risk_scenarios": rep.get("risk_scenarios"),
            "delay_scenarios": rep.get("delay_scenarios"),
            "mean_score": rep.get("mean_score"),
            "best_score": rep.get("best_score"),
            "worst_score": rep.get("worst_score"),
            "robustness_score": rep.get("robustness_score"),
            "max_abs_error": gpu.get("max_abs_error") if gpu else None,
            "max_rel_error": gpu.get("max_rel_error") if gpu else None,
            "correct_cpu": cpu["correct"] if cpu else None,
            "correct_gpu": gpu["correct"] if gpu else None,
        })
    return sorted(out, key=lambda r: r["scenarios"])


def write_csv(rows, output_dir: Path):
    output_dir.mkdir(parents=True, exist_ok=True)
    path = output_dir / "scenario_simulation_analysis_summary.csv"
    fields = [
        "label","tasks","resources","scenarios","cpu_ms_per_run","gpu_ms_per_run","gpu_kernel_ms_per_run",
        "speedup_cpu_over_gpu","feasible_ratio","infeasible_ratio","feasibility_status","correctness_semantics","failure_scenarios","capacity_scenarios",
        "risk_scenarios","delay_scenarios","mean_score","best_score","worst_score","robustness_score",
        "max_abs_error","max_rel_error","correct_cpu","correct_gpu",
    ]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    return path


def write_report(rows, output_dir: Path):
    path = output_dir / "scenario_simulation_analysis_report.md"
    lines = ["# Scenario Simulation Analysis", ""]
    lines.append(f"Matched CPU/GPU points: {len(rows)}")
    lines.append("")
    crossover = next((r for r in rows if r.get("speedup_cpu_over_gpu") and r["speedup_cpu_over_gpu"] > 1.0), None)
    if crossover:
        lines.append(f"First GPU crossover: `{crossover['label']}` ({crossover['scenarios']} scenarios), speedup={crossover['speedup_cpu_over_gpu']:.3f}x.")
    else:
        lines.append("No GPU crossover observed in this sweep.")
    lines.append("")
    lines.append("| label | scenarios | CPU ms/run | GPU ms/run | speedup | feasible ratio | robustness score |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|")
    for r in rows:
        lines.append(
            f"| {r['label']} | {r['scenarios']} | {r.get('cpu_ms_per_run')} | {r.get('gpu_ms_per_run')} | "
            f"{r.get('speedup_cpu_over_gpu')} | {r.get('feasible_ratio')} | {r.get('robustness_score')} |"
        )
    lines.append("")
    lines.append("## Correctness semantics")
    lines.append("")
    lines.append("The benchmark `correct` flag means CPU/GPU agreement on scenario outcomes and scores. It does not mean the plan is feasible, robust, or operationally good.")
    lines.append("")

    ratios = [r.get("feasible_ratio") for r in rows if r.get("feasible_ratio") is not None]
    if ratios and all(float(r) == 0.0 for r in ratios):
        lines.append("## Feasibility warning")
        lines.append("")
        lines.append("All measured points have feasible ratio 0.0. This is valid for performance testing but weak for robustness interpretation because the generated plan fails every scenario.")
        lines.append("")
    elif ratios and all(float(r) == 1.0 for r in ratios):
        lines.append("## Feasibility warning")
        lines.append("")
        lines.append("All measured points have feasible ratio 1.0. This is valid for performance testing but weak for robustness interpretation because no scenario stresses the plan enough to fail.")
        lines.append("")
    elif ratios:
        lines.append("## Feasibility calibration")
        lines.append("")
        lines.append(f"Feasible ratios range from {min(ratios):.3f} to {max(ratios):.3f}, so the sweep contains both acceptable and failing scenario outcomes.")
        lines.append("")

    lines.append("## Interpretation")
    lines.append("")
    lines.append("This workload evaluates the same plan under many independent uncertainty scenarios. It is expected to become GPU-friendly once the scenario count is large enough to amortize transfer and launch overhead.")
    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--jsonl", required=True)
    ap.add_argument("--output-dir", default="results/scenario_simulation_analysis")
    args = ap.parse_args()
    rows = summarize(load_rows(Path(args.jsonl)))
    out = Path(args.output_dir)
    csv_path = write_csv(rows, out)
    report_path = write_report(rows, out)
    print(f"Wrote summary CSV: {csv_path}")
    print(f"Wrote report: {report_path}")

if __name__ == "__main__":
    main()
