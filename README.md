# algorithms-research

[![CI](https://github.com/us-llm-engineer/algorithms-research/actions/workflows/ci.yml/badge.svg)](https://github.com/us-llm-engineer/algorithms-research/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)](https://en.cppreference.com/w/cpp/20)

Graduate-level algorithm and data-structure implementations in C++20, each paired with a rigorous complexity
and correctness write-up, a hard test suite (Catch2) and reproducible benchmarks against standard baselines.

Each module lives in `modules/<domain>/<name>/` and documents itself in eight sections: problem statement,
mathematical model and proofs, complexity table, quickstart, benchmarks, verification, trade-offs, references
(see [`docs/MODULE_TEMPLATE.md`](docs/MODULE_TEMPLATE.md)).

## Modules at a glance

Every module is benchmarked against **brute-force, baseline, near-optimal/heuristic and optimal** strategies on identical seeded inputs. Wall times are medians; "wins" and "loses" are reported as measured, including where the textbook-optimal algorithm is slower in practice.

| Module | Problem | Key bound | Strategies compared | Headline measured result |
| :--- | :--- | :--- | :--- | :--- |
| [fibonacci-heap](modules/data-structures/fibonacci-heap) | priority queue with `decrease_key` | $O(1)$ amortized decrease-key | binary heap, lazy-deletion heap | **loses**: 15.6x slower push/pop, 1.3x slower with heavy decrease-key at $n=2^{20}$ |
| [treap](modules/data-structures/treap) | ordered set + order statistics | $O(\log n)$ expected | sorted array, `std::set`, `std::unordered_set` | 5.6x faster than `std::set` on sorted inserts; up to 2x slower on random lookups |
| [skip-list](modules/data-structures/skip-list) | ordered set | $O(\log n)$ expected | same as treap | 2.2-3.2x slower than `std::set`; 1.5x faster on sorted inserts |
| [splay-tree](modules/data-structures/splay-tree) | self-adjusting ordered set | $O(\log n)$ amortized | same as treap | 19.6x faster sorted inserts, 1.4x faster skewed lookups, 2.5x slower uniform lookups |
| [persistent-tree](modules/data-structures/persistent-tree) | versioned ordered map | $O(\log n)$ per update | full copy per version, ephemeral map | 120x less memory and 1000x faster build than copying at $n=4096$ |
| [rank-select](modules/data-structures/rank-select) | succinct bit vector | $O(1)$ rank, 4% overhead | linear scan, prefix counters | about 100x faster than scanning at $n=2^{18}$; Fast layout 2.3x faster than Compact |
| [van-emde-boas](modules/data-structures/van-emde-boas) | integer predecessor search | $O(\log\log U)$ | bitmap scan, sorted array, `std::set` | 3.1x faster successor than `std::set` at $U=2^{24}$, on par with a static sorted array |
| [hopcroft-karp](modules/graph-flow/hopcroft-karp) | bipartite matching | $O(E\sqrt V)$ | greedy, Kuhn | 720x faster than Kuhn at $n=64000$; greedy reaches 87.6% of optimum |
| [push-relabel](modules/graph-flow/push-relabel) | maximum flow | $O(V^2\sqrt E)$ | Edmonds-Karp, Dinic, FIFO | Dinic fastest on the grid (5.6x over Edmonds-Karp); highest-label not fastest on sparse inputs |
| [tarjan-scc](modules/graph-flow/tarjan-scc) | strongly connected components | $O(V+E)$ | transitive closure, Kosaraju | 12x faster than closure at $n=1024$; ties with Kosaraju |
| [dp-optimization](modules/optimization/dp-optimization) | partition / interval-merge DP | $O(kn\log n)$, $O(n^2)$ | brute force, quadratic/cubic DP | 240x faster than the quadratic DP at $n=8192$ |
| [min-cost-flow](modules/optimization/min-cost-flow) | min-cost flow | $O(F\,E\log V)$ | Bellman-Ford SSP, SPFA SSP, greedy | on par with Bellman-Ford; **1.9x slower than SPFA** on sparse inputs |
| [simplex](modules/optimization/simplex) | linear programming | exponential worst case | vertex enumeration, greedy | 60,000x faster than enumeration at $n=10$; greedy reaches 52% of optimum at $n=256$ |
| [count-min](modules/sketches/count-min) | frequency estimation | $O(wd)$ memory | exact map, sort, Misra-Gries | 64 KiB vs 14 MB, 2x faster, about 2x overestimate on top keys |
| [hyperloglog](modules/sketches/hyperloglog) | distinct counting | $1.04/\sqrt{m}$ error | sort, hash set, linear counting | 191x faster than a hash set at 0.1% error (16 KiB); linear counting saturates |
| [christofides](modules/approximation/christofides) | metric TSP | $3/2$-approximation | brute force, Held-Karp, NN, double-tree, 2-opt | 2-opt gives better tours; Christofides is 3.4x faster than 2-opt at $n=256$ |

## Visualizations

All figures are generated from the CSV files named in their captions by `python3 tools/plot.py` (log-log median time versus input size; dashed = brute force, dotted = heuristic/approximation, bold = this module; quality plots divide the result by the exact or optimal reference, so 1.0 is optimal). Every module README embeds the full set for that module.

### Data structures

<!-- FIGURES:ROOT:data-structures -->
<table>
<tr><td width="50%"><img src="modules/data-structures/fibonacci-heap/results/figures/push-pop.png" alt="fibonacci-heap: push-pop &mdash; data-structures/fibonacci-heap/results/bench.csv"><br><sub>fibonacci-heap: push-pop &mdash; data-structures/fibonacci-heap/results/bench.csv</sub></td><td width="50%"><img src="modules/data-structures/fibonacci-heap/results/figures/decrease-key.png" alt="fibonacci-heap: decrease-key &mdash; data-structures/fibonacci-heap/results/bench.csv"><br><sub>fibonacci-heap: decrease-key &mdash; data-structures/fibonacci-heap/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/data-structures/persistent-tree/results/figures/build-versions.png" alt="persistent-tree: build-versions &mdash; data-structures/persistent-tree/results/bench.csv"><br><sub>persistent-tree: build-versions &mdash; data-structures/persistent-tree/results/bench.csv</sub></td><td width="50%"><img src="modules/data-structures/persistent-tree/results/figures/query-old-versions.png" alt="persistent-tree: query-old-versions &mdash; data-structures/persistent-tree/results/bench.csv"><br><sub>persistent-tree: query-old-versions &mdash; data-structures/persistent-tree/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/data-structures/rank-select/results/figures/rank.png" alt="rank-select: rank &mdash; data-structures/rank-select/results/bench.csv"><br><sub>rank-select: rank &mdash; data-structures/rank-select/results/bench.csv</sub></td><td width="50%"><img src="modules/data-structures/rank-select/results/figures/select.png" alt="rank-select: select &mdash; data-structures/rank-select/results/bench.csv"><br><sub>rank-select: select &mdash; data-structures/rank-select/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/data-structures/skip-list/results/figures/lookup-uniform.png" alt="skip-list: lookup-uniform &mdash; data-structures/skip-list/results/bench.csv"><br><sub>skip-list: lookup-uniform &mdash; data-structures/skip-list/results/bench.csv</sub></td><td width="50%"><img src="modules/data-structures/skip-list/results/figures/insert-sorted.png" alt="skip-list: insert-sorted &mdash; data-structures/skip-list/results/bench.csv"><br><sub>skip-list: insert-sorted &mdash; data-structures/skip-list/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/data-structures/splay-tree/results/figures/lookup-skewed.png" alt="splay-tree: lookup-skewed &mdash; data-structures/splay-tree/results/bench.csv"><br><sub>splay-tree: lookup-skewed &mdash; data-structures/splay-tree/results/bench.csv</sub></td><td width="50%"><img src="modules/data-structures/splay-tree/results/figures/insert-sorted.png" alt="splay-tree: insert-sorted &mdash; data-structures/splay-tree/results/bench.csv"><br><sub>splay-tree: insert-sorted &mdash; data-structures/splay-tree/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/data-structures/treap/results/figures/insert-sorted.png" alt="treap: insert-sorted &mdash; data-structures/treap/results/bench.csv"><br><sub>treap: insert-sorted &mdash; data-structures/treap/results/bench.csv</sub></td><td width="50%"><img src="modules/data-structures/treap/results/figures/lookup-uniform.png" alt="treap: lookup-uniform &mdash; data-structures/treap/results/bench.csv"><br><sub>treap: lookup-uniform &mdash; data-structures/treap/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/data-structures/van-emde-boas/results/figures/successor.png" alt="van-emde-boas: successor &mdash; data-structures/van-emde-boas/results/bench.csv"><br><sub>van-emde-boas: successor &mdash; data-structures/van-emde-boas/results/bench.csv</sub></td><td width="50%"><img src="modules/data-structures/van-emde-boas/results/figures/build.png" alt="van-emde-boas: build &mdash; data-structures/van-emde-boas/results/bench.csv"><br><sub>van-emde-boas: build &mdash; data-structures/van-emde-boas/results/bench.csv</sub></td></tr>
</table>
<!-- /FIGURES -->

### Graph algorithms and flows

<!-- FIGURES:ROOT:graph-flow -->
<table>
<tr><td width="50%"><img src="modules/graph-flow/hopcroft-karp/results/figures/random-deg3.png" alt="hopcroft-karp: random-deg3 &mdash; graph-flow/hopcroft-karp/results/bench.csv"><br><sub>hopcroft-karp: random-deg3 &mdash; graph-flow/hopcroft-karp/results/bench.csv</sub></td><td width="50%"><img src="modules/graph-flow/hopcroft-karp/results/figures/random-deg3-quality.png" alt="hopcroft-karp: random-deg3 quality &mdash; graph-flow/hopcroft-karp/results/bench.csv"><br><sub>hopcroft-karp: random-deg3 quality &mdash; graph-flow/hopcroft-karp/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/graph-flow/push-relabel/results/figures/random-sparse.png" alt="push-relabel: random-sparse &mdash; graph-flow/push-relabel/results/bench.csv"><br><sub>push-relabel: random-sparse &mdash; graph-flow/push-relabel/results/bench.csv</sub></td><td width="50%"><img src="modules/graph-flow/push-relabel/results/figures/grid.png" alt="push-relabel: grid &mdash; graph-flow/push-relabel/results/bench.csv"><br><sub>push-relabel: grid &mdash; graph-flow/push-relabel/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/graph-flow/tarjan-scc/results/figures/random-digraph.png" alt="tarjan-scc: random-digraph &mdash; graph-flow/tarjan-scc/results/bench.csv"><br><sub>tarjan-scc: random-digraph &mdash; graph-flow/tarjan-scc/results/bench.csv</sub></td><td width="50%"></td></tr>
</table>
<!-- /FIGURES -->

### Optimization

<!-- FIGURES:ROOT:optimization -->
<table>
<tr><td width="50%"><img src="modules/optimization/dp-optimization/results/figures/partition-squared.png" alt="dp-optimization: partition-squared &mdash; optimization/dp-optimization/results/bench.csv"><br><sub>dp-optimization: partition-squared &mdash; optimization/dp-optimization/results/bench.csv</sub></td><td width="50%"><img src="modules/optimization/dp-optimization/results/figures/optimal-merge.png" alt="dp-optimization: optimal-merge &mdash; optimization/dp-optimization/results/bench.csv"><br><sub>dp-optimization: optimal-merge &mdash; optimization/dp-optimization/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/optimization/min-cost-flow/results/figures/random-sparse.png" alt="min-cost-flow: random-sparse &mdash; optimization/min-cost-flow/results/bench.csv"><br><sub>min-cost-flow: random-sparse &mdash; optimization/min-cost-flow/results/bench.csv</sub></td><td width="50%"><img src="modules/optimization/min-cost-flow/results/figures/random-sparse-quality.png" alt="min-cost-flow: random-sparse quality &mdash; optimization/min-cost-flow/results/bench.csv"><br><sub>min-cost-flow: random-sparse quality &mdash; optimization/min-cost-flow/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/optimization/simplex/results/figures/random-dense-lp.png" alt="simplex: random-dense-lp &mdash; optimization/simplex/results/bench.csv"><br><sub>simplex: random-dense-lp &mdash; optimization/simplex/results/bench.csv</sub></td><td width="50%"><img src="modules/optimization/simplex/results/figures/random-dense-lp-quality.png" alt="simplex: random-dense-lp quality &mdash; optimization/simplex/results/bench.csv"><br><sub>simplex: random-dense-lp quality &mdash; optimization/simplex/results/bench.csv</sub></td></tr>
</table>
<!-- /FIGURES -->

### Streaming sketches

<!-- FIGURES:ROOT:sketches -->
<table>
<tr><td width="50%"><img src="modules/sketches/count-min/results/figures/zipf-stream.png" alt="count-min: zipf-stream &mdash; sketches/count-min/results/bench.csv"><br><sub>count-min: zipf-stream &mdash; sketches/count-min/results/bench.csv</sub></td><td width="50%"><img src="modules/sketches/count-min/results/figures/zipf-stream-quality.png" alt="count-min: zipf-stream quality &mdash; sketches/count-min/results/bench.csv"><br><sub>count-min: zipf-stream quality &mdash; sketches/count-min/results/bench.csv</sub></td></tr>
<tr><td width="50%"><img src="modules/sketches/hyperloglog/results/figures/distinct-count.png" alt="hyperloglog: distinct-count &mdash; sketches/hyperloglog/results/bench.csv"><br><sub>hyperloglog: distinct-count &mdash; sketches/hyperloglog/results/bench.csv</sub></td><td width="50%"><img src="modules/sketches/hyperloglog/results/figures/distinct-count-quality.png" alt="hyperloglog: distinct-count quality &mdash; sketches/hyperloglog/results/bench.csv"><br><sub>hyperloglog: distinct-count quality &mdash; sketches/hyperloglog/results/bench.csv</sub></td></tr>
</table>
<!-- /FIGURES -->

### Approximation algorithms

<!-- FIGURES:ROOT:approximation -->
<table>
<tr><td width="50%"><img src="modules/approximation/christofides/results/figures/random-euclid.png" alt="christofides: random-euclid &mdash; approximation/christofides/results/bench.csv"><br><sub>christofides: random-euclid &mdash; approximation/christofides/results/bench.csv</sub></td><td width="50%"><img src="modules/approximation/christofides/results/figures/random-euclid-quality.png" alt="christofides: random-euclid quality &mdash; approximation/christofides/results/bench.csv"><br><sub>christofides: random-euclid quality &mdash; approximation/christofides/results/bench.csv</sub></td></tr>
</table>
<!-- /FIGURES -->

## Build and test

Requirements: a C++20 compiler (GCC 11+), CMake >= 3.25, Ninja and [vcpkg](https://vcpkg.io) (Catch2 is the only dependency and is installed in manifest mode from `vcpkg.json`).

```bash
export VCPKG_ROOT=$HOME/vcpkg                    # your vcpkg checkout
cmake --preset debug-asan                        # Debug + AddressSanitizer + UBSan
cmake --build --preset debug-asan
ctest --preset debug-asan                        # all tests run under the sanitizers
cmake --preset release && cmake --build --preset release   # -O3 plus benchmark executables
```

Randomized tests are seeded; a failure prints its seed. Re-run with `ALGO_TEST_SEED=<n>` to explore other inputs.

## Layout

```
common/      seeded RNG, benchmark harness, test helpers (header-only)
modules/     one directory per algorithm: include/, tests/, bench/, results/, README.md
tools/       mutation checker (mutate.py), CSV -> README tables (render_tables.py), plots (plot.py)
```

Benchmarks write CSV files to each module's `results/`; the tables in the READMEs are generated from those files.

## Reproducing the benchmarks

```bash
python3 tools/run_benchmarks.py      # release build, all benchmarks -> modules/*/results/bench.csv, README tables
python3 tools/plot.py                # CSV -> modules/*/results/figures/*.png and the figure galleries
```

Requirements: GCC 11+, CMake >= 3.25, Ninja, vcpkg (Catch2), Python 3 with `matplotlib`. Reference machine for the committed numbers: Intel Core i7-10750H @ 2.60 GHz, 7 GiB RAM, GCC 11.5 `-O3`, one thread. Absolute times vary by machine; the *ratios* between strategies are the reproducible result. The full sweep takes roughly 20 minutes, dominated by the intentionally slow brute-force baselines.

## Limitations

* Benchmarks use synthetic seeded inputs (random, Zipf, grid, staircase); they do not stand in for application workloads. Several sweeps (max flow, matching "staircase") are not adversarial for the baselines, and each module README says so.
* Single-instance quality figures (TSP, LP) are noisy; they illustrate the trade-off rather than estimate averages.
* Timings are single-threaded wall-clock medians on one laptop-class CPU.

## References

* T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein, *Introduction to Algorithms*, 3rd ed., MIT Press, 2009.
* D. E. Knuth, *The Art of Computer Programming*, Addison-Wesley.
* Per-algorithm primary sources are cited in section 8 of each module README.
