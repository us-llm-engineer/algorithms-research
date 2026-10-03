# Christofides metric TSP tour

A 3/2-approximation for metric travelling salesman, compared against brute force, the exact Held-Karp DP and three classical heuristics.

## 1. Problem Statement

* **Input**: a finite, complete, symmetric distance matrix with zero diagonal satisfying the triangle inequality.
* **Output**: a Hamiltonian cycle (`tour`) and its `cost`.
* **Guarantee**: cost $\le \tfrac32\,\mathrm{OPT}$ when the metric assumptions hold and matching is exact.

## 2. Algorithm Overview & Mathematical Model

1. Build a minimum spanning tree $T$ ($w(T)\le \mathrm{OPT}$, since deleting an edge of an optimal tour leaves a spanning path).
2. Let $O$ be the odd-degree vertices of $T$ ($|O|$ is even). Compute a minimum-weight perfect matching $M$ on $O$; $w(M)\le \mathrm{OPT}/2$ because the optimal tour shortcut onto $O$ splits into two perfect matchings.
3. $T\cup M$ is connected with all degrees even, so it has an Euler tour of weight $\le \tfrac32\mathrm{OPT}$.
4. Shortcut repeated vertices; the triangle inequality means this never increases the cost.

Matching is solved exactly by subset DP for up to 22 odd vertices and greedily beyond that (valid tours, but the $3/2$ bound is then not claimed).

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| Spanning tree (Prim, dense) | $O(n^2)$ | $O(n)$ |
| Matching, $k\le22$ odd vertices | $O(2^k k^2)$ | $O(2^k)$ |
| Matching, larger instances | greedy $O(k^2\log k)$ | no formal bound |
| Euler tour and shortcut | $O(n^2)$ | $O(n)$ |

## 4. Quickstart & Usage

Header-only; add `modules/approximation/christofides/include` to the include path (CMake target `christofides`).

```cpp
#include "christofides.hpp"

std::vector<std::vector<double>> d = {{0, 1, 2}, {1, 0, 1.5}, {2, 1.5, 0}};
auto result = algo::christofides(d);   // result.tour, result.cost
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | enumerate all $(n-1)!$ tours (shown to $n=10$) |
| Optimal (exact) | Held-Karp subset DP, $O(2^n n^2)$ (shown to $n=16$) |
| Cheap heuristic | nearest neighbour, $O(n^2)$ |
| 2-approximation | double-tree: preorder walk of the MST |
| Near-optimal heuristic | nearest neighbour followed by 2-opt local search |
| This module | Christofides (3/2-approximation) |

One seeded random Euclidean instance per size (uniform in the unit square). Quality is cost divided by the best cost found at that size, which is the true optimum for $n\le16$ where an exact solver ran.

### Wall time

<!-- BENCH:results/bench.csv workload=random-euclid -->
| n | brute force (permutations) | Held-Karp DP (exact) | nearest neighbour (heuristic) | double-tree MST (2-approx) | nearest neighbour + 2-opt (heuristic) | christofides |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 6 | 0.009 | 0.003 | 0.001 | 0.002 | 0.001 | 0.004 |
| 8 | 0.473 | 0.061 | 0.001 | 0.002 | 0.001 | 0.005 |
| 9 | 4.204 | 0.043 | 0.001 | 0.002 | 0.002 | 0.003 |
| 10 | 58.664 | 0.152 | 0.001 | 0.004 | 0.004 | 0.006 |
| 12 |  | 0.753 | 0.001 | 0.003 | 0.003 | 0.006 |
| 14 |  | 7.655 | 0.001 | 0.004 | 0.004 | 0.007 |
| 16 |  | 34.829 | 0.001 | 0.003 | 0.005 | 0.007 |
| 32 |  |  | 0.003 | 0.010 | 0.030 | 0.398 |
| 64 |  |  | 0.012 | 0.036 | 0.104 | 0.057 |
| 128 |  |  | 0.051 | 0.146 | 0.672 | 0.223 |
| 256 |  |  | 0.194 | 0.517 | 3.468 | 1.010 |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Tour cost / best known (1.0 = optimal for $n\le16$)

<!-- BENCH:results/bench.csv workload=random-euclid value=ratio -->
| n | brute force (permutations) | Held-Karp DP (exact) | nearest neighbour (heuristic) | double-tree MST (2-approx) | nearest neighbour + 2-opt (heuristic) | christofides |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 6 | 1.000 | 1.000 | 1.032 | 1.227 | 1.000 | 1.000 |
| 8 | 1.000 | 1.000 | 1.059 | 1.234 | 1.000 | 1.046 |
| 9 | 1.000 | 1.000 | 1.104 | 1.104 | 1.000 | 1.000 |
| 10 | 1.000 | 1.000 | 1.248 | 1.193 | 1.000 | 1.000 |
| 12 |  | 1.000 | 1.032 | 1.000 | 1.012 | 1.033 |
| 14 |  | 1.000 | 1.082 | 1.192 | 1.000 | 1.087 |
| 16 |  | 1.000 | 1.187 | 1.246 | 1.000 | 1.078 |
| 32 |  |  | 1.301 | 1.184 | 1.000 | 1.065 |
| 64 |  |  | 1.161 | 1.315 | 1.000 | 1.128 |
| 128 |  |  | 1.151 | 1.253 | 1.000 | 1.130 |
| 256 |  |  | 1.145 | 1.289 | 1.000 | 1.168 |

_ratio (see the `extra` column of the CSV)._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/random-euclid.png" alt="random-euclid (results/bench.csv)"><br><sub>How christofides scales on random Euclidean TSP instances with n cities in the unit square. Largest size n = 256: christofides needs 1.01 ms, nearest neighbour (heuristic) only 0.194 ms and nearest neighbour + 2-opt (heuristic) 3.47 ms.</sub></td><td width="50%"><img src="results/figures/random-euclid-quality.png" alt="random-euclid quality (results/bench.csv)"><br><sub>Quality versus the exact reference for random Euclidean TSP instances with n cities in the unit square (christofides). 1.0 means optimal. At n = 256, christofides scores 1.168; double-tree MST (2-approx) is furthest from 1.0 at 1.289. Data: approximation/christofides/results/bench.csv.</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* The exact solvers hit a wall quickly: brute force needs 59 ms at $n=10$ and Held-Karp 35 ms at $n=16$ (growing by about $2\times$ per added vertex), while every heuristic stays below 4 ms at $n=256$.
* **Honest result**: on these instances nearest-neighbour + 2-opt gives the best tours at every size (it matches the optimum wherever an exact solver ran and is the reference for $n>16$), and Christofides is up to 17% above it (3-9% for $n\le32$, 13-17% for $n\ge64$). Christofides beats the double-tree 2-approximation at every size except $n=12$, but plain nearest neighbour is closer to the best tour at $n=256$ (14.5% vs 16.8% above); with one instance per size the curves are noisy.
* Christofides' worst-case guarantee ($\le1.5\times$ optimal) is real, but typical-case quality of local search is better. Christofides runs about 3.4x faster than 2-opt at $n=256$.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/christofides_bench.cpp`](bench/christofides_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) covers worked examples (unit square, triangle, line metric, singleton), input validation (empty, non-square, negative and asymmetric edges), the Hamiltonian-cycle invariants (every vertex exactly once, reported cost equals the selected edges), the three-halves bound on small random metrics, duplicate points, ties and determinism.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target christofides_test
./build/debug-asan/mod_christofides/christofides_test
```

## 7. Limitations & Trade-Offs

* The exact matching is exponential in the number of odd vertices; above 22 the greedy matching voids the formal bound.
* Requires a true metric; non-metric inputs may produce valid but unbounded tours.
* $O(n^2)$ matrix input limits instance size.
* Quality figures use one instance per size.

## 8. References

* N. Christofides, “Worst-case analysis of a new heuristic for the travelling salesman problem,” Carnegie Mellon University technical report, 1976.
* A. van Zuylen and D. P. Williamson, 2009; A. Karlin, N. Klein, S. Oveis Gharan, “A (slightly) improved approximation algorithm for metric TSP,” *STOC* 2021.
* M. Held and R. M. Karp, “A dynamic programming approach to sequencing problems,” *J. SIAM* 10(1), 1962.
* G. A. Croes, “A method for solving traveling-salesman problems,” *Operations Research* 6(6), 1958 (2-opt).
