#!/usr/bin/env python3
"""Create richer diagnostic plots for constraint_network sweep JSONL.

This script complements `plot_constraint_network_scaling.py`.  The scaling plot
answers "when is GPU faster?".  This diagnostic plot answers "what is the data
made of?" by visualizing validity ratios, violation reasons, penalty validation
error, and GPU timing breakdown.
"""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

VIOLATION_FIELDS = [
    ("skill_violations", "skill"),
    ("capacity_violations", "capacity"),
    ("task_window_violations", "task window"),
    ("resource_window_violations", "resource window"),
    ("distance_violations", "distance"),
    ("forbidden_zone_violations", "forbidden zone"),
    ("risk_violations", "risk"),
]


@dataclass(frozen=True)
class Sample:
    variant: str
    label: str
    tasks: int
    resources: int
    candidates: int
    repeat: int
    total_ms_per_run: float
    h2d_ms: float
    kernel_ms_per_run: float
    d2h_ms: float
    correct: bool
    metadata: dict[str, str]


def optional_int(value: object, fallback: int = 0) -> int:
    try:
        return int(value) if value is not None and value != "" else fallback
    except (TypeError, ValueError):
        return fallback


def optional_float(value: object, fallback: float = 0.0) -> float:
    try:
        return float(value) if value is not None and value != "" else fallback
    except (TypeError, ValueError):
        return fallback


def load_samples(path: Path) -> list[Sample]:
    samples: list[Sample] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, start=1):
            line = raw.strip()
            if not line:
                continue
            row = json.loads(line)
            if row.get("benchmark") != "constraint_network":
                continue
            input_size = row.get("input_size") or {}
            metadata_raw = row.get("metadata") or {}
            metadata = {str(k): str(v) for k, v in metadata_raw.items()}
            repeat = int(row.get("repeat", 1))
            if repeat <= 0:
                raise ValueError(f"Invalid repeat count on line {line_no}: {repeat}")
            samples.append(
                Sample(
                    variant=str(row.get("variant", "")),
                    label=str(row.get("preset", "")),
                    tasks=int(input_size.get("tasks", 0)),
                    resources=int(input_size.get("resources", 0)),
                    candidates=int(input_size.get("candidates", 0)),
                    repeat=repeat,
                    total_ms_per_run=float(row.get("total_ms", 0.0)) / repeat,
                    h2d_ms=float(row.get("h2d_ms", 0.0)),
                    kernel_ms_per_run=float(row.get("kernel_ms", 0.0)) / repeat,
                    d2h_ms=float(row.get("d2h_ms", 0.0)),
                    correct=bool(row.get("correct", False)),
                    metadata=metadata,
                )
            )
    return samples


def group_samples(samples: Iterable[Sample]) -> list[dict[str, object]]:
    groups: dict[tuple[int, int, int, str], dict[str, list[Sample]]] = defaultdict(lambda: defaultdict(list))
    for sample in samples:
        if sample.variant in {"cpu", "gpu"}:
            groups[(sample.tasks, sample.resources, sample.candidates, sample.label)][sample.variant].append(sample)

    rows: list[dict[str, object]] = []
    for (tasks, resources, candidates, label), variants in groups.items():
        cpu = [s for s in variants.get("cpu", []) if s.correct]
        gpu = [s for s in variants.get("gpu", []) if s.correct]
        representative = (gpu or cpu or variants.get("cpu", []) or variants.get("gpu", []))[0]
        metadata = representative.metadata
        cpu_ms = sum(s.total_ms_per_run for s in cpu) / len(cpu) if cpu else None
        gpu_ms = sum(s.total_ms_per_run for s in gpu) / len(gpu) if gpu else None
        gpu_h2d = sum(s.h2d_ms for s in gpu) / len(gpu) if gpu else None
        gpu_kernel = sum(s.kernel_ms_per_run for s in gpu) / len(gpu) if gpu else None
        gpu_d2h = sum(s.d2h_ms for s in gpu) / len(gpu) if gpu else None
        speedup = cpu_ms / gpu_ms if cpu_ms is not None and gpu_ms not in {None, 0.0} else None
        valid_count = optional_int(metadata.get("valid_count"))
        invalid_count = optional_int(metadata.get("invalid_count"))
        total_violations = optional_int(metadata.get("total_violations"))
        row: dict[str, object] = {
            "label": label,
            "tasks": tasks,
            "resources": resources,
            "candidates": candidates,
            "cpu_ms_per_run": cpu_ms,
            "gpu_ms_per_run": gpu_ms,
            "gpu_h2d_ms": gpu_h2d,
            "gpu_kernel_ms_per_run": gpu_kernel,
            "gpu_d2h_ms": gpu_d2h,
            "speedup_cpu_over_gpu": speedup,
            "valid_count": valid_count,
            "invalid_count": invalid_count,
            "valid_ratio": valid_count / candidates if candidates else None,
            "invalid_ratio": invalid_count / candidates if candidates else None,
            "total_violations": total_violations,
            "violations_per_candidate": total_violations / candidates if candidates else None,
            "max_abs_error": optional_float(metadata.get("max_abs_error")),
            "max_rel_error": optional_float(metadata.get("max_rel_error")),
        }
        for field, _label in VIOLATION_FIELDS:
            count = optional_int(metadata.get(field))
            row[field] = count
            row[f"{field}_per_candidate"] = count / candidates if candidates else None
        rows.append(row)
    return sorted(rows, key=lambda row: int(row["candidates"]))


def write_summary_csv(rows: list[dict[str, object]], output_dir: Path) -> Path:
    path = output_dir / "constraint_network_diagnostics_summary.csv"
    if not rows:
        path.write_text("", encoding="utf-8")
        return path
    fieldnames = list(rows[0].keys())
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    return path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot constraint_network diagnostic results.")
    parser.add_argument("--jsonl", required=True)
    parser.add_argument("--output-dir", default="results/constraint_network_diagnostics")
    parser.add_argument("--show", action="store_true")
    return parser.parse_args()


def require_matplotlib():
    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise SystemExit(f"matplotlib is required for plotting: {exc}") from exc
    return plt


def plot_valid_invalid(rows: list[dict[str, object]], output_dir: Path) -> Path:
    plt = require_matplotlib()
    x = [int(r["candidates"]) for r in rows]
    valid = [float(r["valid_ratio"] or 0.0) for r in rows]
    invalid = [float(r["invalid_ratio"] or 0.0) for r in rows]
    fig, ax = plt.subplots(figsize=(10, 5.5))
    ax.stackplot(x, valid, invalid, labels=["valid", "invalid"], alpha=0.8)
    ax.set_xscale("log")
    ax.set_ylim(0.0, 1.0)
    ax.set_xlabel("candidate assignments")
    ax.set_ylabel("fraction of candidates")
    ax.set_title("Constraint-network validity mix")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(loc="upper right")
    fig.tight_layout()
    path = output_dir / "constraint_network_valid_invalid_ratio.png"
    fig.savefig(path, dpi=150)
    return path


def plot_violation_reasons(rows: list[dict[str, object]], output_dir: Path) -> Path:
    plt = require_matplotlib()
    x = [int(r["candidates"]) for r in rows]
    series = []
    labels = []
    for field, label in VIOLATION_FIELDS:
        key = f"{field}_per_candidate"
        values = [float(r.get(key) or 0.0) for r in rows]
        if any(v != 0.0 for v in values):
            series.append(values)
            labels.append(label)
    fig, ax = plt.subplots(figsize=(11, 6))
    if series:
        ax.stackplot(x, *series, labels=labels, alpha=0.85)
    ax.set_xscale("log")
    ax.set_xlabel("candidate assignments")
    ax.set_ylabel("violations per candidate")
    ax.set_title("Constraint-network violation reasons")
    ax.grid(True, which="both", alpha=0.3)
    if labels:
        ax.legend(loc="upper left", fontsize="small", ncol=2)
    fig.tight_layout()
    path = output_dir / "constraint_network_violation_reasons.png"
    fig.savefig(path, dpi=150)
    return path


def plot_gpu_breakdown(rows: list[dict[str, object]], output_dir: Path) -> Path:
    plt = require_matplotlib()
    x = [int(r["candidates"]) for r in rows]
    h2d = [float(r.get("gpu_h2d_ms") or 0.0) for r in rows]
    kernel = [float(r.get("gpu_kernel_ms_per_run") or 0.0) for r in rows]
    d2h = [float(r.get("gpu_d2h_ms") or 0.0) for r in rows]
    fig, ax = plt.subplots(figsize=(10, 5.5))
    ax.stackplot(x, h2d, kernel, d2h, labels=["H2D input copy", "kernel per run", "D2H output copy"], alpha=0.85)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("candidate assignments")
    ax.set_ylabel("milliseconds")
    ax.set_title("Constraint-network GPU time breakdown")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(loc="upper left")
    fig.tight_layout()
    path = output_dir / "constraint_network_gpu_time_breakdown.png"
    fig.savefig(path, dpi=150)
    return path


def plot_penalty_error(rows: list[dict[str, object]], output_dir: Path) -> Path:
    plt = require_matplotlib()
    x = [int(r["candidates"]) for r in rows]
    abs_err = [float(r.get("max_abs_error") or 0.0) for r in rows]
    rel_err = [float(r.get("max_rel_error") or 0.0) for r in rows]
    fig, ax1 = plt.subplots(figsize=(10, 5.5))
    ax1.plot(x, abs_err, marker="o", label="max abs error")
    ax1.set_xscale("log")
    ax1.set_yscale("log")
    ax1.set_xlabel("candidate assignments")
    ax1.set_ylabel("max absolute penalty error")
    ax2 = ax1.twinx()
    ax2.plot(x, rel_err, marker="s", linestyle="--", label="max rel error")
    ax2.set_yscale("log")
    ax2.set_ylabel("max relative penalty error")
    ax1.set_title("Constraint-network CPU/GPU penalty validation error")
    ax1.grid(True, which="both", alpha=0.3)
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc="upper left")
    fig.tight_layout()
    path = output_dir / "constraint_network_penalty_error.png"
    fig.savefig(path, dpi=150)
    return path


def write_report(rows: list[dict[str, object]], output_dir: Path) -> Path:
    path = output_dir / "constraint_network_diagnostics_report.md"
    lines: list[str] = ["# Constraint Network Diagnostics", ""]
    lines.append(f"Rows summarized: {len(rows)}")
    if rows:
        first = rows[0]
        last = rows[-1]
        lines.append("")
        lines.append("## Validity mix")
        lines.append("")
        lines.append(f"Smallest point `{first['label']}` valid ratio: {float(first.get('valid_ratio') or 0.0):.4f}")
        lines.append(f"Largest point `{last['label']}` valid ratio: {float(last.get('valid_ratio') or 0.0):.4f}")
        lines.append("")
        lines.append("## Most common violation reason at largest point")
        lines.append("")
        best_field = None
        best_value = -1.0
        for field, label in VIOLATION_FIELDS:
            value = float(last.get(f"{field}_per_candidate") or 0.0)
            if value > best_value:
                best_value = value
                best_field = label
        lines.append(f"Largest point dominant reason: `{best_field}` ({best_value:.4f} violations/candidate).")
        lines.append("")
        lines.append("## Reading the plots")
        lines.append("")
        lines.append("- `constraint_network_valid_invalid_ratio.png` shows whether the generated candidate pool is mostly feasible or infeasible.")
        lines.append("- `constraint_network_violation_reasons.png` shows which constraints are eliminating candidates.")
        lines.append("- `constraint_network_gpu_time_breakdown.png` separates host/device copies from the CUDA kernel.")
        lines.append("- `constraint_network_penalty_error.png` shows the floating-point validation tolerance behavior.")
    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def main() -> int:
    args = parse_args()
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    rows = group_samples(load_samples(Path(args.jsonl)))
    if not rows:
        raise SystemExit(f"No constraint_network rows found in {args.jsonl}")
    csv_path = write_summary_csv(rows, output_dir)
    report_path = write_report(rows, output_dir)
    paths = [
        plot_valid_invalid(rows, output_dir),
        plot_violation_reasons(rows, output_dir),
        plot_gpu_breakdown(rows, output_dir),
        plot_penalty_error(rows, output_dir),
    ]
    print(f"Wrote diagnostic CSV: {csv_path}")
    print(f"Wrote diagnostic report: {report_path}")
    for path in paths:
        print(f"Wrote plot: {path}")
    if args.show:
        import matplotlib.pyplot as plt
        plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
