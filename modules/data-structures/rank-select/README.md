# Rank/select bit vector

A succinct bit vector answering `rank` and `select` in (near-)constant time with a configurable space/time trade-off.

## 1. Problem Statement

* **Input**: a bit sequence $B[0..n)$ (packed 64-bit words or `vector<bool>`).
* **Output**: `access(i)`, `rank1(i)` = ones in $[0,i)$, `rank0`, `select1(k)` = position of the $k$-th one, `select0`.
* **Constraints**: indices are bounds-checked and throw `std::out_of_range`.

## 2. Algorithm Overview & Mathematical Model

Auxiliary directories sit beside the raw words. Rank uses a two-level counter hierarchy (superblock absolute counts, block-relative counts, then one `popcount` inside a word); select samples every $k$-th one and finishes with a bounded scan or an in-word select. Two layouts expose the classic trade-off:

* **Compact**: small directory (about 4% of $n$ in these runs), a few more probes per query.
* **Fast**: denser directory (about 26%), fewer memory reads per query.

`last_probes()` reports the 64-bit words touched by the most recent query so probe counts can be tested.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| `access`, `rank` | $O(1)$ | Compact: $o(n)$ extra bits |
| `select` | $O(1)$ in practice (bounded scan after sample lookup) | |
| Build | $O(n/64)$ word operations | |
| Storage | | $n + $ `overhead_bits()` bits |

## 4. Quickstart & Usage

Header-only; add `modules/data-structures/rank-select/include` to the include path (CMake target `rank_select`).

```cpp
#include "rank_select.hpp"

std::vector<bool> bits = {1, 0, 1, 1, 0, 0, 1};
algo::RankSelect rs(bits, algo::RankSelect::Layout::Compact);
rs.rank1(4);     // 3 ones before position 4
rs.select1(2);   // third one is at position 3
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | linear popcount scan from the start (shown up to $n=2^{18}$) |
| Speed-optimal, space-heavy | one 64-bit prefix counter per word (about 100% overhead) + binary search for select |
| This module | `Compact` and `Fast` layouts |

The `n` column of the tables is $\log_2$ of the bit-vector length. Each cell times 100,000 queries.

### 100,000 rank queries

<!-- BENCH:results/bench.csv workload=rank -->
| n | rank-select Compact | rank-select Fast | prefix-count array | linear scan (brute force) |
| ---: | ---: | ---: | ---: | ---: |
| 14 | 2.305 | 0.887 | 0.196 | 9.002 |
| 16 | 2.188 | 0.725 | 0.164 | 55.150 |
| 18 | 2.233 | 1.382 | 0.770 | 233.186 |
| 20 | 2.142 | 0.796 | 0.290 |  |
| 22 | 1.861 | 2.161 | 0.622 |  |
| 24 | 1.842 | 1.016 | 0.507 |  |
| 26 | 7.600 | 3.333 | 2.117 |  |

_Median wall time in milliseconds._
<!-- /BENCH -->

### 100,000 select queries

<!-- BENCH:results/bench.csv workload=select -->
| n | rank-select Compact | rank-select Fast | prefix-count array | linear scan (brute force) |
| ---: | ---: | ---: | ---: | ---: |
| 14 | 19.025 | 15.347 | 7.616 | 14.709 |
| 16 | 26.945 | 11.083 | 7.935 | 81.542 |
| 18 | 40.503 | 22.961 | 15.927 | 285.479 |
| 20 | 28.356 | 15.739 | 12.145 |  |
| 22 | 35.354 | 38.912 | 22.712 |  |
| 24 | 28.937 | 30.965 | 27.399 |  |
| 26 | 54.372 | 36.271 | 57.636 |  |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/rank.png" alt="rank (results/bench.csv)"><br><sub>Median time per strategy for 100,000 rank queries on 2^n random bits (rank-select). Both axes are logarithmic. At n = 26, rank-select Fast takes 3.33 ms (rank 2 of 3); prefix-count array is fastest at 2.12 ms.</sub></td><td width="50%"><img src="results/figures/select.png" alt="select (results/bench.csv)"><br><sub>How rank-select scales on 100,000 select queries on 2^n random bits. Both axes are logarithmic. At n = 26, rank-select Fast is fastest at 36.3 ms; prefix-count array is slowest at 57.6 ms. Source: data-structures/rank-select/results/bench.csv.</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* The linear scan grows linearly with $n$ and is about 100x slower than the compact directory (233 ms vs 2.2 ms) at $n=2^{18}$ and grows linearly (it is omitted beyond that).
* At $n=2^{26}$ `Fast` rank is about 2.3x faster than `Compact` at 6x the overhead; the full prefix-counter array is 1.6x faster again than `Fast` but costs 100% overhead versus 4% for `Compact`.
* For select at $n=2^{26}$, `Fast` is fastest (36 ms); the prefix-counter array (58 ms) is no better than `Compact` (54 ms) because its binary search over $n/64$ counters costs cache misses.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/rank_select_bench.cpp`](bench/rank_select_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite covers identical answers for both layouts and both constructors, the space bounds (Compact $\le10\%$, Fast $\le40\%$ overhead for $n\ge2^{16}$), $O(1)$ words touched by `rank`, logarithmic probe counts for `select` on clustered/gappy/extreme distributions, bits placed on directory boundaries (63|64, 511|512, 4095|4096), all-ones and all-zeros vectors at $2^{20}$, copy/move independence, and a scale test at $n=2^{24}$ with $10^6$ random queries per kind. `check_invariants()` recomputes every counter from the raw bits.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target rank_select_test
./build/debug-asan/mod_rank-select/rank_select_test
```

## 7. Limitations & Trade-Offs

* Static: no updates after construction.
* `select` is not worst-case $O(1)$ on adversarial bit patterns (long sparse gaps).
* Memory figures count directory bits only, not allocator overhead.

## 8. References

* G. Jacobson, “Space-efficient static trees and graphs,” *FOCS* 1989.
* S. Vigna, “Broadword implementation of rank/select queries,” *WEA* 2008.
* G. Navarro, *Compact Data Structures*, Cambridge University Press, 2016.
