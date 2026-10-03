#!/usr/bin/env python3
"""Render benchmark CSVs into Markdown tables inside a module README.

A README opts in with a marker pair:

    <!-- BENCH:results/bench.csv workload=random -->
    ...generated table...
    <!-- /BENCH -->

The table has one row per `n` and one column per algorithm, filled with the median
time in milliseconds. Every number in a README therefore traces to a committed CSV.
"""
import csv
import pathlib
import re
import sys
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parent.parent
MARK = re.compile(r"<!-- BENCH:(\S+)((?: \w+=\S+)*) -->\n.*?<!-- /BENCH -->", re.S)


def extra_field(extra, key):
    for part in extra.split(";"):
        if part.startswith(key + "="):
            return float(part.split("=", 1)[1])
    return None


def table(csv_path, filters):
    """`value=<key>` renders the named field of the `extra` column instead of the median time."""
    rows = list(csv.DictReader(open(csv_path)))
    value = filters.pop("value", None)
    if value:
        rows = [dict(r, median_ms=extra_field(r["extra"], value)) for r in rows]
        rows = [r for r in rows if r["median_ms"] is not None]
    for k, v in filters.items():
        rows = [r for r in rows if r.get(k) == v]
    algs = list(dict.fromkeys(r["algorithm"] for r in rows))
    by_n = defaultdict(dict)
    for r in rows:
        by_n[int(r["n"])][r["algorithm"]] = float(r["median_ms"])
    out = ["| n | " + " | ".join(algs) + " |", "| ---: |" + " ---: |" * len(algs)]
    for n in sorted(by_n):
        cells = [f"{by_n[n][a]:.3f}" if a in by_n[n] else "" for a in algs]
        out.append(f"| {n:,} | " + " | ".join(cells) + " |")
    out.append("")
    out.append(f"_{value} (see the `extra` column of the CSV)._" if value else "_Median wall time in milliseconds._")
    return "\n".join(out)


def render(readme):
    text = readme.read_text()

    def sub(m):
        csv_path = readme.parent / m.group(1)
        filters = dict(p.split("=", 1) for p in m.group(2).split())
        head = f"<!-- BENCH:{m.group(1)}{m.group(2)} -->\n"
        return head + table(csv_path, filters) + "\n<!-- /BENCH -->"

    new = MARK.sub(sub, text)
    if new != text:
        readme.write_text(new)
        print("updated", readme.relative_to(ROOT))


if __name__ == "__main__":
    targets = [pathlib.Path(a) for a in sys.argv[1:]] or list(ROOT.glob("modules/*/*/README.md"))
    for t in targets:
        render(t)
