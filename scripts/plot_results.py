#!/usr/bin/env python3
"""Simple first-pass plotter for JSONL benchmark output."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("jsonl", help="Input JSONL file")
    parser.add_argument("--out", default="results/plot.png")
    args = parser.parse_args()

    rows = []
    with open(args.jsonl, "r", encoding="utf-8") as f:
        for line in f:
            if line.strip():
                rows.append(json.loads(line))

    if not rows:
        print("No rows found")
        return 1

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed. Showing rows instead:")
        for row in rows:
            print(row)
        return 0

    labels = [f"{r['benchmark']}\n{r['variant']}\n{r['preset']}" for r in rows]
    values = [r["total_ms"] for r in rows]

    fig = plt.figure(figsize=(max(8, len(rows) * 1.2), 5))
    ax = fig.add_subplot(111)
    ax.bar(labels, values)
    ax.set_ylabel("total_ms")
    ax.set_title("Benchmark total time")
    ax.tick_params(axis="x", rotation=45)
    fig.tight_layout()

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out)
    print(f"Wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
