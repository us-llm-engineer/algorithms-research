# Dynamic-programming optimizations

Divide-and-conquer optimization for $k$-segment partition cost and Knuth-style monotone-split speedup for optimal interval merging, each with an exact baseline.

## 1. Problem Statement

* **Partition**: given $a_1..a_n\ge0$ and $k$, split into exactly $k$ non-empty contiguous segments minimizing $\sum (\text{segment sum})^2$.
* **Optimal merge**: given weights $w_1..w_n\ge 0$, merge adjacent items until one remains, paying the merged weight each time, minimizing total cost.
* **Fallback**: with negative values monotonicity is not guaranteed, so the module uses the exact quadratic recurrence.

## 2. Algorithm Overview & Mathematical Model

Partition: $f_g(j)=\min_{i<j} f_{g-1}(i)+(S_j-S_i)^2$. The cost satisfies the quadrangle inequality, so the optimal split point $\mathrm{opt}_g(j)$ is non-decreasing in $j$. Divide-and-conquer solves the middle $j$ by scanning its allowed range and recurses on the two halves with narrowed ranges: $O(n\log n)$ per layer instead of $O(n^2)$.

Optimal merge: $dp[l][r]=\min_{l\le s<r} dp[l][s]+dp[s+1][r]+W(l,r)$ and Knuth's bound $\mathrm{opt}[l][r-1]\le \mathrm{opt}[l][r]\le \mathrm{opt}[l+1][r]$ reduces total work from $O(n^3)$ to $O(n^2)$. `partition_decisions_monotone` checks the monotonicity property on a given input.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| Partition, naive | $O(kn^2)$ | $O(n)$ |
| Partition, divide-and-conquer | $O(kn\log n)$ | $O(n)$ |
| Optimal merge, naive | $O(n^3)$ | $O(n^2)$ |
| Optimal merge, Knuth bound | $O(n^2)$ | $O(n^2)$ |

## 4. Quickstart & Usage

Header-only; add `modules/optimization/dp-optimization/include` to the include path (CMake target `dp_optimization`).

```cpp
#include "dp_optimization.hpp"

std::vector<std::int64_t> a{3, 1, 4, 1, 5, 9, 2, 6};
auto cost = algo::DpOptimization::partition_squared(a, /*segments=*/3);
auto merge = algo::DpOptimization::optimal_merge(a);
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | recursion over every split ($n\le32$, $k=16$) |
| Baseline | exact quadratic DP (`partition_squared_naive`, `optimal_merge_naive`) |
| Optimal | divide-and-conquer / Knuth-bound DP (this module) |

All strategies must agree on the optimum (checked). Partition uses $k=16$.

### Partition into 16 segments minimizing squared sums

<!-- BENCH:results/bench.csv workload=partition-squared -->
| n | divide-and-conquer DP (module) | quadratic DP (baseline) | brute force (all splits) |
| ---: | ---: | ---: | ---: |
| 16 | 0.004 | 0.004 | 0.000 |
| 24 | 0.008 | 0.010 | 20.305 |
| 32 | 0.011 | 0.021 | 7742.816 |
| 128 | 0.073 | 0.441 |  |
| 512 | 0.436 | 6.456 |  |
| 2,048 | 1.456 | 164.425 |  |
| 8,192 | 7.449 | 1818.719 |  |
| 32,768 | 66.388 |  |  |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Optimal adjacent merge

<!-- BENCH:results/bench.csv workload=optimal-merge -->
| n | monotone-split DP (module) | cubic DP (baseline) |
| ---: | ---: | ---: |
| 32 | 0.015 | 0.022 |
| 64 | 0.058 | 0.145 |
| 128 | 0.225 | 1.078 |
| 256 | 1.837 | 21.791 |
| 512 | 19.972 | 227.470 |
| 1,024 | 100.565 |  |
| 2,048 | 896.515 |  |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/partition-squared.png" alt="partition-squared (results/bench.csv)"><br><sub>partition-squared (results/bench.csv)</sub></td><td width="50%"><img src="results/figures/optimal-merge.png" alt="optimal-merge (results/bench.csv)"><br><sub>optimal-merge (results/bench.csv)</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* Partition: the divide-and-conquer DP is about 240x faster than the quadratic DP at $n=8192$ (7.4 ms vs 1.8 s) and handles $n=32768$ in 66 ms, where the quadratic DP was not run.
* Optimal merge: the Knuth-bound DP is 11x faster than the cubic baseline at $n=512$ (20 ms vs 227 ms) and reaches $n=2048$ in under a second.
* The brute force is only feasible for $n\le32$; the plots show its explosive growth.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/dp_optimization_bench.cpp`](bench/dp_optimization_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) checks worked examples, one segment, one item per segment, invalid segment counts, monotone weights against the quadratic reference, exact arithmetic with negative values, the Knuth merge example, singleton and empty merges, monotonicity of the optimum as segments grow, the optimized partition against the quadratic oracle and the optimized merge against the cubic oracle, 64-bit exactness on large inputs, monotonicity certificates and determinism.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target dp_optimization_test
./build/debug-asan/mod_dp-optimization/dp_optimization_test
```

## 7. Limitations & Trade-Offs

* Speedups require non-negative inputs (otherwise silently the quadratic path).
* Costs use 64-bit arithmetic with overflow checking.
* Only these two problem shapes; the technique does not apply to cost functions that violate the quadrangle inequality.

## 8. References

* D. E. Knuth, “Optimum binary search trees,” *Acta Informatica* 1, 1971.
* F. F. Yao, “Efficient dynamic programming using quadrangle inequalities,” *STOC* 1980.
* Z. Galil and K. Park, “Dynamic programming with convexity, concavity and sparsity,” *Theoretical Computer Science* 92, 1992.
