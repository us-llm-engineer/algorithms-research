# Min-cost flow

Successive shortest paths with Johnson potentials and Dijkstra, returning flow, cost, per-edge flows and dual potentials.

## 1. Problem Statement

* **Input**: a directed network with integer capacities and (possibly negative) integer costs, source $s$, sink $t$, requested flow $F$.
* **Output**: the flow actually sent (at most $F$), its minimum cost, per-edge flows and node potentials.
* **Constraints**: no negative-cost cycles reachable with positive capacity.

## 2. Algorithm Overview & Mathematical Model

Maintain a flow that is minimum-cost for its value. Each iteration augments along a shortest $s$-$t$ path in the residual graph. Potentials $\pi$ turn residual costs into non-negative **reduced costs** $c_\pi(u,v)=c(u,v)+\pi(u)-\pi(v)\ge0$, so Dijkstra applies; after each search $\pi\leftarrow\pi+d$. The invariant "no negative residual cycle" is exactly optimality of the current flow (Busacker-Gowen).

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| One augmentation | $O(E\log V)$ (Dijkstra) | $O(V+E)$ |
| Total | $O(F\,E\log V)$ | |
| Bellman-Ford baseline | $O(F\,VE)$ | |

## 4. Quickstart & Usage

Header-only; add `modules/optimization/min-cost-flow/include` to the include path (CMake target `min_cost_flow`).

```cpp
#include "min_cost_flow.hpp"

algo::MinCostFlow g(4);
g.add_edge(0, 1, 2, 1); g.add_edge(0, 2, 1, 2); g.add_edge(1, 3, 1, 3); g.add_edge(2, 3, 2, 1);
auto r = g.min_cost_flow(0, 3, /*requested=*/2);   // r.flow, r.cost, r.edge_flows
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | successive shortest paths with full Bellman-Ford each round |
| Practical baseline | SSP with SPFA (queue-based Bellman-Ford) |
| Near-optimal heuristic | greedy SSP on original costs that never undoes a routed unit (no reverse arcs) |
| Optimal | Dijkstra + potentials (this module) |

Random sparse networks, capacities 1-20, costs 1-100, requested flow $0.3n+5$. All exact strategies must match flow and cost (checked). The heuristic's ratio compares cost per unit of flow.

### Random sparse network

<!-- BENCH:results/bench.csv workload=random-sparse -->
| n | Dijkstra + potentials (module) | Bellman-Ford SSP (brute force) | SPFA SSP (baseline) | greedy no-undo (heuristic) |
| ---: | ---: | ---: | ---: | ---: |
| 50 | 0.181 | 0.122 | 0.069 | 0.115 |
| 100 | 0.586 | 0.853 | 0.411 | 0.401 |
| 200 | 4.073 | 3.889 | 1.257 | 1.594 |
| 400 | 5.983 | 5.847 | 2.555 | 1.912 |
| 800 | 7.629 | 17.445 | 15.703 | 9.223 |
| 1,600 | 31.629 | 33.959 | 16.440 | 12.617 |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Cost per unit flow vs optimum

<!-- BENCH:results/bench.csv workload=random-sparse value=ratio -->
| n | Dijkstra + potentials (module) | Bellman-Ford SSP (brute force) | SPFA SSP (baseline) | greedy no-undo (heuristic) |
| ---: | ---: | ---: | ---: | ---: |
| 50 | 1.000 | 1.000 | 1.000 | 1.003 |
| 100 | 1.000 | 1.000 | 1.000 | 1.000 |
| 200 | 1.000 | 1.000 | 1.000 | 1.030 |
| 400 | 1.000 | 1.000 | 1.000 | 1.044 |
| 800 | 1.000 | 1.000 | 1.000 | 1.016 |
| 1,600 | 1.000 | 1.000 | 1.000 | 1.015 |

_ratio (see the `extra` column of the CSV)._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/random-sparse.png" alt="random-sparse (results/bench.csv)"><br><sub>random-sparse (results/bench.csv)</sub></td><td width="50%"><img src="results/figures/random-sparse-quality.png" alt="random-sparse quality (results/bench.csv)"><br><sub>random-sparse quality (results/bench.csv)</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* **Honest result**: at these sizes (small requested flow, sparse graphs) the module is only comparable to Bellman-Ford SSP and about 1.9x *slower* than SPFA at $n=1600$: SPFA's queue relaxations are very cheap on sparse random graphs, and the module's per-augmentation Dijkstra/potential update has constant overhead. Dijkstra with potentials has the better worst-case bound and is the right choice for adversarial or dense costs.
* The greedy no-undo heuristic is fastest but returns solutions up to about 4.4% costlier per unit of flow (1.5% at $n=1600$).

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/min_cost_flow_bench.cpp`](bench/min_cost_flow_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) covers the cheapest of two paths, parallel arcs, insufficient capacity, zero requested flow, negative-cost acyclic edges, reverse residual arcs repairing a bad route, self-loops and zero capacities, input validation, capacity and conservation of the returned flows, permutation invariance, cost monotonicity in the requested flow, repeated calls, 64-bit costs, disconnected graphs, marginal cost on bottlenecks and non-negative reduced costs from the returned potentials.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target min_cost_flow_test
./build/debug-asan/mod_min-cost-flow/min_cost_flow_test
```

## 7. Limitations & Trade-Offs

* Integer costs/capacities; negative cycles are not handled.
* The sweep uses small requested flow, which favours cheap-per-iteration algorithms.

## 8. References

* R. G. Busacker and P. J. Gowen, 1960; N. Tomizawa, 1971; J. Edmonds and R. M. Karp, “Theoretical improvements in algorithmic efficiency for network flow problems,” *J. ACM* 19(2), 1972.
* R. K. Ahuja, T. L. Magnanti, J. B. Orlin, *Network Flows*, Prentice Hall, 1993.
