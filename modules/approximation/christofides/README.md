# Christofides metric TSP tour

## Problem Statement

Given a finite, complete, symmetric metric distance matrix, return a Hamiltonian cycle whose cost is at most
three halves of the optimum when the metric assumptions hold.

## Algorithm Overview & Mathematical Model

The implementation builds a minimum spanning tree, computes a minimum weight perfect matching on its odd-degree
vertices, finds an Euler tour of the resulting multigraph, and shortcuts repeated vertices. Matching is solved
exactly by subset dynamic programming for up to 22 odd vertices and greedily for larger instances.

## Complexity Analysis

| phase | complexity |
| --- | --- |
| spanning tree | $O(n^2)$ |
| matching (at most 22 odd vertices) | $O(2^k k^2)$ |
| Euler tour and shortcut | $O(n^2)$ |

## Quickstart & Usage

```cpp
#include "christofides.hpp"
auto result = algo::christofides(distance_matrix);
```

## Empirical Benchmarks

<!-- BENCH:results/bench.csv workload=euclidean-circle -->
| n | christofides |
| ---: | ---: |
| 32 | 0.006 |
| 64 | 0.021 |
| 128 | 0.069 |
| 256 | 0.281 |

_Median wall time in milliseconds._
<!-- /BENCH -->

## Correctness & Verification

The Catch2 suite checks matrix validation, Hamiltonian-cycle invariants, exact small metrics, deterministic output,
and the three-halves bound on small Euclidean instances.

```bash
cmake --build --preset debug-asan --target christofides_test
./build/debug-asan/mod_christofides/christofides_test
```

## Limitations & Trade-Offs

The exact matching dynamic program is exponential in the number of odd vertices. Large instances use a greedy
matching, which preserves valid tours but does not claim the formal three-halves bound.

## References

N. Christofides, “Worst-case analysis of a new heuristic for the travelling salesman problem,” 1976.
