# van Emde Boas tree

A predecessor/successor structure over a bounded integer universe in $O(\log\log U)$ time, in dense and hash-sparse variants.

## 1. Problem Statement

* **Input**: integers in the universe $[0, 2^u)$ with $u\le 24$ for the dense tree.
* **Output**: `insert`, `erase`, `contains`, `min`, `max`, `successor`, `predecessor`.
* **Constraints**: `VebTree` requires $1\le u\le 24$; `SparseVebTree` allocates only touched clusters.

## 2. Algorithm Overview & Mathematical Model

Split a $u$-bit key into a high half (cluster index) and a low half (position in cluster). A node stores `min` and `max` *outside* the recursive clusters plus a summary structure over non-empty clusters, so each operation makes **one** recursive call, giving $T(u)=T(u/2)+O(1)=O(\log u)=O(\log\log U)$. `last_depth()` and `last_calls()` expose recursion statistics for testing.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| `insert`, `erase`, `contains`, `successor`, `predecessor` | $O(\log\log U)$ | |
| `min`, `max` | $O(1)$ | |
| Dense storage | | $O(U)$ bits-ish (about 17 MB at $U=2^{24}$ in these runs) |
| Sparse storage | | proportional to $n$ (not $U$) |

## 4. Quickstart & Usage

Header-only; add `modules/data-structures/van-emde-boas/include` to the include path (CMake target `van_emde_boas`).

```cpp
#include "van_emde_boas.hpp"

algo::VebTree t(/*bits=*/16);
t.insert(100); t.insert(5000);
t.successor(100);     // 5000
t.predecessor(5000);  // 100
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | linear scan of a bitmap for the next set bit (shown up to $U=2^{20}$) |
| Baselines | sorted array + binary search; `std::set` |
| This module | dense `VebTree` and `SparseVebTree` |

`n` in the tables is the universe size $U$; the structure holds $U/16$ random keys; each cell times 200,000 successor queries or one full build.

### Successor queries

<!-- BENCH:results/bench.csv workload=successor -->
| n | van Emde Boas (dense) | van Emde Boas (sparse) | std::set | sorted array + binary search | bitmap scan (brute force) |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 4,096 | 5.472 | 9.393 | 8.209 | 8.702 | 0.866 |
| 16,384 | 6.971 | 14.011 | 12.182 | 11.461 | 0.995 |
| 65,536 | 9.225 | 18.693 | 20.762 | 15.064 | 1.015 |
| 262,144 | 10.173 | 27.617 | 36.371 | 17.531 | 0.907 |
| 1,048,576 | 10.180 | 32.607 | 45.278 | 18.681 | 0.992 |
| 4,194,304 | 25.724 | 108.894 | 205.685 | 24.631 |  |
| 16,777,216 | 73.708 | 157.059 | 230.394 | 47.390 |  |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Build from $U/16$ keys

<!-- BENCH:results/bench.csv workload=build -->
| n | van Emde Boas (dense) | van Emde Boas (sparse) | std::set | sorted array + binary search | bitmap scan (brute force) |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 4,096 | 0.004 | 0.015 | 0.017 | 0.003 | 0.000 |
| 16,384 | 0.106 | 0.189 | 0.086 | 0.028 | 0.001 |
| 65,536 | 0.331 | 0.824 | 0.467 | 0.223 | 0.004 |
| 262,144 | 1.347 | 4.355 | 3.199 | 0.947 | 0.015 |
| 1,048,576 | 4.535 | 16.649 | 15.810 | 3.746 | 0.084 |
| 4,194,304 | 52.657 | 172.755 | 419.769 | 18.599 |  |
| 16,777,216 | 586.257 | 1096.393 | 1851.197 | 90.667 |  |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/build.png" alt="build (results/bench.csv)"><br><sub>build (results/bench.csv)</sub></td><td width="50%"><img src="results/figures/successor.png" alt="successor (results/bench.csv)"><br><sub>successor (results/bench.csv)</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* Dense vEB successor is **about 3x faster than `std::set`** at $U=2^{24}$ (74 ms vs 230 ms for 200,000 queries) but only on par with a binary search over a *static* sorted array (47 ms): the array is contiguous and tiny (4 MB), the tree is 17 MB of pointers.
* The sparse variant is about 2x slower than dense and larger at this density (keys cover 1/16 of the universe, so sparsity does not help); it pays off when $n\ll U$ and $U$ is large.
* Bitmap scanning is competitive at small $U$ and degrades linearly; it is omitted past $2^{20}$.
* Dense build beats `std::set` build by about 3x.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/van_emde_boas_bench.cpp`](bench/van_emde_boas_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) checks the CLRS example, **exhaustively every subset of the universes of 1-4 bits**, range errors and constructor limits, random operations against `std::set` on many universe sizes (100 seeds each), min/max maintenance, odd bit-lengths, a completely full universe, dense $\Theta(U)$ versus sparse $O(n)$ memory, recursion depth $\le\lceil\lg\mathrm{bits}\rceil+1$ and calls per operation $\le 2\times$ that, adversarial cluster layouts, identical dense/sparse answers, deep copies, and a scale test with $2^{22}$ dense and $2^{32}$ sparse universes.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target van_emde_boas_test
./build/debug-asan/mod_van-emde-boas/van_emde_boas_test
```

## 7. Limitations & Trade-Offs

* Dense variant memory is proportional to the universe, not the key count.
* Keys are 32-bit; the dense tree is limited to $u\le 24$.
* For static data a sorted array is as fast and 4x smaller.

## 8. References

* P. van Emde Boas, “Preserving order in a forest in less than logarithmic time,” *FOCS* 1975.
* T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein, *Introduction to Algorithms*, 3rd ed., MIT Press, 2009. Chapter 20 (3rd ed. 20: van Emde Boas trees).
