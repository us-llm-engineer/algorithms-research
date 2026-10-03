# HyperLogLog

Approximate distinct counting in $2^p$ bytes with relative standard error about $1.04/\sqrt{2^p}$, mergeable across streams.

## 1. Problem Statement

* **Input**: a stream of 64-bit values (hashed internally).
* **Output**: `estimate()` of the number of distinct values; `merge` of two sketches of equal precision.
* **Constraints**: precision $p\in[4,18]$.

## 2. Algorithm Overview & Mathematical Model

Hash each value to 64 bits. The first $p$ bits select a register; the number of leading zeros of the remaining bits plus one is a *rank*, and each register keeps the maximum rank it saw. A register holding $\rho$ suggests about $2^{\rho}$ distinct items fell into its bucket. The raw estimate is the harmonic mean
$$E=\alpha_m m^2\Big/\sum_j 2^{-M[j]},\qquad \alpha_m\approx\frac{0.7213}{1+1.079/m},\ m=2^p,$$
with linear-counting correction for small cardinalities ($E\le 2.5m$, using the number of empty registers) and a large-range correction. Relative standard error is $\approx1.04/\sqrt m$.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| `add` | $O(1)$ | |
| `estimate`, `merge` | $O(m)$ | |
| Storage | | $m=2^p$ bytes ($p=14$: 16 KiB) |

## 4. Quickstart & Usage

Header-only; add `modules/sketches/hyperloglog/include` to the include path (CMake target `hyperloglog`).

```cpp
#include "hyperloglog.hpp"

algo::HyperLogLog h(/*precision=*/14);
for (std::uint64_t i = 0; i < 1'000'000; ++i) h.add(i % 250'000);
double n = h.estimate();   // about 250000 (+/- 0.8%)
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | sort + unique (exact, $O(n)$ memory) |
| Exact | hash set |
| Near-optimal heuristic | linear counting on a 64 Kbit bitmap (accurate until the bitmap saturates) |
| This module | HyperLogLog at $p=10$ (1 KiB) and $p=14$ (16 KiB) |

Streams of $n$ items with $n/2$ distinct values; ratio is `estimate / true cardinality`.

### Time to process the stream

<!-- BENCH:results/bench.csv workload=distinct-count -->
| n | sort + unique (brute force) | hash set (exact) | linear counting 64 Kbit (heuristic) | HyperLogLog p=10 | HyperLogLog p=14 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 4,096 | 0.235 | 0.431 | 0.130 | 0.037 | 0.204 |
| 16,384 | 1.587 | 3.472 | 0.161 | 0.129 | 0.345 |
| 65,536 | 7.561 | 24.140 | 0.294 | 0.484 | 0.874 |
| 262,144 | 43.603 | 160.922 | 0.468 | 0.966 | 1.508 |
| 1,048,576 | 134.896 | 509.147 | 6.655 | 7.534 | 10.756 |
| 4,194,304 | 614.179 | 3426.733 | 7.594 | 18.045 | 17.921 |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Estimate / true cardinality

<!-- BENCH:results/bench.csv workload=distinct-count value=ratio -->
| n | sort + unique (brute force) | hash set (exact) | linear counting 64 Kbit (heuristic) | HyperLogLog p=10 | HyperLogLog p=14 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 4,096 | 1.000 | 1.000 | 1.002 | 0.964 | 0.991 |
| 16,384 | 1.000 | 1.000 | 1.003 | 0.965 | 1.001 |
| 65,536 | 1.000 | 1.000 | 1.000 | 1.050 | 0.989 |
| 262,144 | 1.000 | 1.000 | 1.001 | 0.997 | 1.015 |
| 1,048,576 | 1.000 | 1.000 | 1.032 | 0.999 | 0.999 |
| 4,194,304 | 1.000 | 1.000 | 0.347 | 1.017 | 1.001 |

_ratio (see the `extra` column of the CSV)._
<!-- /BENCH -->

### Memory (KiB)

<!-- BENCH:results/bench.csv workload=distinct-count value=memory_kb -->
| n | sort + unique (brute force) | hash set (exact) | linear counting 64 Kbit (heuristic) | HyperLogLog p=10 | HyperLogLog p=14 |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 4,096 | 32.000 | 80.000 | 8.000 | 1.000 | 16.000 |
| 16,384 | 128.000 | 320.000 | 8.000 | 1.000 | 16.000 |
| 65,536 | 512.000 | 1280.000 | 8.000 | 1.000 | 16.000 |
| 262,144 | 2048.000 | 5120.000 | 8.000 | 1.000 | 16.000 |
| 1,048,576 | 8192.000 | 20480.000 | 8.000 | 1.000 | 16.000 |
| 4,194,304 | 32768.000 | 81920.000 | 8.000 | 1.000 | 16.000 |

_memory_kb (see the `extra` column of the CSV)._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/distinct-count.png" alt="distinct-count (results/bench.csv)"><br><sub>distinct-count (results/bench.csv)</sub></td><td width="50%"><img src="results/figures/distinct-count-quality.png" alt="distinct-count quality (results/bench.csv)"><br><sub>distinct-count quality (results/bench.csv)</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* HyperLogLog is about **190x faster than the exact hash set** (18 ms vs 3.4 s at $n=2^{22}$) and 34x faster than sort-and-unique, with relative error 0.1% ($p=14$) to 1.7% ($p=10$) in this run, in 16 KiB or 1 KiB.
* Linear counting looks perfect while the bitmap is sparse but **saturates** once the distinct count passes the bitmap size (ratio 0.35 at $n/2=2^{21}$): a classic example of a heuristic with a hidden cliff, which HyperLogLog avoids.
* Errors are from one seeded stream per $n$, so single-run deviations are expected to scatter around $\pm1.04/\sqrt m$.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/hyperloglog_bench.cpp`](bench/hyperloglog_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) covers the empty estimate, duplicates, small and medium cardinalities, precision bounds, `clear`, deterministic hashing, high-bit and zero values, merge equal to the union estimate, rejection of different precisions, idempotent merge, error bounds across disjoint trials, error improving with precision, repeated merge trees and finite estimates at large cardinality.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target hyperloglog_test
./build/debug-asan/mod_hyperloglog/hyperloglog_test
```

## 7. Limitations & Trade-Offs

* Approximate: error scales as $1/\sqrt m$.
* No deletion; no per-item membership queries.
* Hash mixing is a fixed 64-bit mixer, not a cryptographic hash.

## 8. References

* P. Flajolet, É. Fusy, O. Gandouet, F. Meunier, “HyperLogLog: the analysis of a near-optimal cardinality estimation algorithm,” *AofA* 2007.
* S. Heule, M. Nunkesser, A. Hall, “HyperLogLog in practice,” *EDBT* 2013.
