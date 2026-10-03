#!/usr/bin/env python3
"""Plot every module's benchmark CSV and render two-per-row figure galleries into READMEs.

    python3 tools/plot.py            # figures for all modules + refresh <!-- FIGURES --> blocks

One PNG per workload is written to <module>/results/figures/<workload>.png (log-log median time vs n,
brute-force strategies dashed, near-optimal/heuristic dotted, the module's own algorithm bold).
READMEs opt in with a marker pair:

    <!-- FIGURES:results/bench.csv -->
    ...generated <table>...
    <!-- /FIGURES -->

The root README uses <!-- FIGURES:ROOT:<domain> --> to embed a curated selection per domain.
"""
import csv
import pathlib
import re
import sys
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = pathlib.Path(__file__).resolve().parent.parent
MARK = re.compile(r"<!-- FIGURES:(\S+) -->\n.*?<!-- /FIGURES -->", re.S)
PALETTE = ["#1f6feb", "#d1242f", "#2da44e", "#bf8700", "#8250df", "#0a7e8c", "#6e7781", "#cf5f00"]


# Algorithm-label prefixes that identify the module's own implementation(s) in each CSV.
OWN = {
    "hopcroft-karp": "Hopcroft-Karp", "tarjan-scc": "Tarjan", "push-relabel": "push-relabel",
    "count-min": "Count-Min", "hyperloglog": "HyperLogLog", "rank-select": "rank-select",
    "van-emde-boas": "van Emde Boas", "persistent-tree": "persistent map",
}


def is_own(label, module):
    return label == module or "(module)" in label or label.startswith(OWN.get(module, "\0"))


def style(label, own):
    low = label.lower()
    if own:
        return dict(lw=2.6, ls="-", marker="o", ms=6, zorder=5)
    if "brute" in low or "naive" in low:
        return dict(lw=1.6, ls="--", marker="s", ms=4)
    if any(k in low for k in ("heuristic", "greedy", "approx", "nearest", "2-opt", "double")):
        return dict(lw=1.6, ls=":", marker="^", ms=5)
    return dict(lw=1.6, ls="-", marker="D", ms=4, alpha=0.9)


def extra_value(extra, key):
    for part in extra.split(";"):
        if part.startswith(key + "="):
            try:
                return float(part.split("=", 1)[1])
            except ValueError:
                return None
    return None


def load(csv_path, key=None):
    """workload -> algorithm -> [(n, value)]; value is median_ms or, with `key`, a field of `extra`."""
    data = defaultdict(lambda: defaultdict(list))
    for r in csv.DictReader(open(csv_path)):
        v = float(r["median_ms"]) if key is None else extra_value(r["extra"], key)
        if v is not None:
            data[r["workload"]][r["algorithm"]].append((int(r["n"]), v))
    return data


def plot_workload(workload, series, module, out, title_prefix, ylabel="median time (ms)", logy=True):
    fig, ax = plt.subplots(figsize=(6, 4))
    for i, (alg, pts) in enumerate(series.items()):
        pts.sort()
        xs, ys = zip(*pts)
        ax.plot(xs, ys, color=PALETTE[i % len(PALETTE)], label=alg, **style(alg, is_own(alg, module)))
    ax.set_xscale("log")
    if logy:
        ax.set_yscale("log")
    else:
        ax.axhline(1.0, color="black", lw=0.8, alpha=0.5)
    ax.set_xlabel("n")
    ax.set_ylabel(ylabel)
    ax.set_title(f"{title_prefix}: {workload}", fontsize=11)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=7.5)
    fig.tight_layout()
    out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(out, dpi=130)
    plt.close(fig)


def module_figures(module):
    csv_path = module / "results" / "bench.csv"
    if not csv_path.exists():
        return []
    figs = []
    for workload, series in load(csv_path).items():
        out = module / "results" / "figures" / f"{workload}.png"
        plot_workload(workload, series, module.name, out, module.name)
        figs.append((workload, out))
    for workload, series in load(csv_path, "ratio").items():
        out = module / "results" / "figures" / f"{workload}-quality.png"
        plot_workload(workload, series, module.name, out, module.name + " quality",
                      ylabel="result / reference (1.0 = optimal or exact)", logy=False)
        figs.append((workload + " quality", out))
    return figs


def table(items, base):
    """items: [(caption, path)] -> HTML table, two figures per row."""
    rows = []
    for i in range(0, len(items), 2):
        cells = []
        for cap, path in items[i:i + 2]:
            rel = path.relative_to(base).as_posix()
            cells.append(f'<td width="50%"><img src="{rel}" alt="{cap}"><br><sub>{cap}</sub></td>')
        if len(cells) == 1:
            cells.append('<td width="50%"></td>')
        rows.append("<tr>" + "".join(cells) + "</tr>")
    return "<table>\n" + "\n".join(rows) + "\n</table>"


# Figures shown in the root README: (module, figure names). The first is the time plot, the second a
# complementary one (a quality plot where the module has one).
HEADLINE = {
    "fibonacci-heap": ["push-pop", "decrease-key"],
    "treap": ["insert-sorted", "lookup-uniform"],
    "skip-list": ["lookup-uniform", "insert-sorted"],
    "splay-tree": ["lookup-skewed", "insert-sorted"],
    "persistent-tree": ["build-versions", "query-old-versions"],
    "rank-select": ["rank", "select"],
    "van-emde-boas": ["successor", "build"],
    "hopcroft-karp": ["random-deg3", "random-deg3 quality"],
    "push-relabel": ["random-sparse", "grid"],
    "tarjan-scc": ["random-digraph"],
    "dp-optimization": ["partition-squared", "optimal-merge"],
    "min-cost-flow": ["random-sparse", "random-sparse quality"],
    "simplex": ["random-dense-lp", "random-dense-lp quality"],
    "count-min": ["zipf-stream", "zipf-stream quality"],
    "hyperloglog": ["distinct-count", "distinct-count quality"],
    "christofides": ["random-euclid", "random-euclid quality"],
}


def refresh(readme, all_figs):
    text = readme.read_text()

    def sub(m):
        key = m.group(1)
        if key.startswith("ROOT:"):
            domain = key.split(":", 1)[1]
            items = []
            for mod, figs in sorted(all_figs.items()):
                if mod.parent.name != domain:
                    continue
                by_name = dict(figs)
                for name in HEADLINE[mod.name]:
                    if name in by_name:
                        items.append((f"{mod.name}: {name} &mdash; {mod.parent.name}/{mod.name}/results/bench.csv", by_name[name]))
            body = table(items, ROOT)
        else:
            body = table([(f"{wl} (results/bench.csv)", p) for wl, p in all_figs.get(readme.parent, [])], readme.parent)
        return f"<!-- FIGURES:{key} -->\n{body}\n<!-- /FIGURES -->"

    new = MARK.sub(sub, text)
    if new != text:
        readme.write_text(new)
        print("updated", readme.relative_to(ROOT))


if __name__ == "__main__":
    all_figs = {m: module_figures(m) for m in sorted(ROOT.glob("modules/*/*")) if m.is_dir()}
    all_figs = {m: f for m, f in all_figs.items() if f}
    for r in [ROOT / "README.md", *ROOT.glob("modules/*/*/README.md")]:
        refresh(r, all_figs)
