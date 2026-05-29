#!/usr/bin/env python3
"""Plot constraint_network scaling results."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path


def load_summary(path: Path) -> list[dict[str, object]]:
    groups: dict[tuple[int, int, int, str], dict[str, list[dict[str, object]]]] = defaultdict(lambda: defaultdict(list))
    with path.open("r", encoding="utf-8") as handle:
        for raw in handle:
            if not raw.strip():
                continue
            row = json.loads(raw)
            if row.get("benchmark") != "constraint_network":
                continue
            input_size = row.get("input_size") or {}
            metadata = row.get("metadata") or {}
            repeat = int(row.get("repeat", 1))
            key = (int(input_size.get("tasks", 0)), int(input_size.get("resources", 0)), int(input_size.get("candidates", 0)), str(row.get("preset", "")))
            groups[key][str(row.get("variant", ""))].append({
                "ms": float(row.get("total_ms", 0.0)) / repeat,
                "correct": bool(row.get("correct", False)),
                "valid_count": int(metadata.get("valid_count", 0)),
                "total_violations": int(metadata.get("total_violations", 0)),
            })
    rows: list[dict[str, object]] = []
    for (tasks, resources, candidates, label), variants in groups.items():
        cpu = [r for r in variants.get("cpu", []) if r["correct"]]
        gpu = [r for r in variants.get("gpu", []) if r["correct"]]
        cpu_ms = sum(float(r["ms"]) for r in cpu) / len(cpu) if cpu else None
        gpu_ms = sum(float(r["ms"]) for r in gpu) / len(gpu) if gpu else None
        speedup = cpu_ms / gpu_ms if cpu_ms is not None and gpu_ms not in {None, 0.0} else None
        rep = (gpu or cpu or [{}])[0]
        rows.append({
            "label": label,
            "tasks": tasks,
            "resources": resources,
            "candidates": candidates,
            "cpu_ms": cpu_ms,
            "gpu_ms": gpu_ms,
            "speedup": speedup,
            "valid_ratio": (int(rep.get("valid_count", 0)) / candidates) if candidates else None,
            "violations_per_candidate": (int(rep.get("total_violations", 0)) / candidates) if candidates else None,
        })
    return sorted(rows, key=lambda r: int(r["candidates"]))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--jsonl", required=True)
    parser.add_argument("--output-dir", default="results/constraint_network_scale_plots")
    parser.add_argument("--show", action="store_true")
    args = parser.parse_args()

    import matplotlib.pyplot as plt

    rows = load_summary(Path(args.jsonl))
    out = Path(args.output_dir)
    out.mkdir(parents=True, exist_ok=True)

    x = [r["candidates"] for r in rows]

    fig, ax = plt.subplots(figsize=(9, 5.5))
    ax.plot(x, [r["cpu_ms"] for r in rows], marker="o", label="CPU")
    ax.plot(x, [r["gpu_ms"] for r in rows], marker="s", linestyle="--", label="GPU")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("candidate assignments")
    ax.set_ylabel("average ms per run")
    ax.set_title("Constraint network CPU/GPU timing")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out / "constraint_network_times.png", dpi=150)

    fig, ax = plt.subplots(figsize=(9, 5.5))
    ax.plot(x, [r["speedup"] for r in rows], marker="o")
    ax.axhline(1.0, linestyle="--", linewidth=1)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("candidate assignments")
    ax.set_ylabel("CPU ms/run ÷ GPU ms/run")
    ax.set_title("Constraint network GPU speedup")
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()
    fig.savefig(out / "constraint_network_speedup.png", dpi=150)

    fig, ax = plt.subplots(figsize=(9, 5.5))
    ax.plot(x, [r["valid_ratio"] for r in rows], marker="o", label="valid ratio")
    ax.plot(x, [r["violations_per_candidate"] for r in rows], marker="s", label="violations / candidate")
    ax.set_xscale("log")
    ax.set_xlabel("candidate assignments")
    ax.set_title("Constraint-network output mix")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out / "constraint_network_output_mix.png", dpi=150)

    print(f"Wrote plots to: {out}")
    if args.show:
        plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
