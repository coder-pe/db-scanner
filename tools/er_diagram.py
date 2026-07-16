#!/usr/bin/env python3
"""Generate a Graphviz entity-relationship diagram from db-scanner's schema.json.

Usage:
    er_diagram.py schema.json full [--declared-only] [--min-confidence 0.8] [--columns]
    er_diagram.py schema.json neighbors TABLE_NAME [--hops 2] [--declared-only] [--columns]

Requires the Graphviz "dot" CLI to render (apt install graphviz). Without it,
the .dot file is still written and can be rendered elsewhere (e.g. VS Code's
Graphviz preview, or https://dreampuf.github.io/GraphvizOnline/).
"""
import argparse
import html
import json
import subprocess
import sys
from collections import defaultdict


def load_schema(path):
    with open(path) as f:
        return json.load(f)


def build_table_index(schema):
    return {t["name"]: t for t in schema["tables"]}


def escape(value):
    return html.escape(str(value))


def node_label(table):
    pk_cols = set(table["primaryKey"]["columns"]) if table.get("primaryKey") else set()
    rows = []
    for col in sorted(table["columns"], key=lambda c: c["columnId"]):
        is_pk = col["name"] in pk_cols
        name_cell = f"<B>{escape(col['name'])}</B>" if is_pk else escape(col["name"])
        marker = "PK" if is_pk else ""
        rows.append(
            f'<TR><TD ALIGN="LEFT">{name_cell}</TD>'
            f'<TD ALIGN="LEFT">{escape(col["dataType"])}</TD>'
            f'<TD>{marker}</TD></TR>'
        )
    header = (
        f'<TR><TD COLSPAN="3" BGCOLOR="#4a6fa5">'
        f'<FONT COLOR="white"><B>{escape(table["name"])}</B></FONT></TD></TR>'
    )
    return (
        '<<TABLE BORDER="1" CELLBORDER="0" CELLSPACING="0" CELLPADDING="4">'
        f'{header}{"".join(rows)}</TABLE>>'
    )


def build_dot(schema, table_names, relationships, show_columns):
    table_index = build_table_index(schema)
    lines = ["digraph ER {", "  rankdir=LR;", "  fontsize=10;"]
    if show_columns:
        lines.append('  node [shape=plaintext, fontsize=10];')
    else:
        lines.append('  node [shape=box, style=filled, fillcolor="#e8eef7", fontsize=10];')
    lines.append('  edge [fontsize=9];')

    for name in sorted(table_names):
        table = table_index.get(name)
        if table is None:
            continue
        if show_columns:
            lines.append(f'  "{name}" [label={node_label(table)}];')
        else:
            lines.append(f'  "{name}" [label="{escape(name)}"];')

    for rel in relationships:
        declared = rel["kind"] == "Declared"
        style = "solid" if declared else "dashed"
        color = "#2e5c8a" if declared else "#999999"
        label = ",".join(rel["childColumns"])
        if not declared:
            label += f" ({rel['confidence']:.2f})"
        lines.append(
            f'  "{rel["childTable"]}" -> "{rel["parentTable"]}" '
            f'[style={style}, color="{color}", label="{escape(label)}"];'
        )

    lines.append("}")
    return "\n".join(lines)


def filter_relationships(schema, declared_only, min_confidence):
    rels = schema["relationships"]
    if declared_only:
        rels = [r for r in rels if r["kind"] == "Declared"]
    if min_confidence is not None:
        rels = [r for r in rels if r["kind"] == "Declared" or r.get("confidence", 1.0) >= min_confidence]
    return rels


def mode_full(args, schema):
    rels = filter_relationships(schema, args.declared_only, args.min_confidence)
    tables = set()
    for r in rels:
        tables.add(r["parentTable"])
        tables.add(r["childTable"])
    return build_dot(schema, tables, rels, args.columns)


def mode_neighbors(args, schema):
    rels = filter_relationships(schema, args.declared_only, args.min_confidence)
    adjacency = defaultdict(set)
    for r in rels:
        adjacency[r["parentTable"]].add(r["childTable"])
        adjacency[r["childTable"]].add(r["parentTable"])

    if args.table not in build_table_index(schema):
        sys.exit(f"error: table '{args.table}' not found in schema (names are case-sensitive)")

    visited = {args.table}
    frontier = {args.table}
    for _ in range(args.hops):
        nxt = set()
        for t in frontier:
            nxt |= adjacency[t]
        nxt -= visited
        visited |= nxt
        frontier = nxt

    rel_subset = [r for r in rels if r["parentTable"] in visited and r["childTable"] in visited]
    return build_dot(schema, visited, rel_subset, args.columns)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("schema", help="path to db-scanner's schema.json")
    parser.add_argument("-o", "--output", default="er_diagram", help="output basename, no extension")
    parser.add_argument("--format", default="svg", choices=["svg", "png", "pdf"])
    parser.add_argument("--declared-only", action="store_true", help="skip name-inferred relationships")
    parser.add_argument("--min-confidence", type=float, default=None, help="drop inferred relationships below this confidence (0-1)")
    parser.add_argument("--columns", action="store_true", help="show each table's full column list (slower, best for small graphs)")

    sub = parser.add_subparsers(dest="mode", required=True)
    sub.add_parser("full", help="every table that has at least one relationship")
    p_neighbors = sub.add_parser("neighbors", help="one table plus its N-hop neighbors")
    p_neighbors.add_argument("table", help="table name, case-sensitive (usually UPPERCASE)")
    p_neighbors.add_argument("--hops", type=int, default=1)

    args = parser.parse_args()
    schema = load_schema(args.schema)

    dot = mode_full(args, schema) if args.mode == "full" else mode_neighbors(args, schema)

    dot_path = f"{args.output}.dot"
    with open(dot_path, "w") as f:
        f.write(dot)

    out_path = f"{args.output}.{args.format}"
    try:
        subprocess.run(["dot", f"-T{args.format}", dot_path, "-o", out_path], check=True)
        print(f"wrote {dot_path} and {out_path}")
    except FileNotFoundError:
        print(f"wrote {dot_path} (graphviz 'dot' not found in PATH -- install it to render automatically)")
    except subprocess.CalledProcessError as e:
        sys.exit(f"dot failed: {e}")


if __name__ == "__main__":
    main()
