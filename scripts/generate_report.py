#!/usr/bin/env python3
"""Generate a tiny Markdown summary from JSONL results."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("jsonl")
    parser.add_argument("--out", default="results/report.md")
    args = parser.parse_args()

    rows = []
    with open(args.jsonl, "r", encoding="utf-8") as f:
        for line in f:
            if line.strip():
                rows.append(json.loads(line))

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    with out.open("w", encoding="utf-8") as f:
        f.write("# Benchmark Report\n\n")
        f.write("| Benchmark | Variant | Preset | Total ms | Kernel ms | Correct | Device |\n")
        f.write("|---|---:|---:|---:|---:|---:|---|\n")
        for r in rows:
            f.write(
                f"| {r['benchmark']} | {r['variant']} | {r['preset']} | "
                f"{r['total_ms']:.3f} | {r['kernel_ms']:.3f} | {r['correct']} | {r['device']} |\n"
            )

    print(f"Wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
