#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt


def save_line(df, output_dir, filename, title, ylabel, cols, log_y=False, hline_one=False):
    fig, ax = plt.subplots(figsize=(10, 6))
    for col, label in cols:
        if col in df and df[col].notna().any():
            ax.plot(df["scenarios"], df[col], marker="o", label=label)
    ax.set_xscale("log")
    if log_y:
        ax.set_yscale("log")
    if hline_one:
        ax.axhline(1.0, linestyle="--", linewidth=1)
    ax.set_xlabel("scenarios")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    path = output_dir / filename
    fig.savefig(path, dpi=150)
    print(f"Wrote plot: {path}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--summary-csv", required=True)
    ap.add_argument("--output-dir", default="results/scenario_simulation_plots")
    ap.add_argument("--show", action="store_true")
    args = ap.parse_args()
    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)
    df = pd.read_csv(args.summary_csv).sort_values("scenarios")

    save_line(df, out, "scenario_simulation_times.png", "Scenario simulation CPU/GPU timing", "average ms per run", [("cpu_ms_per_run", "CPU"), ("gpu_ms_per_run", "GPU total"), ("gpu_kernel_ms_per_run", "GPU kernel")], log_y=True)
    save_line(df, out, "scenario_simulation_speedup.png", "Scenario simulation CPU/GPU speedup", "CPU ms/run ÷ GPU ms/run", [("speedup_cpu_over_gpu", "speedup")], log_y=True, hline_one=True)
    save_line(df, out, "scenario_simulation_feasibility_ratio.png", "Feasible/infeasible scenario ratio", "ratio", [("feasible_ratio", "feasible"), ("infeasible_ratio", "infeasible")])
    save_line(df, out, "scenario_simulation_score_summary.png", "Plan score summary across scenarios", "score", [("best_score", "best"), ("mean_score", "mean"), ("worst_score", "worst"), ("robustness_score", "robustness")], log_y=True)

    if {"failure_scenarios","capacity_scenarios","risk_scenarios","delay_scenarios"}.issubset(df.columns):
        fig, ax = plt.subplots(figsize=(10, 6))
        x = range(len(df))
        labels = df["label"].astype(str).tolist()
        bottom = [0] * len(df)
        for col, label in [("failure_scenarios","failure"),("capacity_scenarios","capacity"),("risk_scenarios","risk"),("delay_scenarios","delay")]:
            vals = df[col].fillna(0).to_numpy()
            ax.bar(x, vals, bottom=bottom, label=label)
            bottom = [b + float(v) for b, v in zip(bottom, vals)]
        ax.set_xticks(list(x), labels, rotation=35, ha="right")
        ax.set_ylabel("scenario count with violation type")
        ax.set_title("Scenario violation breakdown")
        ax.legend()
        fig.tight_layout()
        path = out / "scenario_simulation_violation_breakdown.png"
        fig.savefig(path, dpi=150)
        print(f"Wrote plot: {path}")

    report = out / "scenario_simulation_scaling_report.md"
    best = df[df["speedup_cpu_over_gpu"].notna()].sort_values("speedup_cpu_over_gpu", ascending=False).head(1)
    lines = ["# Scenario simulation scaling report", ""]
    if not best.empty:
        row = best.iloc[0]
        lines.append(f"Best observed speedup: {row['speedup_cpu_over_gpu']:.3f}x at `{row['label']}` ({int(row['scenarios'])} scenarios).")
        lines.append("")
    lines.append("## Correctness semantics")
    lines.append("")
    lines.append("The benchmark `correct` flag means CPU/GPU agreement. Feasibility and robustness are represented separately by `feasible_ratio`, violation counts, and robustness score.")
    if "feasible_ratio" in df:
        ratios = df["feasible_ratio"].dropna()
        if not ratios.empty:
            lines.append("")
            lines.append(f"Feasible-ratio range: {ratios.min():.3f} to {ratios.max():.3f}.")
            if (ratios == 0.0).all():
                lines.append("WARNING: every scenario was infeasible. This is poor for robustness interpretation even if CPU/GPU agreement is correct.")
            elif (ratios == 1.0).all():
                lines.append("WARNING: every scenario was feasible. This is poor for robustness interpretation because the scenario generator is too mild.")
    report.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote report: {report}")
    if args.show:
        plt.show()

if __name__ == "__main__":
    main()
