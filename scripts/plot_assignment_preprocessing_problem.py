#!/usr/bin/env python3
"""Plot exported assignment_preprocessing problem instance."""
from __future__ import annotations
import argparse
import csv
from pathlib import Path


def read_matrix(path: Path):
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.reader(f)
        header = next(reader)
        rows = []
        labels = []
        for row in reader:
            labels.append(row[0])
            rows.append([float(x) for x in row[1:]])
    return header[1:], labels, rows


def read_topk(path: Path):
    with path.open("r", encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--input-dir", required=True)
    ap.add_argument("--output-dir", default=None)
    ap.add_argument("--show", action="store_true")
    args = ap.parse_args()
    indir = Path(args.input_dir)
    outdir = Path(args.output_dir) if args.output_dir else indir / "plots"
    outdir.mkdir(parents=True, exist_ok=True)
    import matplotlib.pyplot as plt

    resource_labels, task_labels, feasible = read_matrix(indir / "pair_feasibility_matrix.csv")
    _, _, costs = read_matrix(indir / "pair_cost_matrix.csv")

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.imshow(feasible, aspect="auto", interpolation="nearest")
    ax.set_title("Assignment feasibility matrix: task can use resource")
    ax.set_xlabel("resource")
    ax.set_ylabel("task")
    fig.tight_layout()
    fig.savefig(outdir / "assignment_feasibility_matrix.png", dpi=150)

    masked_costs = [[v if v >= 0 else float("nan") for v in row] for row in costs]
    fig, ax = plt.subplots(figsize=(10, 6))
    image = ax.imshow(masked_costs, aspect="auto", interpolation="nearest")
    fig.colorbar(image, ax=ax, label="cost")
    ax.set_title("Assignment feasible-pair cost matrix")
    ax.set_xlabel("resource")
    ax.set_ylabel("task")
    fig.tight_layout()
    fig.savefig(outdir / "assignment_cost_matrix.png", dpi=150)

    topk = read_topk(indir / "topk_candidates.csv")
    counts = {}
    for row in topk:
        rid = int(row["resource_id"])
        if rid >= 0:
            counts[rid] = counts.get(rid, 0) + 1
    fig, ax = plt.subplots(figsize=(10, 5))
    xs = sorted(counts)
    ax.bar(xs, [counts[x] for x in xs])
    ax.set_title("How often each resource appears in task top-K")
    ax.set_xlabel("resource")
    ax.set_ylabel("top-K appearances")
    fig.tight_layout()
    fig.savefig(outdir / "assignment_topk_resource_usage.png", dpi=150)

    report = outdir / "assignment_problem_report.md"
    report.write_text(
        "# Assignment preprocessing problem visualization\n\n"
        "Generated plots:\n\n"
        "- `assignment_feasibility_matrix.png`: A task/resource cell is active when the resource can plausibly serve the task.\n"
        "- `assignment_cost_matrix.png`: cost for feasible pairs; infeasible pairs are masked.\n"
        "- `assignment_topk_resource_usage.png`: how often a resource appears in the top-K candidate list.\n",
        encoding="utf-8",
    )
    print(f"Wrote plots to: {outdir}")
    if args.show:
        plt.show()


if __name__ == "__main__":
    main()
