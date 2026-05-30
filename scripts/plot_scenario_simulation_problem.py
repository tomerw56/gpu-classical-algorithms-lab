#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-dir", required=True)
    ap.add_argument("--show", action="store_true")
    args = ap.parse_args()
    d = Path(args.input_dir)
    tasks = pd.read_csv(d / "tasks.csv")
    resources = pd.read_csv(d / "resources.csv")
    scenarios = pd.read_csv(d / "scenarios.csv")

    fig, ax = plt.subplots(figsize=(8, 7))
    ax.scatter(resources["x"], resources["y"], marker="s", s=80, label="resources")
    ax.scatter(tasks["x"], tasks["y"], marker="o", s=35, label="tasks")
    for _, task in tasks.head(80).iterrows():
        r = resources.loc[resources["resource_id"] == task["assigned_resource"]].iloc[0]
        ax.plot([task["x"], r["x"]], [task["y"], r["y"]], linewidth=0.4, alpha=0.35)
    ax.set_title("Scenario simulation base plan: task assignments")
    ax.set_xlabel("x")
    ax.set_ylabel("y")
    ax.grid(True, alpha=0.25)
    ax.legend()
    fig.tight_layout()
    path = d / "scenario_base_plan.png"
    fig.savefig(path, dpi=150)
    print(f"Wrote plot: {path}")

    fig, ax = plt.subplots(figsize=(9, 5))
    ax.hist(scenarios["score"], bins=40)
    ax.set_title("Scenario score distribution")
    ax.set_xlabel("scenario score")
    ax.set_ylabel("count")
    fig.tight_layout()
    path = d / "scenario_score_distribution.png"
    fig.savefig(path, dpi=150)
    print(f"Wrote plot: {path}")

    fig, ax = plt.subplots(figsize=(9, 5))
    counts = {
        "failure": int((scenarios["failed_tasks"] > 0).sum()),
        "capacity": int((scenarios["capacity_violations"] > 0).sum()),
        "risk": int((scenarios["risk_violations"] > 0).sum()),
        "delay": int((scenarios["delay_violations"] > 0).sum()),
        "feasible": int((scenarios["feasible"] == 1).sum()),
    }
    ax.bar(list(counts.keys()), list(counts.values()))
    ax.set_title("Scenario outcome counts")
    ax.set_ylabel("scenarios")
    fig.tight_layout()
    path = d / "scenario_outcome_breakdown.png"
    fig.savefig(path, dpi=150)
    print(f"Wrote plot: {path}")

    report = d / "scenario_simulation_problem_report.md"
    report.write_text(
        "# Scenario simulation problem report\n\n"
        f"Tasks: {len(tasks)}\n\n"
        f"Resources: {len(resources)}\n\n"
        f"Scenarios: {len(scenarios)}\n\n"
        f"Feasible scenarios: {int((scenarios['feasible'] == 1).sum())}\n\n"
        f"Worst score: {float(scenarios['score'].max()):.3f}\n\n",
        encoding="utf-8",
    )
    print(f"Wrote report: {report}")
    if args.show:
        plt.show()

if __name__ == "__main__":
    main()
