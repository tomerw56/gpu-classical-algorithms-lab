#!/usr/bin/env python3
"""Print CPU/GPU speedup summary from JSONL results."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("jsonl")
    args = parser.parse_args()

    groups = defaultdict(dict)
    with open(args.jsonl, "r", encoding="utf-8") as f:
        for line in f:
            if not line.strip():
                continue
            row = json.loads(line)
            key = (row["benchmark"], row["preset"])
            groups[key][row["variant"]] = row

    for (benchmark, preset), variants in sorted(groups.items()):
        cpu = variants.get("cpu")
        gpu = variants.get("gpu")
        if not cpu or not gpu:
            continue
        if gpu["total_ms"] <= 0:
            continue
        speedup = cpu["total_ms"] / gpu["total_ms"]
        print(f"{benchmark:24s} {preset:8s} speedup={speedup:8.3f}x  cpu={cpu['total_ms']:.3f}ms gpu={gpu['total_ms']:.3f}ms")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
