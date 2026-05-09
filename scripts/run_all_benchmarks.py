#!/usr/bin/env python3
"""Run all benchmark presets and append results to JSONL."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True, help="Path to gpu_algobench executable")
    parser.add_argument("--output", default="results/all.jsonl")
    parser.add_argument("--repeat", type=int, default=5)
    parser.add_argument("--warmup", type=int, default=1)
    parser.add_argument("--presets", nargs="+", default=["tiny", "small", "medium"])
    parser.add_argument("--no-gpu", action="store_true")
    args = parser.parse_args()

    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()

    for preset in args.presets:
        cmd = [
            args.exe,
            "--benchmark", "all",
            "--preset", preset,
            "--repeat", str(args.repeat),
            "--warmup", str(args.warmup),
            "--output", str(output),
        ]
        if args.no_gpu:
            cmd.append("--no-gpu")

        print("Running:", " ".join(cmd))
        subprocess.run(cmd, check=True)

    print(f"Wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
