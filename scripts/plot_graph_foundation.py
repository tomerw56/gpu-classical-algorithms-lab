#!/usr/bin/env python3
"""Visualize graph-foundation export bundles.

Expected input directory contents are produced by:

    export_graph_foundation --output-dir results/graph_foundation_demo

The script creates:

- chain_graph.png
- grid_graph.png
- layered_graph.png
- random_sparse_graph.png
- degree_histogram.png

The plots are intentionally small and inspectable. Performance benchmarks later
reuse the same graph generators at much larger scales, but those large graphs
would not make readable diagrams.
"""

from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import matplotlib.pyplot as plt


@dataclass(frozen=True)
class Node:
    node_id: int
    x: float
    y: float
    out_degree: int
    group_index: int
    slot_index: int


@dataclass(frozen=True)
class Edge:
    source: int
    target: int


@dataclass(frozen=True)
class GraphSnapshot:
    directory: str
    graph_type: str
    summary: dict[str, object]
    nodes: list[Node]
    edges: list[Edge]


PLOT_FILENAMES = {
    "chain": "chain_graph.png",
    "grid": "grid_graph.png",
    "layered": "layered_graph.png",
    "random_sparse": "random_sparse_graph.png",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot exported Phase 3.1 graph-foundation CSV files.")
    parser.add_argument(
        "--input-dir",
        type=Path,
        required=True,
        help="Directory containing manifest.json and graph subdirectories.",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Directory for PNG files. Default: <input-dir>/plots.",
    )
    parser.add_argument(
        "--annotate-max-nodes",
        type=int,
        default=40,
        help="Annotate node ids only when a graph has at most this many nodes.",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Open plot windows in addition to writing PNG files.",
    )
    return parser.parse_args()


def ensure_output_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def load_json(path: Path) -> dict[str, object]:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def load_nodes(path: Path) -> list[Node]:
    if not path.exists():
        raise FileNotFoundError(f"required node export is missing: {path}")

    nodes: list[Node] = []
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            nodes.append(
                Node(
                    node_id=int(row["node_id"]),
                    x=float(row["x"]),
                    y=float(row["y"]),
                    out_degree=int(row["out_degree"]),
                    group_index=int(row["group_index"]),
                    slot_index=int(row["slot_index"]),
                )
            )
    return nodes


def load_edges(path: Path) -> list[Edge]:
    if not path.exists():
        raise FileNotFoundError(f"required edge export is missing: {path}")

    edges: list[Edge] = []
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            edges.append(Edge(source=int(row["source"]), target=int(row["target"])))
    return edges


def load_snapshots(input_dir: Path) -> list[GraphSnapshot]:
    manifest_path = input_dir / "manifest.json"
    if manifest_path.exists():
        manifest = load_json(manifest_path)
        graph_entries = manifest.get("graphs", [])
    else:
        graph_entries = [
            {"directory": "chain", "graph_type": "chain"},
            {"directory": "grid", "graph_type": "grid"},
            {"directory": "layered", "graph_type": "layered"},
            {"directory": "random_sparse", "graph_type": "random_sparse"},
        ]

    snapshots: list[GraphSnapshot] = []
    for entry in graph_entries:
        directory = str(entry["directory"])
        graph_type = str(entry.get("graph_type", directory))
        graph_dir = input_dir / directory
        summary = load_json(graph_dir / "graph_summary.json")
        snapshots.append(
            GraphSnapshot(
                directory=directory,
                graph_type=graph_type,
                summary=summary,
                nodes=load_nodes(graph_dir / "nodes.csv"),
                edges=load_edges(graph_dir / "edges.csv"),
            )
        )
    return snapshots


def graph_title(snapshot: GraphSnapshot) -> str:
    display_name = snapshot.summary.get("display_name", snapshot.graph_type)
    node_count = snapshot.summary.get("node_count", len(snapshot.nodes))
    edge_count = snapshot.summary.get("directed_adjacency_entry_count", len(snapshot.edges))
    return f"{display_name}\n{node_count} nodes, {edge_count} directed adjacency entries"


def draw_graph(snapshot: GraphSnapshot, out_dir: Path, annotate_max_nodes: int) -> Path:
    node_by_id = {node.node_id: node for node in snapshot.nodes}
    fig, ax = plt.subplots(figsize=(11, 7))

    for edge in snapshot.edges:
        source = node_by_id.get(edge.source)
        target = node_by_id.get(edge.target)
        if source is None or target is None:
            continue
        ax.plot([source.x, target.x], [source.y, target.y], linewidth=0.8, alpha=0.45)

    xs = [node.x for node in snapshot.nodes]
    ys = [node.y for node in snapshot.nodes]
    degrees = [node.out_degree for node in snapshot.nodes]
    sizes = [32.0 + 8.0 * degree for degree in degrees]
    scatter = ax.scatter(xs, ys, s=sizes, c=degrees)
    fig.colorbar(scatter, ax=ax, label="out-degree")

    if len(snapshot.nodes) <= annotate_max_nodes:
        for node in snapshot.nodes:
            ax.text(node.x, node.y, str(node.node_id), fontsize=8, ha="center", va="bottom")

    ax.set_title(graph_title(snapshot))
    ax.set_xlabel("layout x")
    ax.set_ylabel("layout y")
    ax.grid(alpha=0.25)
    ax.set_aspect("equal", adjustable="datalim")
    fig.tight_layout()

    filename = PLOT_FILENAMES.get(snapshot.graph_type, f"{snapshot.graph_type}_graph.png")
    output = out_dir / filename
    fig.savefig(output, dpi=180)
    return output


def degree_histogram_series(snapshot: GraphSnapshot) -> tuple[list[int], list[int]]:
    counts = Counter(node.out_degree for node in snapshot.nodes)
    if not counts:
        return [0], [0]
    degrees = list(range(0, max(counts) + 1))
    totals = [counts.get(degree, 0) for degree in degrees]
    return degrees, totals


def save_degree_histogram(snapshots: Iterable[GraphSnapshot], out_dir: Path) -> Path:
    fig, ax = plt.subplots(figsize=(11, 7))

    for snapshot in snapshots:
        degrees, totals = degree_histogram_series(snapshot)
        label = str(snapshot.summary.get("display_name", snapshot.graph_type))
        ax.step(degrees, totals, where="mid", linewidth=2.0, label=label)
        ax.scatter(degrees, totals, s=24)

    ax.set_title("Out-degree distributions for exported graph-foundation demos")
    ax.set_xlabel("out-degree")
    ax.set_ylabel("node count")
    ax.grid(alpha=0.25)
    ax.legend(loc="best")
    fig.tight_layout()

    output = out_dir / "degree_histogram.png"
    fig.savefig(output, dpi=180)
    return output


def main() -> None:
    args = parse_args()
    if args.annotate_max_nodes < 0:
        raise ValueError("--annotate-max-nodes must be non-negative")

    input_dir = args.input_dir
    out_dir = args.out_dir or (input_dir / "plots")
    ensure_output_dir(out_dir)

    snapshots = load_snapshots(input_dir)
    outputs = [draw_graph(snapshot, out_dir, args.annotate_max_nodes) for snapshot in snapshots]
    outputs.append(save_degree_histogram(snapshots, out_dir))

    print("Wrote graph foundation plots:")
    for output in outputs:
        print(f"  {output}")

    if args.show:
        plt.show()
    else:
        plt.close("all")


if __name__ == "__main__":
    main()
