# Fibonacci heap

A mergeable priority queue with amortized $O(1)$ `decrease_key`, index-based nodes and stable handles, benchmarked against a binary heap.

## 1. Problem Statement

* **Input**: a stream of `push(key)`, `pop()`, `decrease_key(handle, smaller_key)`, `erase(handle)` and `meld(other)` operations over a totally ordered key type.
* **Output**: the minimum key on demand; handles stay valid until their node is popped or erased.
* **Constraints**: `decrease_key` may only lower a key; handles are 32-bit indices into an arena, so at most $2^{32}-2$ live nodes.

## 2. Algorithm Overview & Mathematical Model

The heap is a forest of heap-ordered trees whose roots sit in a circular list with a pointer to the minimum.

* `push` adds a singleton root, $O(1)$. `meld` splices two root lists, $O(1)$.
* `pop` removes the minimum and **consolidates**: roots of equal degree are linked until all root degrees are distinct, so at most $D(n)+1$ roots remain.
* `decrease_key` cuts the node from its parent and, if the parent was already *marked* (had lost a child), cuts it too (**cascading cut**).

Potential function $\Phi = t + 2m$ ($t$ roots, $m$ marked nodes) gives amortized $O(1)$ for `decrease_key` and $O(\log n)$ for `pop`. The cascading-cut rule keeps every degree-$k$ subtree at least $F_{{k+2}}$ nodes, hence $D(n)\le \log_\varphi n$.

## 3. Complexity Analysis

| Operation / phase | Time | Space / notes |
| :--- | :--- | :--- |
| `push`, `meld`, `top` | $O(1)$ | $O(1)$ extra |
| `decrease_key` | $O(1)$ amortized | $O(1)$ |
| `pop`, `erase` | $O(\log n)$ amortized | $O(\log n)$ scratch for consolidation |
| Total storage | | $O(n)$ nodes in one arena (`reserve(n)` avoids reallocation) |

## 4. Quickstart & Usage

Header-only; add `modules/data-structures/fibonacci-heap/include` to the include path (CMake target `fibonacci_heap`).

```cpp
#include "fibonacci_heap.hpp"

algo::FibonacciHeap<int> heap;
auto a = heap.push(40);
auto b = heap.push(25);
heap.decrease_key(a, 10);          // handle-based update
int smallest = heap.pop();         // 10
```

## 5. Empirical Benchmarks

Measured on an Intel Core i7-10750H @ 2.60 GHz (12 threads), 7 GiB RAM, GCC 11.5 `-O3`, single thread. Each cell is the median of 3-9 repetitions after one warm-up; inputs are seeded (`common/rng.hpp`) so every run sees identical data. Regenerate with `python3 tools/run_benchmarks.py && python3 tools/plot.py`.

**Strategies compared** (brute force, baseline, near-optimal heuristic and optimal/exact tiers, all on the same inputs):

| Tier | Strategy |
| :--- | :--- |
| Optimal asymptotics | `fibonacci-heap` (this module) |
| Baseline | `std::priority_queue` binary heap |
| Practical alternative | binary heap with *lazy deletion* (push a duplicate instead of `decrease_key`) |

### Push n random keys, then pop all

<!-- BENCH:results/bench.csv workload=push-pop -->
| n | fibonacci-heap | std::priority_queue |
| ---: | ---: | ---: |
| 4,096 | 3.251 | 0.445 |
| 16,384 | 15.333 | 1.934 |
| 65,536 | 114.292 | 16.381 |
| 262,144 | 1076.842 | 59.128 |
| 1,048,576 | 8018.993 | 513.339 |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Push n, lower n random keys, pop all

<!-- BENCH:results/bench.csv workload=decrease-key -->
| n | fibonacci-heap | std::priority_queue (lazy) |
| ---: | ---: | ---: |
| 4,096 | 3.483 | 0.954 |
| 16,384 | 16.112 | 5.092 |
| 65,536 | 128.244 | 60.303 |
| 262,144 | 1703.073 | 464.014 |
| 1,048,576 | 5103.921 | 3843.700 |

_Median wall time in milliseconds._
<!-- /BENCH -->

### Figures

Dashed lines are brute-force strategies, dotted lines are heuristics or approximations, solid bold is this module. Axes are log-log; quality plots show result divided by the exact/optimal reference (1.0 is optimal).

<!-- FIGURES:results/bench.csv -->
<table>
<tr><td width="50%"><img src="results/figures/push-pop.png" alt="push-pop (results/bench.csv)"><br><sub>Median time per strategy for n random keys pushed into an empty heap and then all popped (fibonacci-heap). At n = 1,048,576, fibonacci-heap is slowest at 8,019 ms; std::priority_queue is fastest at 513 ms. Source: data-structures/fibonacci-heap/results/bench.csv.</sub></td><td width="50%"><img src="results/figures/decrease-key.png" alt="decrease-key (results/bench.csv)"><br><sub>fibonacci-heap: runtime on n pushes, n random decrease-keys, then all pops. Largest size n = 1,048,576: fibonacci-heap needs 5,104 ms against 3,844 ms for std::priority_queue (lazy).</sub></td></tr>
</table>
<!-- /FIGURES -->

### Findings

* The Fibonacci heap **loses** on both workloads in wall-clock time on this machine: roughly 15x slower than `std::priority_queue` for push/pop at $n=2^{20}$, and about 1.3x slower even when `decrease_key` dominates.
* The asymptotic win (amortized $O(1)$ `decrease_key`) only pays off when the number of decrease-key operations greatly exceeds pops *and* the lazy-deletion binary heap's duplicate entries become expensive (dense Dijkstra/Prim). The constant factors of pointer-linked trees and cache misses dominate otherwise.
* Use it when you need true `decrease_key`/`meld` semantics (stable handles, no duplicates), not as a drop-in speedup.

Raw data: [`results/bench.csv`](results/bench.csv). Benchmark source: [`bench/fibonacci_heap_bench.cpp`](bench/fibonacci_heap_bench.cpp).

## 6. Correctness & Verification

The Catch2 suite (16 cases) covers the CLRS worked example, dead handles and illegal `decrease_key`, duplicates and extreme keys, a random operation mix against a multiset oracle (300 seeds), handle stability, `meld`, `erase` of roots/minimum/interior/marked nodes, and Dijkstra with `decrease_key` against Dijkstra on `std::priority_queue`. Structural invariants are checked after every operation, and the amortized costs are checked against the potential function (insert $\le2$, decrease-key $\le5$, extract-min $\le3(\lfloor\log_\varphi n\rfloor+1)+3$) with a Fibonacci-size adversary and a $10^6$-operation scale test.

```bash
cmake --preset debug-asan && cmake --build --preset debug-asan --target fibonacci_heap_test
./build/debug-asan/mod_fibonacci-heap/fibonacci_heap_test
```

## 7. Limitations & Trade-Offs

* Slower than a binary heap on workloads without heavy `decrease_key` use (measured above).
* Handles are indices, so a popped handle may be recycled; check `contains(handle)`.
* No iterator or `find`: only the minimum is reachable.

## 8. References

* M. L. Fredman and R. E. Tarjan, “Fibonacci heaps and their uses in improved network optimization algorithms,” *J. ACM* 34(3), 1987.
* T. H. Cormen, C. E. Leiserson, R. L. Rivest, C. Stein, *Introduction to Algorithms*, 3rd ed., MIT Press, 2009. Chapter 19.
