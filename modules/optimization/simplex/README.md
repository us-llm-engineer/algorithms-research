# Simplex method

A dense-tableau two-phase simplex solver for $\max c^\top x$ s.t. $Ax\le b$, $x\ge0$ that returns primal, dual and certificate-checkable results.

## 1. Problem Statement

* **Input**: matrix $A\in\mathbb R^{m\times n}$, vector $b$, vector $c$ (all finite).
* **Output**: status (`Optimal`, `Infeasible`, `Unbounded`), primal $x$, objective, dual $y$.
* **Constraints**: $b$ may be negative (handled by phase 1 of the two-phase method).

## 2. Algorithm Overview & Mathematical Model

The feasible region is a polyhedron; if an optimum exists it is attained at a vertex (basic feasible solution). Simplex walks from vertex to adjacent vertex, each pivot increasing the objective, until all reduced costs are non-positive. Strong duality supplies a verifiable certificate: the returned dual $y\ge0$ satisfies $A^\top y\ge c$ and $b^\top y=c^\top x$, which `Result` can validate.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| One pivot | $O(mn)$ | $O(mn)$ dense tableau |
| Pivots | exponential worst case (Klee-Minty), typically $O(m)$-$O(m\log n)$ | |

## 4. Quickstart & Usage

Header-only; add `modules/optimization/simplex/include` to the include path (CMake target `simplex`).

```cpp
#include "simplex.hpp"

auto r = algo::Simplex::solve({{1, 1}, {1, 0}, {0, 1}}, {4, 3, 2}, {3, 2});
// r.status == Optimal, r.objective == 11, r.dual certifies it
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | vertex enumeration: solve every $n\times n$ basis of tight constraints (shown to $n=10$) |
| Near-optimal heuristic | greedy: raise variables in order of objective density until constraints block |
| Optimal | simplex (this module) |

Random dense LPs with $m=n$, $A_{ij}\in[0.1,1.1)$, $b_i\in[1,10)$, $c_j\in[1,10)$ (always feasible and bounded). The brute-force objective must match simplex (checked).

### Random dense LP

<!-- BENCH:results/bench.csv workload=random-dense-lp -->
| n | simplex (module) | greedy density (heuristic) | vertex enumeration (brute force) |
| ---: | ---: | ---: | ---: |
| 4 | 0.002 | 0.000 | 0.034 |
| 6 | 0.003 | 0.001 | 0.502 |
| 8 | 0.004 | 0.001 | 10.366 |
| 10 | 0.004 | 0.001 | 255.963 |
| 16 | 0.012 | 0.004 |  |
| 32 | 0.045 | 0.017 |  |
| 64 | 0.344 | 0.086 |  |
| 128 | 2.324 | 0.641 |  |
| 256 | 11.790 | 2.843 |  |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Objective / optimum (higher is better)

<!-- BENCH:results/bench.csv workload=random-dense-lp value=ratio -->
| n | simplex (module) | greedy density (heuristic) | vertex enumeration (brute force) |
| ---: | ---: | ---: | ---: |
| 4 | 1.000 | 0.783 | 1.000 |
| 6 | 1.000 | 1.000 | 1.000 |
| 8 | 1.000 | 0.907 | 1.000 |
| 10 | 1.000 | 0.970 | 1.000 |
| 16 | 1.000 | 0.864 |  |
| 32 | 1.000 | 0.629 |  |
| 64 | 1.000 | 0.456 |  |
| 128 | 1.000 | 0.563 |  |
| 256 | 1.000 | 0.520 |  |

_ratio (see the `extra` column of the CSV)._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/random-dense-lp.png" alt="random-dense-lp (results/bench.csv)"><br><sub>random-dense-lp (results/bench.csv)</sub></td><td width="50%"><img src="results/figures/random-dense-lp-quality.png" alt="random-dense-lp quality (results/bench.csv)"><br><sub>random-dense-lp quality (results/bench.csv)</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* Vertex enumeration explodes combinatorially: $\binom{2n}{n}$ bases make $n=10$ about 60,000x slower than simplex (256 ms vs 4 microseconds).
* The greedy heuristic is about 4x faster than simplex at $n=256$ (2.8 ms vs 11.8 ms) but reaches only **52%** of the optimal objective there: a fast feasible point, nowhere near optimal.
* Simplex solves a 256x256 dense LP in 12 ms.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/simplex_bench.cpp`](bench/simplex_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) covers bounded, infeasible and unbounded LPs, zero objective, zero rows/columns, redundant constraints, zero capacities, primal feasibility of every solution, the dual certificate bounding the primal, scaling of capacities and objective, permutation invariance, near-degenerate pivots, large coefficients and determinism.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target simplex_test
./build/debug-asan/mod_simplex/simplex_test
```

## 7. Limitations & Trade-Offs

* Dense floating point with fixed tolerances; ill-conditioned problems may need scaling.
* Not suitable for large sparse LPs (use a revised simplex or interior point method).
* Worst-case exponential pivots.

## 8. References

* G. B. Dantzig, *Linear Programming and Extensions*, Princeton University Press, 1963.
* V. Chvátal, *Linear Programming*, W. H. Freeman, 1983.
* V. Klee and G. J. Minty, “How good is the simplex algorithm?”, 1972.
