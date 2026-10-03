# Tarjan strongly connected components

Linear-time SCC decomposition with an iterative DFS (no recursion limit) and the condensation DAG.

## 1. Problem Statement

* **Input**: a directed multigraph on vertices $0..n-1$.
* **Output**: `component_of[v]`, the list of components (in reverse topological order of the condensation), and the condensation edges.
* **Constraints**: iterative implementation handles deep graphs (tested on a large sparse path and a large cycle) without stack overflow.

## 2. Algorithm Overview & Mathematical Model

One DFS assigns discovery indices and **low-links** $\mathrm{low}(v)=\min(\mathrm{disc}(v),\mathrm{disc}(w)\text{ for back/cross edges into the stack},\mathrm{low}(\cdot)\text{ of children})$. A vertex with $\mathrm{low}(v)=\mathrm{disc}(v)$ is the root of an SCC and everything above it on the Tarjan stack is popped as one component. Components are emitted in reverse topological order, which yields the condensation DAG for free.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| Whole decomposition | $O(V+E)$ | $O(V+E)$ |
| Condensation edges | $O(E)$ | |

## 4. Quickstart & Usage

Header-only; add `modules/graph-flow/tarjan-scc/include` to the include path (CMake target `tarjan_scc`).

```cpp
#include "tarjan_scc.hpp"

algo::TarjanScc g(5);
g.add_edge(0, 1); g.add_edge(1, 2); g.add_edge(2, 0); g.add_edge(2, 3); g.add_edge(3, 4);
auto r = g.run();   // components {0,1,2}, {3}, {4}
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | bitset transitive closure (Warshall, $O(V^3/64)$), $u\sim v$ iff mutually reachable (shown to $n=1024$) |
| Baseline | Kosaraju: two DFS passes over $G$ and $G^T$ |
| This module | Tarjan, one pass |

All three must agree on the component count.

### Random digraph, average out-degree 1.5

<!-- BENCH:results/bench.csv workload=random-digraph -->
| n | Tarjan | Kosaraju (two-pass baseline) | brute-force transitive closure |
| ---: | ---: | ---: | ---: |
| 128 | 0.031 | 0.027 | 0.102 |
| 256 | 0.087 | 0.053 | 0.298 |
| 512 | 0.204 | 0.125 | 1.488 |
| 1,024 | 0.399 | 0.253 | 4.692 |
| 4,096 | 1.897 | 1.229 |  |
| 16,384 | 15.087 | 6.995 |  |
| 65,536 | 57.831 | 55.725 |  |
| 262,144 | 297.794 | 287.456 |  |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/random-digraph.png" alt="random-digraph (results/bench.csv)"><br><sub>How tarjan-scc scales on random directed graphs with n vertices and 1.5n edges. At n = 262,144, Tarjan is slowest at 298 ms; Kosaraju (two-pass baseline) is fastest at 287 ms. Source: graph-flow/tarjan-scc/results/bench.csv.</sub></td><td width="50%"></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* The brute force is already 12x slower than Tarjan at $n=1024$ and grows cubically.
* **Tarjan and Kosaraju are indistinguishable** in this sweep (297 ms vs 287 ms at $n=262144$; Kosaraju is even slightly faster because it builds the transpose with simple loops). Tarjan's advantage is a single traversal and no transposed graph in memory, not raw speed here.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/tarjan_scc_bench.cpp`](bench/tarjan_scc_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) covers the CLRS example, empty/singleton/self-loop graphs, invalid vertices, a single cycle regardless of insertion order, a path with one component per vertex, random graphs against a mutual-reachability reference, duplicate edges, strong connectivity of every reported component, acyclicity of the condensation, determinism, label shuffling, and stack safety on a large sparse path and a large cycle.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target tarjan_scc_test
./build/debug-asan/mod_tarjan-scc/tarjan_scc_test
```

## 7. Limitations & Trade-Offs

* No incremental/dynamic updates.
* Memory is $O(V+E)$ for adjacency plus $O(V)$ bookkeeping; the benchmark rebuilds the graph each repetition.

## 8. References

* R. E. Tarjan, “Depth-first search and linear graph algorithms,” *SIAM J. Comput.* 1(2), 1972.
* T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein, *Introduction to Algorithms*, 3rd ed., MIT Press, 2009.
