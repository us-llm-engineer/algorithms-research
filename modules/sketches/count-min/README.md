# Count-Min sketch

A fixed-memory frequency estimator that never underestimates, with mergeable sketches and overflow-checked counters.

## 1. Problem Statement

* **Input**: a stream of keys with positive increments.
* **Output**: `estimate(key)` $\ge$ true count, with $\hat f\le f+\varepsilon N$ with probability $\ge1-\delta$ for width $w=\lceil e/\varepsilon\rceil$ and depth $d=\lceil\ln(1/\delta)\rceil$.
* **Constraints**: sketches merge only when width, depth and seed match.

## 2. Algorithm Overview & Mathematical Model

A $d\times w$ array of counters with one seeded hash per row (`std::hash` followed by a 64-bit mixer; not provably pairwise independent). `add` increments one counter per row; `estimate` returns the row-wise minimum. Each row's counter overestimates by the colliding mass, whose expectation is at most $N/w$ (Markov: $\le \varepsilon N$ with probability $\ge1/2$ per row), and the minimum over $d$ independent rows drives the failure probability to $e^{-d}$.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| `add`, `estimate` | $O(d)$ | |
| `merge` | $O(wd)$ | |
| Storage | | $O(wd)$ 64-bit counters, independent of the stream length |

## 4. Quickstart & Usage

Header-only; add `modules/sketches/count-min/include` to the include path (CMake target `count_min`).

```cpp
#include "count_min.hpp"

algo::CountMinSketch<std::string> cm(/*width=*/2048, /*depth=*/4, /*seed=*/1);
cm.add("apple"); cm.add("apple", 3);
cm.estimate("apple");     // >= 4
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | sort the stream and count runs (exact, $O(n)$ memory) |
| Exact | hash map of counts (memory grows with distinct keys) |
| Near-optimal heuristic | Misra-Gries summary with $k=1024$ counters (deterministic, **underestimates**) |
| This module | Count-Min, $2048\times4$ = 64 KiB |

Zipf(1.1) stream of $n$ items over $n/4$ keys. The accuracy ratio is the mean `estimate / true count` over the 1000 most frequent keys.

### Time to process the stream

<!-- BENCH:results/bench.csv workload=zipf-stream -->
| n | exact hash map | sort + count runs (brute force) | Misra-Gries k=1024 (heuristic) | Count-Min 2048x4 |
| ---: | ---: | ---: | ---: | ---: |
| 16,384 | 0.671 | 0.989 | 0.736 | 1.365 |
| 65,536 | 6.252 | 4.664 | 4.078 | 7.538 |
| 262,144 | 47.994 | 26.259 | 22.512 | 19.877 |
| 1,048,576 | 287.558 | 99.100 | 89.949 | 124.493 |
| 4,194,304 | 689.259 | 371.895 | 401.445 | 346.824 |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Estimate / true count on the top-1000 keys

<!-- BENCH:results/bench.csv workload=zipf-stream value=ratio -->
| n | exact hash map | sort + count runs (brute force) | Misra-Gries k=1024 (heuristic) | Count-Min 2048x4 |
| ---: | ---: | ---: | ---: | ---: |
| 16,384 | 1.000 | 1.000 | 0.277 | 1.083 |
| 65,536 | 1.000 | 1.000 | 0.167 | 1.310 |
| 262,144 | 1.000 | 1.000 | 0.106 | 1.581 |
| 1,048,576 | 1.000 | 1.000 | 0.088 | 1.790 |
| 4,194,304 | 1.000 | 1.000 | 0.078 | 1.976 |

_ratio (see the `extra` column of the CSV)._
<!-- /BENCH -->

### Memory (KiB)

<!-- BENCH:results/bench.csv workload=zipf-stream value=memory_kb -->
| n | exact hash map | sort + count runs (brute force) | Misra-Gries k=1024 (heuristic) | Count-Min 2048x4 |
| ---: | ---: | ---: | ---: | ---: |
| 16,384 | 86.000 | 128.000 | 16.000 | 64.000 |
| 65,536 | 304.000 | 512.000 | 16.000 | 64.000 |
| 262,144 | 1087.000 | 2048.000 | 16.000 | 64.000 |
| 1,048,576 | 3890.000 | 8192.000 | 16.000 | 64.000 |
| 4,194,304 | 13985.000 | 32768.000 | 16.000 | 64.000 |

_memory_kb (see the `extra` column of the CSV)._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/zipf-stream.png" alt="zipf-stream (results/bench.csv)"><br><sub>Median time per strategy for a Zipf(1.1) stream of n items over n/4 distinct keys (count-min). At n = 4,194,304, Count-Min 2048x4 is fastest at 347 ms; exact hash map is slowest at 689 ms.</sub></td><td width="50%"><img src="results/figures/zipf-stream-quality.png" alt="zipf-stream quality (results/bench.csv)"><br><sub>count-min: result quality on a Zipf(1.1) stream of n items over n/4 distinct keys. At n = 4,194,304, Count-Min 2048x4 scores 1.976; Count-Min 2048x4 is furthest from 1.0 at 1.976. Data: sketches/count-min/results/bench.csv.</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* Count-Min processes the stream about 2x faster than the exact hash map using **64 KiB instead of 14 MB** at $n=2^{22}$, and it never underestimates.
* Accuracy is the cost: with a fixed $2048\times4$ sketch, the top-1000 keys are overestimated by about 2x on average at $n=2^{22}$ (error grows with $N/w$). To keep $\varepsilon$ fixed the width must grow with the stream or the accuracy degrades.
* Misra-Gries uses only 16 KiB but underestimates heavily on the top keys when the summary is small relative to the number of heavy hitters (ratio 0.08 here), the opposite failure mode.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/count_min_bench.cpp`](bench/count_min_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) covers the empty sketch, point updates never underestimating, unseen keys, weighted updates, randomized frequency bounds, constructor validation, `clear`, merge equal to pointwise addition, rejection of incompatible dimensions or seeds, heavy hitters, determinism per seed, counter-overflow handling, the collision-error bound (total mass over width) and repeated merge/clear cycles.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target count_min_test
./build/debug-asan/mod_count-min/count_min_test
```

## 7. Limitations & Trade-Offs

* Overestimates only; heavy collisions inflate the estimate for low-frequency keys.
* Fixed width: accuracy degrades as the stream grows.
* No deletion/negative updates (conservative-update and count-sketch variants not provided).

## 8. References

* G. Cormode and S. Muthukrishnan, “An improved data stream summary: the count-min sketch and its applications,” *J. Algorithms* 55(1), 2005.
* J. Misra and D. Gries, “Finding repeated elements,” *Science of Computer Programming* 2, 1982.
