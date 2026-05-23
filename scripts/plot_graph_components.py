#!/usr/bin/env python3
"""Plot connected-components graph exports.

Input files are produced by:

    export_graph_components --output-dir results/graph_components_demo

The script creates:

- graph_components.png: graph layout colored by component label
- graph_component_sizes.png: component-size bar chart
"""

from __future__ import annotations

import argparse
import csv
import json
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt


@dataclass(frozen=True)
class Node:
    node_id: int
    x: float
    y: float
    component_label: int
    component_index: int
    component_size: int
    out_degree: int


@dataclass(frozen=True)
class Edge:
    source: int
    target: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot exported connected-components graph CSV files.")
    parser.add_argument("--input-dir", type=Path, required=True, help="Directory containing nodes.csv and edges.csv.")
    parser.add_argument("--out-dir", type=Path, default=None, help="Output plot directory. Default: <input-dir>/plots.")
    parser.add_argument("--annotate-max-nodes", type=int, default=80, help="Annotate node ids only under this count.")
    parser.add_argument("--max-edges", type=int, default=5000, help="Draw at most this many edges to keep plots readable.")
    parser.add_argument("--show", action="store_true", help="Open plot windows after saving PNGs.")
    return parser.parse_args()


def load_nodes(path: Path) -> list[Node]:
    nodes: list[Node] = []
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            nodes.append(
                Node(
                    node_id=int(row["node_id"]),
                    x=float(row["x"]),
                    y=float(row["y"]),
                    component_label=int(row["component_label"]),
                    component_index=int(row["component_index"]),
                    component_size=int(row["component_size"]),
                    out_degree=int(row["out_degree"]),
                )
            )
    return nodes


def load_edges(path: Path) -> list[Edge]:
    edges: list[Edge] = []
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            edges.append(Edge(source=int(row["source"]), target=int(row["target"])))
    return edges


def load_json(path: Path) -> dict[str, object]:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def plot_components(nodes: list[Node], edges: list[Edge], metadata: dict[str, object], out_dir: Path, annotate_max_nodes: int, max_edges: int) -> Path:
    node_by_id = {node.node_id: node for node in nodes}

    fig, ax = plt.subplots(figsize=(12, 7))
    for edge in edges[:max_edges]:
        source = node_by_id.get(edge.source)
        target = node_by_id.get(edge.target)
        if source is None or target is None:
            continue
        ax.plot([source.x, target.x], [source.y, target.y], linewidth=0.5, alpha=0.25)

    xs = [node.x for node in nodes]
    ys = [node.y for node in nodes]
    colors = [node.component_index for node in nodes]
    sizes = [28.0 + 5.0 * min(node.out_degree, 12) for node in nodes]
    scatter = ax.scatter(xs, ys, c=colors, s=sizes)
    fig.colorbar(scatter, ax=ax, label="component index")

    if len(nodes) <= annotate_max_nodes:
        for node in nodes:
            ax.text(node.x, node.y, str(node.node_id), fontsize=7, ha="center", va="bottom")

    graph_kind = metadata.get("graph_kind", "connected-components graph")
    node_count = metadata.get("node_count", len(nodes))
    edge_count = metadata.get("directed_adjacency_entry_count", len(edges))
    component_count = metadata.get("component_count", len(set(colors)))
    ax.set_title(f"{graph_kind}: {node_count} nodes, {edge_count} directed entries, {component_count} components")
    ax.set_xlabel("layout x")
    ax.set_ylabel("layout y")
    ax.grid(alpha=0.2)
    ax.set_aspect("equal", adjustable="datalim")
    fig.tight_layout()

    output = out_dir / "graph_components.png"
    fig.savefig(output, dpi=180)
    return output


def plot_component_sizes(nodes: list[Node], out_dir: Path) -> Path:
    sizes_by_index: dict[int, int] = {}
    for node in nodes:
        sizes_by_index[node.component_index] = node.component_size

    indices = sorted(sizes_by_index)
    sizes = [sizes_by_index[index] for index in indices]

    fig, ax = plt.subplots(figsize=(12, 6))
    ax.bar(indices, sizes)
    ax.set_title("Connected component sizes")
    ax.set_xlabel("component index")
    ax.set_ylabel("nodes")
    ax.grid(axis="y", alpha=0.25)
    fig.tight_layout()

    output = out_dir / "graph_component_sizes.png"
    fig.savefig(output, dpi=180)
    return output


def main() -> int:
    args = parse_args()
    input_dir = args.input_dir
    out_dir = args.out_dir or (input_dir / "plots")
    ensure_dir(out_dir)

    nodes = load_nodes(input_dir / "nodes.csv")
    edges = load_edges(input_dir / "edges.csv")
    metadata = load_json(input_dir / "metadata.json")

    outputs = [
        plot_components(nodes, edges, metadata, out_dir, args.annotate_max_nodes, args.max_edges),
        plot_component_sizes(nodes, out_dir),
    ]

    for output in outputs:
        print(f"Wrote {output}")

    if args.show:
        plt.show()
    else:
        plt.close("all")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
