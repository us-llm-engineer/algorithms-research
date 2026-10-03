# Max flow: push-relabel (with Dinic and Edmonds-Karp)

Four maximum-flow strategies behind one API: FIFO and highest-label push-relabel (gap + global relabeling), Dinic, and Edmonds-Karp.

## 1. Problem Statement

* **Input**: a directed network with non-negative 64-bit integer capacities, a source $s$ and sink $t\ne s$.
* **Output**: the maximum flow value, per-edge flows (`flow_on`), and a minimum cut (`min_cut_source_side`).
* **Constraints**: overflow-checked sums; parallel edges and self-loops allowed.

## 2. Algorithm Overview & Mathematical Model

Push-relabel keeps a *preflow* (excess allowed at vertices) and integer heights $h$ with the validity invariant $h(u)\le h(v)+1$ for every residual arc $(u,v)$. An active vertex pushes along admissible arcs ($h(u)=h(v)+1$) or relabels to $1+\min h(v)$. Heuristics: **gap** (if no vertex has height $k$, lift everything above $k$ to $n+1$) and **global relabeling** (recompute exact distances with a reverse BFS). Termination yields a maximum preflow whose excess is returned to $s$; `stats()` reports pushes, saturating pushes, relabels, augmentations and phases.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| Edmonds-Karp | $O(VE^2)$ | $O(V+E)$ |
| Dinic | $O(V^2E)$ | $O(V+E)$ |
| FIFO push-relabel | $O(V^3)$ | $O(V+E)$ |
| Highest-label push-relabel | $O(V^2\sqrt E)$ | $O(V+E)$ |

## 4. Quickstart & Usage

Header-only; add `modules/graph-flow/push-relabel/include` to the include path (CMake target `push_relabel`).

```cpp
#include "push_relabel.hpp"

algo::MaxFlow g(4);
g.add_edge(0, 1, 3); g.add_edge(0, 2, 2); g.add_edge(1, 3, 2); g.add_edge(2, 3, 3);
auto f = g.max_flow(0, 3, algo::MaxFlow::Algorithm::HighestLabelPushRelabel);  // 4
auto side = g.min_cut_source_side();
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Baseline | Edmonds-Karp (shortest augmenting path) |
| Practical | Dinic (blocking flows) |
| Asymptotically optimal family | FIFO push-relabel, highest-label push-relabel |

All strategies must return the same flow value (the benchmark aborts otherwise). `n` is the vertex count.

### Sparse random network (about 5 edges per vertex)

<!-- BENCH:results/bench.csv workload=random-sparse -->
| n | Edmonds-Karp (baseline) | Dinic | push-relabel FIFO | push-relabel highest-label |
| ---: | ---: | ---: | ---: | ---: |
| 200 | 0.220 | 0.236 | 0.251 | 0.201 |
| 800 | 2.587 | 2.614 | 2.714 | 2.146 |
| 3,200 | 17.943 | 12.067 | 20.329 | 17.354 |
| 12,800 | 152.300 | 69.911 | 48.698 | 32.444 |
| 25,600 | 211.037 | 108.713 | 93.587 | 202.605 |

_Median wall time in milliseconds._
<!-- /BENCH -->

### $k\times k$ grid, source and sink in opposite corners ($n=k^2$)

<!-- BENCH:results/bench.csv workload=grid -->
| n | Edmonds-Karp (baseline) | Dinic | push-relabel FIFO | push-relabel highest-label |
| ---: | ---: | ---: | ---: | ---: |
| 64 | 0.030 | 0.025 | 0.027 | 0.028 |
| 256 | 0.146 | 0.070 | 0.125 | 0.124 |
| 1,024 | 0.568 | 0.276 | 0.394 | 0.473 |
| 2,304 | 4.033 | 1.337 | 2.548 | 2.592 |
| 4,096 | 7.707 | 1.378 | 2.734 | 3.294 |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/random-sparse.png" alt="random-sparse (results/bench.csv)"><br><sub>random-sparse (results/bench.csv)</sub></td><td width="50%"><img src="results/figures/grid.png" alt="grid (results/bench.csv)"><br><sub>grid (results/bench.csv)</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* Differences are modest on these inputs: Dinic and FIFO push-relabel are about 2x faster than Edmonds-Karp on the sparse network at 25,600 vertices; on the grid Dinic is about 5.6x faster than Edmonds-Karp.
* **Highest-label push-relabel is not the fastest here**, despite the best worst-case bound: on small-flow networks the augmenting-path methods do very little work, and push-relabel's constant factors (heights, buckets, global relabeling) only pay off on dense or high-flow instances, which this sweep does not stress.
* Treat the asymptotic ordering as a worst-case guarantee, not a prediction for sparse random data.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/push_relabel_bench.cpp`](bench/push_relabel_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) checks the CLRS network (flow 23) for every algorithm, degenerate inputs ($s=t$, disconnected, zero capacity, parallel edges, self-loops), max-flow against a brute-force minimum cut on 600 tiny random graphs, agreement of all four algorithms with a matrix Edmonds-Karp oracle (300 seeds), structured families (grid, layered, complete, path, star, bipartite), independent flow/cut certificates, a valid final height labeling, operation counts against theory (relabels $\le2n^2$), the gap heuristic's benefit, capacities near $2^{62}$, and 300x300 grid and 20,000-node scale tests.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target push_relabel_test
./build/debug-asan/mod_push-relabel/push_relabel_test
```

## 7. Limitations & Trade-Offs

* Integer capacities only.
* Benchmarks use sparse, low-flow instances; dense and high-flow graphs (where push-relabel is expected to dominate) are not included.
* Single run per strategy builds the residual graph from scratch (build time included in each measurement).

## 8. References

* A. V. Goldberg and R. E. Tarjan, “A new approach to the maximum-flow problem,” *J. ACM* 35(4), 1988.
* B. V. Cherkassky and A. V. Goldberg, “On implementing the push-relabel method for the maximum flow problem,” *Algorithmica* 19, 1997.
* E. A. Dinic, 1970; J. Edmonds and R. M. Karp, *J. ACM* 19(2), 1972.
* T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein, *Introduction to Algorithms*, 3rd ed., MIT Press, 2009.
