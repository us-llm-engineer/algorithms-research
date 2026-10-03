// Shared workloads for ordered-set style structures (treap, skip list, splay tree and the std baselines).
// A "set" only needs insert(k), erase(k) and contains(k); `make` builds a fresh empty one.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "rng.hpp"

namespace algo::bench {

struct OrderedWorkloads {
  std::vector<std::uint64_t> random_keys, sorted_keys, queries_uniform, queries_skewed;
};

inline OrderedWorkloads make_ordered_workloads(std::size_t n) {
  Rng rng(n * 7919u + 1);
  OrderedWorkloads w;
  w.random_keys.resize(n);
  for (std::size_t i = 0; i < n; ++i) w.random_keys[i] = rng.below(~0ULL >> 1);
  w.sorted_keys.resize(n);
  for (std::size_t i = 0; i < n; ++i) w.sorted_keys[i] = i * 2;
  w.queries_uniform.resize(n);
  w.queries_skewed.resize(n);
  const std::size_t hot = std::max<std::size_t>(1, n / 100);
  for (std::size_t i = 0; i < n; ++i) {
    w.queries_uniform[i] = w.random_keys[rng.below(n)];
    // 90% of queries hit the hottest 1% of keys (working-set behaviour).
    w.queries_skewed[i] = w.random_keys[rng.coin(0.9) ? rng.below(hot) : rng.below(n)];
  }
  return w;
}

// Emits four workloads: insert-random, insert-sorted, lookup-uniform, lookup-skewed.
template <class MakeSet>
void run_ordered(Csv& csv, const std::string& name, std::size_t n, const OrderedWorkloads& w, MakeSet make,
                 int reps) {
  csv.row(name, "insert-random", n, measure({}, [&] {
            auto s = make();
            for (auto k : w.random_keys) s.insert(k);
            do_not_optimize(s);
          }, reps));
  csv.row(name, "insert-sorted", n, measure({}, [&] {
            auto s = make();
            for (auto k : w.sorted_keys) s.insert(k);
            do_not_optimize(s);
          }, reps));
  auto base = make();
  for (auto k : w.random_keys) base.insert(k);
  csv.row(name, "lookup-uniform", n, measure({}, [&] {
            std::size_t hits = 0;
            for (auto k : w.queries_uniform) hits += base.contains(k);
            do_not_optimize(hits);
          }, reps));
  csv.row(name, "lookup-skewed", n, measure({}, [&] {
            std::size_t hits = 0;
            for (auto k : w.queries_skewed) hits += base.contains(k);
            do_not_optimize(hits);
          }, reps));
  csv.row(name, "erase-random", n, measure({}, [&] {
            auto s = make();
            for (auto k : w.random_keys) s.insert(k);
            for (auto k : w.random_keys) s.erase(k);
            do_not_optimize(s);
          }, reps));
}

// std::set / std::unordered_set adapters with the same three-method interface.
template <class Impl>
struct StdSetAdapter {
  Impl impl;
  void insert(std::uint64_t k) { impl.insert(k); }
  void erase(std::uint64_t k) { impl.erase(k); }
  bool contains(std::uint64_t k) const { return impl.find(k) != impl.end(); }
};

}  // namespace algo::bench

#include <algorithm>

namespace algo::bench {
// Brute-force strategy: a sorted array with binary-search lookup and O(n) shifting insert/erase.
struct SortedVectorSet {
  std::vector<std::uint64_t> v;
  void insert(std::uint64_t k) {
    auto it = std::lower_bound(v.begin(), v.end(), k);
    if (it == v.end() || *it != k) v.insert(it, k);
  }
  void erase(std::uint64_t k) {
    auto it = std::lower_bound(v.begin(), v.end(), k);
    if (it != v.end() && *it == k) v.erase(it);
  }
  bool contains(std::uint64_t k) const { return std::binary_search(v.begin(), v.end(), k); }
};
}  // namespace algo::bench
