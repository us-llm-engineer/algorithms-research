#!/usr/bin/env python3
"""Plot a benchmark CSV (log-log median time vs n, one line per algorithm).

    python3 tools/plot.py modules/data-structures/fibonacci-heap/results/bench.csv random out.png
"""
import csv
import sys
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

csv_path, workload, out = sys.argv[1:4]
series = defaultdict(list)
for r in csv.DictReader(open(csv_path)):
    if r["workload"] == workload:
        series[r["algorithm"]].append((int(r["n"]), float(r["median_ms"])))
fig, ax = plt.subplots(figsize=(6, 4))
for alg, pts in series.items():
    pts.sort()
    ax.plot(*zip(*pts), marker="o", label=alg)
ax.set_xscale("log")
ax.set_yscale("log")
ax.set_xlabel("n")
ax.set_ylabel("median time (ms)")
ax.set_title(f"workload: {workload}")
ax.grid(True, which="both", alpha=0.3)
ax.legend()
fig.tight_layout()
fig.savefig(out, dpi=140)
