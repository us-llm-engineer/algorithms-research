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
import random
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


ORDERED = {
    "insert-random": "n random keys inserted one by one into an empty set",
    "insert-sorted": "n keys inserted in ascending order into an empty set",
    "lookup-uniform": "n membership lookups for keys drawn uniformly from the stored set",
    "lookup-skewed": "n membership lookups where 90% of the queries hit the hottest 1% of the keys",
    "erase-random": "n random keys inserted and then all erased in the same order",
}
WORKLOADS = {
    ("fibonacci-heap", "push-pop"): "n random keys pushed into an empty heap and then all popped",
    ("fibonacci-heap", "decrease-key"): "n keys pushed, n random decrease-key operations applied, then everything popped (the binary-heap baseline re-pushes duplicates and skips stale entries)",
    ("persistent-tree", "build-versions"): "n updates, each of which creates a new queryable version of the map",
    ("persistent-tree", "query-old-versions"): "n lookups, each against a uniformly random past version",
    ("rank-select", "rank"): "100,000 random rank queries on a random bit vector of 2^n bits",
    ("rank-select", "select"): "100,000 random select queries on a random bit vector of 2^n bits",
    ("van-emde-boas", "successor"): "200,000 random successor queries over a universe of n = U integers holding U/16 random keys",
    ("van-emde-boas", "build"): "building the structure from U/16 random keys drawn from a universe of n = U integers",
    ("hopcroft-karp", "random-deg3"): "random bipartite graphs with n vertices per side and three random edges per left vertex",
    ("hopcroft-karp", "staircase"): "a staircase bipartite graph with n vertices per side where left vertex i is adjacent to right vertices i and i+1",
    ("push-relabel", "random-sparse"): "random sparse flow networks with n vertices and about five arcs per vertex",
    ("push-relabel", "grid"): "square grid networks with n = k*k vertices, source and sink in opposite corners",
    ("tarjan-scc", "random-digraph"): "random directed graphs with n vertices and 1.5n edges",
    ("dp-optimization", "partition-squared"): "splitting n numbers into 16 contiguous segments to minimise the sum of squared segment sums",
    ("dp-optimization", "optimal-merge"): "optimal merging of n adjacent weights, paying the merged weight at each step",
    ("min-cost-flow", "random-sparse"): "random sparse networks with n vertices, about 5n arcs and a requested flow of 0.3n + 5 units",
    ("simplex", "random-dense-lp"): "random dense linear programs with n variables and n constraints",
    ("count-min", "zipf-stream"): "a Zipf(1.1) stream of n items over n/4 distinct keys",
    ("hyperloglog", "distinct-count"): "a stream of n items containing n/2 distinct values",
    ("christofides", "random-euclid"): "random Euclidean TSP instances with n cities in the unit square",
}
for _m in ("treap", "skip-list", "splay-tree"):
    for _w, _d in ORDERED.items():
        WORKLOADS[(_m, _w)] = _d
N_UNIT = {"rank-select": "n is log2 of the bit-vector length", "van-emde-boas": "n is the universe size U"}
CAPTIONS = {}


def fmt(v, quality=False):
    if quality:
        return f"{v:.3f}"
    return f"{v:,.0f}" if v >= 100 else f"{v:.3g}"


SHORT = {
    ("fibonacci-heap", "decrease-key"): "n pushes, n random decrease-keys, then all pops",
    ("van-emde-boas", "successor"): "200,000 successor queries over U = n integers holding U/16 keys",
    ("van-emde-boas", "build"): "building from U/16 random keys in a universe of U = n",
    ("rank-select", "rank"): "100,000 rank queries on 2^n random bits",
    ("rank-select", "select"): "100,000 select queries on 2^n random bits",
    ("hopcroft-karp", "staircase"): "staircase bipartite graphs, n vertices per side",
    ("hopcroft-karp", "random-deg3"): "random bipartite graphs, three edges per left vertex",
    ("push-relabel", "grid"): "grid networks with n = k*k vertices",
    ("push-relabel", "random-sparse"): "sparse random networks, about five arcs per vertex",
    ("min-cost-flow", "random-sparse"): "sparse random networks requesting 0.3n + 5 units of flow",
    ("dp-optimization", "partition-squared"): "splitting n numbers into 16 segments of minimal squared sums",
    ("dp-optimization", "optimal-merge"): "optimal adjacent merging of n weights",
    ("persistent-tree", "query-old-versions"): "n lookups against random past versions",
    ("persistent-tree", "build-versions"): "n updates, each creating a new version",
}


def make_caption(module, workload, series, quality):
    """A short caption (25-40 words) naming the workload and the measured outcome at the largest n.

    Phrasing and length vary between figures; the choice is seeded by the figure's name so regenerating
    the READMEs is deterministic.
    """
    rng = random.Random(f"{module.name}/{workload}/{quality}")
    desc = SHORT.get((module.name, workload), WORKLOADS[(module.name, workload)])
    own = [a for a in series if is_own(a, module.name)]
    n_star = max(n for a in own for n, _ in series[a])
    at = {a: dict(pts)[n_star] for a, pts in series.items() if n_star in dict(pts)}
    me = (max if quality else min)(own, key=lambda a: at.get(a, float("-inf") if quality else float("inf")))
    rows = sorted(at, key=at.get)
    path = f"{module.parent.name}/{module.name}/results/bench.csv"
    n = f"{n_star:,}"
    m = module.name
    if quality:
        far = max(at, key=lambda a: abs(at[a] - 1.0))
        intros = [f"{m}: result quality on {desc}.", f"How close each {m} strategy gets to optimal on {desc}.", f"Quality versus the exact reference for {desc} ({m})."]
        reads = ["", "1.0 means optimal.", "Values are result divided by the exact answer, so 1.0 is optimal."]
        results = [f"At n = {n}, {me} scores {fmt(at[me], True)}; {far} is furthest from 1.0 at {fmt(at[far], True)}.",
                   f"Largest size n = {n}: {me} reaches {fmt(at[me], True)} while {far} lands at {fmt(at[far], True)}."]
    else:
        fast, slow = rows[0], rows[-1]
        rank = rows.index(me) + 1
        intros = [f"{m}: runtime on {desc}.", f"Median time per strategy for {desc} ({m}).", f"How {m} scales on {desc}."]
        reads = ["", "Both axes are logarithmic.", "Log-log axes; dashed is brute force, dotted is heuristic, bold is this module."]
        if len(rows) == 1:
            results = [f"At n = {n}, {me} takes {fmt(at[me])} ms and is the only strategy run at that size.",
                       f"Largest size n = {n}: only {me} was run, taking {fmt(at[me])} ms."]
        elif rank == 1:
            results = [f"At n = {n}, {me} is fastest at {fmt(at[me])} ms; {slow} is slowest at {fmt(at[slow])} ms.",
                       f"Largest size n = {n}: {me} leads with {fmt(at[me])} ms, {slow} trails at {fmt(at[slow])} ms."]
        elif me == slow:
            results = [f"At n = {n}, {me} is slowest at {fmt(at[me])} ms; {fast} is fastest at {fmt(at[fast])} ms.",
                       f"Largest size n = {n}: {me} needs {fmt(at[me])} ms against {fmt(at[fast])} ms for {fast}."]
        else:
            results = [f"At n = {n}, {me} takes {fmt(at[me])} ms (rank {rank} of {len(rows)}); {fast} is fastest at {fmt(at[fast])} ms.",
                       f"Largest size n = {n}: {me} needs {fmt(at[me])} ms, {fast} only {fmt(at[fast])} ms and {slow} {fmt(at[slow])} ms."]
    datas = ["", f"Data: {path}.", f"Source: {path}."]
    combos = [" ".join(x for x in (i, r, res, d) if x) for i in intros for r in reads for res in results for d in datas]
    fits = [c for c in combos if 25 <= len(c.split()) <= 40]
    assert fits, (module.name, workload)
    return rng.choice(fits)


def module_figures(module):
    csv_path = module / "results" / "bench.csv"
    if not csv_path.exists():
        return []
    figs = []
    for workload, series in load(csv_path).items():
        out = module / "results" / "figures" / f"{workload}.png"
        plot_workload(workload, series, module.name, out, module.name)
        CAPTIONS[out] = make_caption(module, workload, series, False)
        figs.append((workload, out))
    for workload, series in load(csv_path, "ratio").items():
        out = module / "results" / "figures" / f"{workload}-quality.png"
        plot_workload(workload, series, module.name, out, module.name + " quality",
                      ylabel="result / reference (1.0 = optimal or exact)", logy=False)
        CAPTIONS[out] = make_caption(module, workload, series, True)
        figs.append((workload + " quality", out))
    return figs


def table(items, base):
    """items: [(label, path)] -> HTML table, two figures per row; the caption under each comes from CAPTIONS."""
    rows = []
    for i in range(0, len(items), 2):
        cells = []
        for label, path in items[i:i + 2]:
            rel = path.relative_to(base).as_posix()
            cells.append(f'<td width="50%"><img src="{rel}" alt="{label}"><br><sub>{CAPTIONS[path]}</sub></td>')
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
