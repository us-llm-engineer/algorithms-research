// Hard tests for algo::Treap: a randomized balanced BST (set semantics, order statistics).
// The expected-case claims are tested statistically with analytically derived tolerances:
// the depth of key k is a sum of independent Bernoulli(1/(|j-k|+1)) variables, so Var <= E.
//   [S1] correctness against std::set and order-statistic oracles
//   [S2] invariants, expected depth / height / search cost, adversarial inputs, scale
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "test_support.hpp"
#include "treap.hpp"

using Treap = algo::Treap<std::int64_t>;
using algo::Rng;

namespace {
double harmonic(std::size_t n) { double h = 0; for (std::size_t i = 1; i <= n; ++i) h += 1.0 / static_cast<double>(i); return h; }
// E[depth of the key with 1-based rank k] in a random treap of n keys (root has depth 0).
double expected_depth(std::size_t k, std::size_t n) { return harmonic(k) + harmonic(n - k + 1) - 2.0; }
std::vector<std::int64_t> keys_of(const std::set<std::int64_t>& s) { return {s.begin(), s.end()}; }
}  // namespace

// ------------------------------------------------------------------ S1

TEST_CASE("S1.1 small worked example with order statistics", "[S1][given]") {
  Treap t(1);
  for (std::int64_t k : {50, 20, 70, 10, 30, 60, 80}) REQUIRE(t.insert(k));
  REQUIRE(t.to_vector() == std::vector<std::int64_t>{10, 20, 30, 50, 60, 70, 80});
  REQUIRE(t.kth(0) == 10);
  REQUIRE(t.kth(3) == 50);
  REQUIRE(t.kth(6) == 80);
  REQUIRE(t.rank(50) == 3);
  REQUIRE(t.rank(55) == 4);
  REQUIRE(t.lower_bound(55) == 60);
  REQUIRE(t.erase(50));
  REQUIRE(t.size() == 6);
  REQUIRE_FALSE(t.contains(50));
}

TEST_CASE("S1.2 empty, singleton, duplicates, absent erase, extreme keys, out-of-range order statistics", "[S1][boundary]") {
  Treap t(2);
  REQUIRE(t.empty());
  REQUIRE(t.height() == 0);
  REQUIRE_FALSE(t.contains(0));
  REQUIRE_FALSE(t.erase(0));
  REQUIRE(t.kth(0) == std::nullopt);
  REQUIRE(t.rank(5) == 0);
  REQUIRE(t.lower_bound(0) == std::nullopt);
  REQUIRE(t.insert(7));
  REQUIRE_FALSE(t.insert(7));
  REQUIRE(t.size() == 1);
  REQUIRE(t.height() == 1);
  REQUIRE(t.depth_of(7) == 0);
  REQUIRE(t.depth_of(8) == std::nullopt);
  REQUIRE(t.erase(7));
  REQUIRE(t.empty());
  for (auto k : {INT64_MAX, INT64_MIN, std::int64_t{0}}) REQUIRE(t.insert(k));
  REQUIRE(t.kth(0) == INT64_MIN);
  REQUIRE(t.kth(2) == INT64_MAX);
  REQUIRE(t.kth(3) == std::nullopt);
  REQUIRE(t.rank(INT64_MAX) == 2);
  t.check_invariants();
}

TEST_CASE("S1.3 random operations equal std::set, rank and kth equal the sorted vector (300 seeds)", "[S1][stress]") {
  for (std::uint64_t trial = 0; trial < 300; ++trial) {
    ALGO_TRIAL(trial, 401);
    Rng r(seed_);
    Treap t(seed_);
    std::set<std::int64_t> ref;
    for (int i = 0; i < 250; ++i) {
      const std::int64_t k = r.range(-40, 40);
      switch (r.below(6)) {
        case 0: case 1: REQUIRE(t.insert(k) == ref.insert(k).second); break;
        case 2: REQUIRE(t.erase(k) == (ref.erase(k) == 1)); break;
        case 3: REQUIRE(t.contains(k) == (ref.count(k) == 1)); break;
        case 4: { auto it = ref.lower_bound(k); REQUIRE(t.lower_bound(k) == (it == ref.end() ? std::nullopt : std::optional(*it))); break; }
        default: {
          REQUIRE(t.rank(k) == static_cast<std::size_t>(std::distance(ref.begin(), ref.lower_bound(k))));
          if (!ref.empty()) { auto idx = r.below(ref.size()); auto it = ref.begin(); std::advance(it, static_cast<long>(idx)); REQUIRE(t.kth(idx) == *it); }
        }
      }
      REQUIRE(t.size() == ref.size());
    }
    REQUIRE(t.to_vector() == keys_of(ref));
    t.check_invariants();
  }
}

TEST_CASE("S1.4 sorted, reverse and organ-pipe insertion keep the set exact", "[S1][structural]") {
  const int n = 4000;
  for (int order = 0; order < 3; ++order) {
    Treap t(order + 1);
    std::set<std::int64_t> ref;
    for (int i = 0; i < n; ++i) {
      const std::int64_t k = order == 0 ? i : order == 1 ? n - i : (i % 2 ? n - i / 2 : i / 2);
      REQUIRE(t.insert(k) == ref.insert(k).second);
    }
    REQUIRE(t.to_vector() == keys_of(ref));
    t.check_invariants();
  }
}

TEST_CASE("S1.5 erase every key in random / ascending / descending order; empty again, reusable", "[S1][structural]") {
  Rng r(5);
  for (int order = 0; order < 3; ++order) {
    Treap t(11);
    std::vector<std::int64_t> keys;
    for (int i = 0; i < 1500; ++i) keys.push_back(i);
    for (auto k : keys) t.insert(k);
    if (order == 0) r.shuffle(keys.begin(), keys.end());
    if (order == 2) std::reverse(keys.begin(), keys.end());
    std::size_t left = keys.size();
    for (auto k : keys) {
      REQUIRE(t.erase(k));
      REQUIRE(t.size() == --left);
      if (left % 100 == 0) t.check_invariants();
    }
    REQUIRE(t.empty());
    REQUIRE(t.insert(1));
    REQUIRE(t.size() == 1);
  }
}

TEST_CASE("S1.6 same seed -> identical tree shape; different seeds -> (almost surely) different shapes", "[S1][structural]") {
  auto build = [](std::uint64_t seed) {
    Treap t(seed);
    for (int i = 0; i < 2000; ++i) t.insert(i);
    return t;
  };
  Treap a = build(7), b = build(7), c = build(8);
  REQUIRE(a.height() == b.height());
  bool any_depth_differs_ab = false, any_depth_differs_ac = false;
  for (int k = 0; k < 2000; ++k) {
    any_depth_differs_ab |= a.depth_of(k) != b.depth_of(k);
    any_depth_differs_ac |= a.depth_of(k) != c.depth_of(k);
  }
  REQUIRE_FALSE(any_depth_differs_ab);
  REQUIRE(any_depth_differs_ac);
}

TEST_CASE("S1.7 custom comparator and string keys", "[S1][structural]") {
  algo::Treap<std::string, std::greater<std::string>> t(3);
  for (const char* w : {"kiwi", "apple", "pear", "fig", "apple"}) t.insert(w);
  REQUIRE(t.to_vector() == std::vector<std::string>{"pear", "kiwi", "fig", "apple"});
  REQUIRE(t.kth(0) == std::optional<std::string>("pear"));
  REQUIRE(t.rank("fig") == 2);
}

TEST_CASE("S1.8 slot reuse: node capacity does not grow when keys are erased and re-inserted", "[S1][scale]") {
  Treap t(8);
  Rng r(8);
  std::vector<std::int64_t> keys;
  keys.reserve(20000);
  for (int i = 0; i < 20000; ++i) { t.insert(i); keys.push_back(i); }
  const std::size_t cap = t.node_capacity();
  REQUIRE(cap >= 20000);
  for (int round = 0; round < 5; ++round) {
    for (const auto key : keys) REQUIRE(t.erase(key));
    REQUIRE(t.empty());
    keys.clear();
    for (int i = 0; i < 20000; ++i) {
      const auto key = static_cast<std::int64_t>(r.below(1 << 30)) * 20000 + i;
      REQUIRE(t.insert(key));
      keys.push_back(key);
    }
    REQUIRE(t.node_capacity() == cap);
  }
}

// ------------------------------------------------------------------ S2

TEST_CASE("S2.1 invariants (BST order, max-heap on priorities, subtree sizes) after every operation", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 40; ++trial) {
    ALGO_TRIAL(trial, 451);
    Rng r(seed_);
    Treap t(seed_);
    for (int i = 0; i < 300; ++i) {
      const auto k = r.range(-50, 50);
      if (r.coin(0.6)) t.insert(k); else t.erase(k);
      t.check_invariants();
    }
  }
}

TEST_CASE("S2.2 expected depth of the key of rank k equals H_k + H_{n-k+1} - 2 (mean over 400 seeds, 6 sigma)", "[S2][invariant]") {
  const std::size_t n = 1024;
  const int trials = 400;
  for (std::size_t k : {std::size_t{1}, std::size_t{2}, n / 4, n / 2, n - 1, n}) {
    double sum = 0;
    for (int s = 0; s < trials; ++s) {
      Treap t(1000 + static_cast<std::uint64_t>(s));
      for (std::size_t i = 1; i <= n; ++i) t.insert(static_cast<std::int64_t>(i));  // sorted: worst case for a plain BST
      sum += static_cast<double>(*t.depth_of(static_cast<std::int64_t>(k)));
    }
    const double mean = sum / trials, expect = expected_depth(k, n);
    const double sigma = std::sqrt(std::max(expect, 1.0) / trials);  // Var(depth) <= E(depth) (independent Bernoulli sum)
    INFO("rank " << k << ": mean depth " << mean << ", expected " << expect << ", sigma " << sigma);
    REQUIRE(std::abs(mean - expect) <= 6 * sigma + 0.02);
  }
}

TEST_CASE("S2.3 height <= 6 lg n + 10 for 200 seeds under sorted insertion (tail bound of the O(lg n) height)", "[S2][adversarial]") {
  const std::size_t n = 1 << 14;
  std::size_t worst = 0;
  for (int s = 0; s < 200; ++s) {
    Treap t(5000 + static_cast<std::uint64_t>(s));
    for (std::size_t i = 0; i < n; ++i) t.insert(static_cast<std::int64_t>(i));
    worst = std::max(worst, t.height());
  }
  INFO("worst height over 200 seeds: " << worst);
  REQUIRE(worst <= static_cast<std::size_t>(6 * 14 + 10));
}

TEST_CASE("S2.4 mean search cost (comparisons) matches 2(1+1/n)H_n - 3 within 3%", "[S2][invariant]") {
  const std::size_t n = 1 << 15;
  double total_depth = 0;
  const int trials = 12;
  for (int s = 0; s < trials; ++s) {
    Treap t(9000 + static_cast<std::uint64_t>(s));
    Rng r(static_cast<std::uint64_t>(s));
    auto perm = r.permutation(static_cast<int>(n));
    for (int k : perm) t.insert(k);
    for (int k = 0; k < static_cast<int>(n); k += 7) total_depth += static_cast<double>(*t.depth_of(k)) + 1;  // comparisons = depth+1 (successful search)
  }
  const double count = trials * (static_cast<double>((n + 6) / 7));
  const double mean_cmp = total_depth / count;
  const double expect = 2.0 * (1.0 + 1.0 / static_cast<double>(n)) * harmonic(n) - 3.0;  // average successful-search comparisons
  INFO("mean comparisons " << mean_cmp << " expected " << expect);
  REQUIRE(std::abs(mean_cmp - expect) <= 0.03 * expect);
}

TEST_CASE("S2.5 deleting 90% of the keys keeps the treap balanced (no degeneration from deletions)", "[S2][adversarial]") {
  Rng r(5);
  const int n = 1 << 15;
  Treap t(55);
  for (int i = 0; i < n; ++i) t.insert(i);
  std::vector<int> order = r.permutation(n);
  for (int i = 0; i < n * 9 / 10; ++i) t.erase(order[static_cast<std::size_t>(i)]);
  REQUIRE(t.size() == static_cast<std::size_t>(n - n * 9 / 10));
  INFO("height after deletions " << t.height());
  REQUIRE(t.height() <= static_cast<std::size_t>(6 * std::log2(static_cast<double>(t.size())) + 20));
  t.check_invariants();
  // deleting in sorted order (adversarial for naive BST deletion) also stays logarithmic
  Treap u(56);
  for (int i = 0; i < n; ++i) u.insert(i);
  for (int i = 0; i < n * 3 / 4; ++i) u.erase(i);
  REQUIRE(u.height() <= static_cast<std::size_t>(6 * std::log2(static_cast<double>(u.size())) + 20));
}

TEST_CASE("S2.6 order-statistic queries are exact on a large treap and cost O(depth) comparisons", "[S2][structural]") {
  Rng r(6);
  Treap t(66);
  std::set<std::int64_t> ref;
  for (int i = 0; i < 100000; ++i) { const auto k = r.range(0, 1'000'000); t.insert(k); ref.insert(k); }
  const auto sorted = keys_of(ref);
  for (int q = 0; q < 50000; ++q) {
    const auto idx = r.below(sorted.size());
    REQUIRE(t.kth(idx) == sorted[idx]);
    const auto probe = r.range(-10, 1'000'010);
    REQUIRE(t.rank(probe) == static_cast<std::size_t>(std::lower_bound(sorted.begin(), sorted.end(), probe) - sorted.begin()));
  }
  t.reset_stats();
  t.kth(sorted.size() / 2);
  REQUIRE(t.stats().node_visits <= t.height());
}

TEST_CASE("S2.7 stats: a successful search visits depth+1 nodes; counters reset", "[S2][given]") {
  Treap t(77);
  for (int i = 0; i < 1000; ++i) t.insert(i);
  for (int k : {0, 500, 999}) {
    t.reset_stats();
    REQUIRE(t.contains(k));
    REQUIRE(t.stats().node_visits == *t.depth_of(k) + 1);
  }
  t.reset_stats();
  REQUIRE(t.stats().node_visits == 0);
}

TEST_CASE("S2.8 scale: 10^6 mixed operations against std::set", "[S2][scale]") {
  Rng r(8);
  Treap t(88);
  std::set<std::int64_t> ref;
  for (int i = 0; i < 1'000'000; ++i) {
    const std::int64_t k = r.range(0, 100000);
    const auto roll = r.below(10);
    if (roll < 5) { REQUIRE(t.insert(k) == ref.insert(k).second); }
    else if (roll < 8) { REQUIRE(t.erase(k) == (ref.erase(k) == 1)); }
    else { REQUIRE(t.contains(k) == (ref.count(k) == 1)); }
  }
  REQUIRE(t.to_vector() == keys_of(ref));
  t.check_invariants();
  REQUIRE(t.height() <= static_cast<std::size_t>(6 * std::log2(static_cast<double>(std::max<std::size_t>(ref.size(), 2))) + 20));
}
