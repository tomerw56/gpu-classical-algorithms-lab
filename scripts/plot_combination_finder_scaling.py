#!/usr/bin/env python3
"""Plot combination_finder sweep output."""

from __future__ import annotations

import argparse
from pathlib import Path

import pandas as pd


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", required=True, help="combination_finder_analysis_summary.csv")
    parser.add_argument("--output-dir", default="results/combination_finder_plots")
    parser.add_argument("--show", action="store_true")
    args = parser.parse_args()

    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)
    df = pd.read_csv(args.summary)

    try:
        import matplotlib.pyplot as plt
    except ImportError as exc:
        print(f"matplotlib is not available: {exc}")
        return 0

    # Combination growth / timing.
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(df["combinations"], df["cpu_ms_per_run"], marker="o", label="CPU")
    if "gpu_ms_per_run" in df and df["gpu_ms_per_run"].notna().any():
        ax.plot(df["combinations"], df["gpu_ms_per_run"], marker="s", linestyle="--", label="GPU")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("number of combinations")
    ax.set_ylabel("average ms per run")
    ax.set_title("Combination finder CPU/GPU timing")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out / "combination_finder_times.png", dpi=150)

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(df["combinations"], df["speedup_cpu_over_gpu"], marker="o")
    ax.axhline(1.0, linestyle="--", linewidth=1)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("number of combinations")
    ax.set_ylabel("CPU ms/run ÷ GPU ms/run")
    ax.set_title("Combination finder GPU speedup")
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out / "combination_finder_speedup.png", dpi=150)

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(df["combinations"], df["valid_ratio"], marker="o")
    ax.set_xscale("log")
    ax.set_xlabel("number of combinations")
    ax.set_ylabel("valid combinations / total combinations")
    ax.set_title("Combination finder valid ratio")
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out / "combination_finder_valid_ratio.png", dpi=150)

    reason_cols = ["budget_violations", "risk_violations", "coverage_violations", "spread_violations"]
    if all(c in df.columns for c in reason_cols):
        fig, ax = plt.subplots(figsize=(11, 6))
        bottom = None
        x = list(range(len(df)))
        for col in reason_cols:
            vals = df[col].to_numpy()
            ax.bar(x, vals, bottom=bottom, label=col.replace("_violations", ""))
            bottom = vals if bottom is None else bottom + vals
        ax.set_xticks(x)
        ax.set_xticklabels(df["label"], rotation=45, ha="right")
        ax.set_ylabel("violation count")
        ax.set_title("Combination finder rejection reasons")
        ax.legend()
        fig.tight_layout()
        fig.savefig(out / "combination_finder_violation_reasons.png", dpi=150)

    print(f"Wrote plots to {out}")
    if args.show:
        plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
