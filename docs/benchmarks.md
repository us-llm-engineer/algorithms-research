# Reproducible benchmarks

Release benchmarks are built with `ALGO_BUILD_BENCH=ON`, write CSV rows through `common/bench_harness.hpp`, and
are rendered into module READMEs by `tools/render_tables.py`. Run the complete pipeline with:

```bash
python3 tools/run_benchmarks.py
```

Each row records the algorithm, workload, input size, median/minimum/maximum wall time, repetition count, and an
algorithm-specific note. CSV files are the source of truth for the generated Markdown tables.

## Strategy tiers

Every benchmark compares a module against up to four kinds of strategy on identical seeded inputs: **brute force**
(exhaustive or textbook-naive, only run at sizes where it terminates), a standard **baseline** (`std::` container or
textbook algorithm), a fast **near-optimal heuristic** (greedy, local search, sketch) and the **optimal/exact**
algorithm. Where a strategy returns a value that can be checked, the benchmark aborts if it disagrees with the
module. Quality-bearing rows record `ratio=<result/reference>` in the `extra` column (1.0 = optimal or exact), which
`tools/plot.py` turns into `*-quality.png` figures and `tools/render_tables.py` renders with `value=ratio`.

## Figures

`python3 tools/plot.py` writes one log-log PNG per workload (and one quality PNG per workload that records `ratio`)
to `modules/<domain>/<name>/results/figures/` and refreshes the two-figures-per-row HTML galleries between
`<!-- FIGURES:... -->` markers in the READMEs.
