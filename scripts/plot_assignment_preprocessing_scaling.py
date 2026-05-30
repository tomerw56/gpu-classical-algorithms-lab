#!/usr/bin/env python3
"""Plot assignment_preprocessing scale-sweep results."""
from __future__ import annotations
import argparse
import csv
from pathlib import Path


def load_summary(path: Path):
    with path.open("r", encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        for key in ["tasks", "resources", "pairs", "top_k", "cpu_ms_per_run", "gpu_ms_per_run", "gpu_kernel_ms_per_run", "speedup_cpu_over_gpu", "candidate_reduction_ratio", "feasible_pair_count", "selected_candidate_count"]:
            if key in r and r[key] != "":
                try:
                    r[key] = float(r[key])
                except Exception:
                    pass
    return rows


def plot_lines(rows, outdir: Path, filename: str, title: str, ylabel: str, keys):
    import matplotlib.pyplot as plt
    fig, ax = plt.subplots(figsize=(10, 6))
    plotted = False
    for key, label in keys:
        vals = [r for r in rows if r.get(key) not in {None, ""}]
        if vals:
            plotted = True
            ax.plot([r["pairs"] for r in vals], [r[key] for r in vals], marker="o", label=label)
    if not plotted:
        plt.close(fig)
        return None
    ax.set_xscale("log")
    ax.set_xlabel("task/resource pairs")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    path = outdir / filename
    fig.savefig(path, dpi=150)
    return path


def plot_violation_reasons(rows, outdir: Path):
    import matplotlib.pyplot as plt
    if not rows:
        return None
    last = rows[-1]
    labels = ["skill", "capacity", "time", "distance", "zone", "risk"]
    keys = [f"{x}_violations" for x in labels]
    vals = []
    for key in keys:
        try:
            vals.append(float(last.get(key, 0)))
        except Exception:
            vals.append(0.0)
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.bar(labels, vals)
    ax.set_ylabel("violation count")
    ax.set_title(f"Assignment preprocessing violation mix ({last['label']})")
    ax.tick_params(axis="x", rotation=30)
    fig.tight_layout()
    path = outdir / "assignment_preprocessing_violation_reasons.png"
    fig.savefig(path, dpi=150)
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--summary-csv", required=True)
    ap.add_argument("--output-dir", default="results/assignment_preprocessing_plots")
    ap.add_argument("--show", action="store_true")
    args = ap.parse_args()
    outdir = Path(args.output_dir)
    outdir.mkdir(parents=True, exist_ok=True)
    rows = load_summary(Path(args.summary_csv))
    paths = []
    for p in [
        plot_lines(rows, outdir, "assignment_preprocessing_times.png", "Assignment preprocessing CPU/GPU time", "average ms/run", [("cpu_ms_per_run", "CPU"), ("gpu_ms_per_run", "GPU total"), ("gpu_kernel_ms_per_run", "GPU kernel")]),
        plot_lines(rows, outdir, "assignment_preprocessing_speedup.png", "Assignment preprocessing GPU speedup", "CPU ms/run ÷ GPU ms/run", [("speedup_cpu_over_gpu", "speedup")]),
        plot_lines(rows, outdir, "assignment_preprocessing_candidate_reduction.png", "Candidate reduction ratio", "top-K selected pairs / all pairs", [("candidate_reduction_ratio", "reduction ratio")]),
        plot_lines(rows, outdir, "assignment_preprocessing_selected_candidates.png", "Selected candidate count", "selected candidates", [("selected_candidate_count", "top-K candidates")]),
        plot_violation_reasons(rows, outdir),
    ]:
        if p:
            paths.append(p)
    for p in paths:
        print(f"Wrote plot: {p}")
    if args.show:
        import matplotlib.pyplot as plt
        plt.show()


if __name__ == "__main__":
    main()
