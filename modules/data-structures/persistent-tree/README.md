# Persistent map

A fully persistent ordered map (path copying over a balanced tree): every update yields a new immutable version while old versions stay queryable.

## 1. Problem Statement

* **Input**: `insert(version, key, value)` and `erase(version, key)` producing a new version id; queries `find`, `contains`, `size`, `kth`, `rank`, `to_vector` on any version.
* **Output**: a `Version` handle (version 0 is the empty map).
* **Invariant**: a version is never modified after creation; all versions share unchanged subtrees.

## 2. Algorithm Overview & Mathematical Model

Updates copy only the nodes on the root-to-leaf path of the touched key and share every other subtree (*path copying*, Driscoll, Sarnak, Sleator, Tarjan 1989). With a treap-balanced tree (random priorities, expected height $h=O(\log n)$), each update allocates $O(\log n)$ nodes, so $n$ updates cost $O(n\log n)$ total space instead of the $\Theta(n^2)$ of copying the whole map for each version. Subtree sizes support `kth`/`rank` on any version.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| `insert`, `erase` (new version) | $O(\log n)$ time | $O(\log n)$ new nodes |
| `find`, `contains`, `rank`, `kth` on any version | $O(\log n)$ | $O(1)$ |
| Total after $m$ updates | | $O(m \log n)$ nodes |

## 4. Quickstart & Usage

Header-only; add `modules/data-structures/persistent-tree/include` to the include path (CMake target `persistent_tree`).

```cpp
#include "persistent_tree.hpp"

algo::PersistentMap<int, int> m;
auto v1 = m.insert(m.empty_version(), 5, 50);
auto v2 = m.insert(v1, 7, 70);
auto v3 = m.erase(v2, 5);
m.contains(v1, 5);   // true  (old version unchanged)
m.contains(v3, 5);   // false
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Brute force | a full `std::map` copy per version ($O(n)$ time and space each; shown up to $n=4096$) |
| Lower bound | ephemeral `std::map` (no history, cannot answer old-version queries) |
| This module | path-copying persistent map |

### Build n versions

<!-- BENCH:results/bench.csv workload=build-versions -->
| n | persistent map (module) | full copy per version (brute force) | ephemeral std::map (no history) |
| ---: | ---: | ---: | ---: |
| 256 | 0.168 | 3.302 | 0.022 |
| 512 | 0.114 | 17.091 | 0.056 |
| 1,024 | 0.276 | 80.102 | 0.302 |
| 2,048 | 0.930 | 438.000 | 0.343 |
| 4,096 | 3.868 | 4124.001 | 0.908 |
| 16,384 | 51.766 |  | 11.089 |
| 65,536 | 233.912 |  | 89.016 |
| 262,144 | 1702.863 |  | 432.552 |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Allocated history nodes (lower is better)

<!-- BENCH:results/bench.csv workload=build-versions value=nodes -->
| n | persistent map (module) | full copy per version (brute force) | ephemeral std::map (no history) |
| ---: | ---: | ---: | ---: |
| 256 | 3085.000 | 32896.000 | 256.000 |
| 512 | 6546.000 | 131328.000 | 512.000 |
| 1,024 | 14297.000 | 524800.000 | 1024.000 |
| 2,048 | 32118.000 | 2098176.000 | 2048.000 |
| 4,096 | 68474.000 | 8390656.000 | 4096.000 |
| 16,384 | 315073.000 |  | 16384.000 |
| 65,536 | 1469527.000 |  | 65536.000 |
| 262,144 | 6532011.000 |  | 262144.000 |

_nodes (see the `extra` column of the CSV)._
<!-- /BENCH -->

### Lookups against random old versions

<!-- BENCH:results/bench.csv workload=query-old-versions -->
| n | persistent map (module) | full copy per version (brute force) |
| ---: | ---: | ---: |
| 256 | 0.009 | 0.010 |
| 512 | 0.029 | 0.036 |
| 1,024 | 0.110 | 2.094 |
| 2,048 | 0.190 | 5.906 |
| 4,096 | 0.974 | 11.478 |
| 16,384 | 16.742 |  |
| 65,536 | 145.924 |  |
| 262,144 | 790.871 |  |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/build-versions.png" alt="build-versions (results/bench.csv)"><br><sub>build-versions (results/bench.csv)</sub></td><td width="50%"><img src="results/figures/query-old-versions.png" alt="query-old-versions (results/bench.csv)"><br><sub>query-old-versions (results/bench.csv)</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* Space is the headline: at $n=4096$ the full-copy brute force stores $n(n+1)/2\approx 8.4$M nodes, while the persistent map stores 68,474 (see the `nodes` table above): about 120x less space and 1000x less build time (3.9 ms vs 4.1 s), a gap that widens linearly with $n$. At $n=2^{18}$ the persistent map keeps all 262,144 versions in about 6.5M nodes (about 25 per update, i.e. $\approx\log_2 n$ plus rebalancing copies).
* Time overhead versus an ephemeral `std::map` is about 4x for building (allocation of copied paths) in exchange for full history.
* Brute-force copying is the faster option only for tiny maps.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/persistent_tree_bench.cpp`](bench/persistent_tree_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) checks that old versions are unchanged by later updates, a random version DAG against per-version snapshots (100 seeds), immutability after 20,000 updates, history independence of the tree shape, rank/kth inversion, custom comparators, and 200,000-update linear histories. It also bounds structural sharing: at most $2(\mathrm{depth}+1)+2$ nodes per update, $O(\lg n)$ depth on sorted/reverse/bit-reversal orders, and a fan of 10,000 versions from a common root. A $10^6$-key version is verified against a sorted vector.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target persistent_tree_test
./build/debug-asan/mod_persistent-tree/persistent_tree_test
```

## 7. Limitations & Trade-Offs

* Versions are never garbage collected (arena only grows).
* Single-threaded writer; no merge of divergent versions (that would be *confluent* persistence).
* Memory is about 25 nodes per update at $n=2^{18}$.

## 8. References

* J. R. Driscoll, N. Sarnak, D. D. Sleator, R. E. Tarjan, “Making data structures persistent,” *J. Comput. Syst. Sci.* 38(1), 1989.
* C. Okasaki, *Purely Functional Data Structures*, Cambridge University Press, 1998.
