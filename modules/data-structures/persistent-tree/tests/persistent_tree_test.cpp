// Hard tests for algo::PersistentMap: a fully persistent ordered map (path-copying treap with
// key-hash priorities). Every update returns a NEW version id; any version can be updated again.
//   [S1] agreement with per-version std::map snapshots, immutability, edge cases
//   [S2] invariants, path-copying cost, structural sharing, adversarial inputs, scale
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "persistent_tree.hpp"
#include "test_support.hpp"

using Map = algo::PersistentMap<std::int64_t, std::int64_t>;
using Version = Map::Version;
using Snapshot = std::map<std::int64_t, std::int64_t>;
using algo::Rng;

namespace {
std::vector<std::pair<std::int64_t, std::int64_t>> flat(const Snapshot& s) { return {s.begin(), s.end()}; }

void expect_equal(const Map& m, Version v, const Snapshot& s, Rng& r) {
  REQUIRE(m.size(v) == s.size());
  REQUIRE(m.to_vector(v) == flat(s));
  for (int i = 0; i < 6; ++i) {
    const std::int64_t k = r.range(-30, 30);
    auto it = s.find(k);
    REQUIRE(m.contains(v, k) == (it != s.end()));
    REQUIRE(m.find(v, k) == (it == s.end() ? std::nullopt : std::optional(it->second)));
    REQUIRE(m.rank(v, k) == static_cast<std::size_t>(std::distance(s.begin(), s.lower_bound(k))));
  }
  if (!s.empty()) {
    const auto idx = r.below(s.size());
    auto it = s.begin();
    std::advance(it, static_cast<long>(idx));
    REQUIRE(m.kth(v, idx) == std::optional(std::make_pair(it->first, it->second)));
  }
  REQUIRE(m.kth(v, s.size()) == std::nullopt);
}
}  // namespace

// ------------------------------------------------------------------ S1

TEST_CASE("S1.1 old versions are unchanged by later updates (worked example)", "[S1][given]") {
  Map m;
  const Version v0 = m.empty_version();
  const Version v1 = m.insert(v0, 5, 50);
  const Version v2 = m.insert(v1, 3, 30);
  const Version v3 = m.insert(v2, 8, 80);
  const Version v4 = m.erase(v3, 5);
  const Version v5 = m.insert(v1, 4, 40);  // branch from an OLD version
  REQUIRE(m.to_vector(v0).empty());
  REQUIRE(m.to_vector(v1) == std::vector<std::pair<std::int64_t, std::int64_t>>{{5, 50}});
  REQUIRE(m.to_vector(v3) == std::vector<std::pair<std::int64_t, std::int64_t>>{{3, 30}, {5, 50}, {8, 80}});
  REQUIRE(m.to_vector(v4) == std::vector<std::pair<std::int64_t, std::int64_t>>{{3, 30}, {8, 80}});
  REQUIRE(m.to_vector(v5) == std::vector<std::pair<std::int64_t, std::int64_t>>{{4, 40}, {5, 50}});
  REQUIRE(m.versions() == 6);
}

TEST_CASE("S1.2 edge cases: empty version, absent erase, overwrite, bad ids, out-of-range kth", "[S1][boundary]") {
  Map m;
  const Version e = m.empty_version();
  REQUIRE(m.size(e) == 0);
  REQUIRE_FALSE(m.find(e, 1).has_value());
  REQUIRE(m.kth(e, 0) == std::nullopt);
  REQUIRE(m.rank(e, 7) == 0);
  REQUIRE(m.depth(e) == 0);
  const Version same = m.erase(e, 1);  // erasing an absent key still yields a (new, equal) version
  REQUIRE(same != e);
  REQUIRE(m.size(same) == 0);
  const Version a = m.insert(e, 1, 10);
  const Version b = m.insert(a, 1, 99);  // overwrite only affects the new version
  REQUIRE(m.find(a, 1) == 10);
  REQUIRE(m.find(b, 1) == 99);
  REQUIRE(m.size(b) == 1);
  const Version c = m.erase(a, 2);
  REQUIRE(m.to_vector(c) == m.to_vector(a));
  REQUIRE_THROWS_AS(m.size(9999), std::out_of_range);
  REQUIRE_THROWS_AS(m.insert(9999, 1, 1), std::out_of_range);
  REQUIRE_THROWS_AS(m.erase(9999, 1), std::out_of_range);
  REQUIRE_THROWS_AS(m.find(9999, 1), std::out_of_range);
  REQUIRE(m.versions() == 5);  // failed calls created no versions
  const Version z = m.erase(a, 1);
  REQUIRE(m.size(z) == 0);
  m.check_invariants(z);
}

TEST_CASE("S1.3 random version DAG equals per-version snapshots (100 seeds)", "[S1][stress]") {
  for (std::uint64_t trial = 0; trial < 100; ++trial) {
    ALGO_TRIAL(trial, 301);
    Rng r(seed_);
    Map m;
    std::vector<Snapshot> snaps{Snapshot{}};
    for (int i = 0; i < 150; ++i) {
      const Version base = static_cast<Version>(r.below(snaps.size()));  // any existing version, not just the newest
      Snapshot next = snaps[base];
      Version v;
      const std::int64_t k = r.range(-30, 30);
      if (r.coin(0.6)) { const std::int64_t val = r.range(0, 1000); v = m.insert(base, k, val); next[k] = val; }
      else { v = m.erase(base, k); next.erase(k); }
      REQUIRE(v == snaps.size());  // version ids are dense and sequential
      snaps.push_back(std::move(next));
      expect_equal(m, v, snaps.back(), r);
    }
    for (Version v = 0; v < snaps.size(); ++v) expect_equal(m, v, snaps[v], r);  // all history still intact
  }
}

TEST_CASE("S1.4 immutability under heavy branching: 20000 updates, then re-verify every version", "[S1][adversarial]") {
  Rng r(4);
  Map m;
  std::vector<Snapshot> snaps{Snapshot{}};
  for (int i = 0; i < 20000; ++i) {
    const Version base = r.coin(0.5) ? static_cast<Version>(snaps.size() - 1) : static_cast<Version>(r.below(snaps.size()));
    Snapshot next = snaps[base];
    const std::int64_t k = r.range(0, 60);
    Version v;
    if (r.coin(0.65)) { v = m.insert(base, k, i); next[k] = i; } else { v = m.erase(base, k); next.erase(k); }
    snaps.push_back(std::move(next));
    REQUIRE(v + 1 == snaps.size());
  }
  for (Version v = 0; v < snaps.size(); ++v) {
    REQUIRE(m.size(v) == snaps[v].size());
    if (v % 7 == 0) REQUIRE(m.to_vector(v) == flat(snaps[v]));
  }
}

TEST_CASE("S1.5 history independence: the tree depends only on the key set, not on the update history", "[S1][structural]") {
  Rng r(5);
  for (int rep = 0; rep < 40; ++rep) {
    std::vector<std::int64_t> keys;
    for (int i = 0; i < 200; ++i) keys.push_back(i * 3 + 1);
    Map m;
    Version a = m.empty_version(), b = m.empty_version();
    for (auto k : keys) a = m.insert(a, k, 0);                       // sorted order
    auto shuffled = keys;
    r.shuffle(shuffled.begin(), shuffled.end());
    for (auto k : shuffled) {
      b = m.insert(b, k, 0);
      if (r.coin(0.3)) { b = m.insert(b, 100000 + k, 0); b = m.erase(b, 100000 + k); }  // detours leave no trace
    }
    REQUIRE(m.to_vector(a) == m.to_vector(b));
    REQUIRE(m.depth(a) == m.depth(b));
    REQUIRE(m.shape_hash(a) == m.shape_hash(b));
  }
}

TEST_CASE("S1.6 rank and kth are inverse; rank counts keys strictly smaller", "[S1][structural]") {
  Rng r(6);
  Map m;
  Version v = m.empty_version();
  Snapshot ref;
  for (int i = 0; i < 500; ++i) { const auto k = r.range(0, 2000); v = m.insert(v, k, k * 2); ref[k] = k * 2; }
  std::size_t idx = 0;
  for (const auto& [k, val] : ref) {
    REQUIRE(m.rank(v, k) == idx);
    REQUIRE(m.kth(v, idx) == std::optional(std::make_pair(k, val)));
    REQUIRE(m.rank(v, k + 1) == idx + 1);   // keys are distinct integers
    ++idx;
  }
  REQUIRE(m.rank(v, -5) == 0);
  REQUIRE(m.rank(v, 100000) == ref.size());
}

TEST_CASE("S1.7 non-trivial key/value types and a custom comparator", "[S1][structural]") {
  algo::PersistentMap<std::string, std::vector<int>, std::greater<std::string>> m;
  auto v = m.empty_version();
  v = m.insert(v, "pear", {1, 2});
  const auto v1 = v;
  v = m.insert(v, "apple", {3});
  v = m.insert(v, "zebra", {});
  REQUIRE(m.to_vector(v).front().first == "zebra");  // descending order
  REQUIRE(m.find(v, "pear") == std::optional<std::vector<int>>({1, 2}));
  REQUIRE(m.size(v1) == 1);
  REQUIRE(m.kth(v, 2)->first == "apple");
}

TEST_CASE("S1.8 linear history of 200000 updates ends sorted and complete", "[S1][scale]") {
  Rng r(8);
  Map m;
  Version v = m.empty_version();
  Snapshot ref;
  for (int i = 0; i < 200000; ++i) {
    const std::int64_t k = r.range(0, 50000);
    if (r.below(10) < 7) { v = m.insert(v, k, i); ref[k] = i; } else { v = m.erase(v, k); ref.erase(k); }
  }
  REQUIRE(m.to_vector(v) == flat(ref));
  m.check_invariants(v);
}

// ------------------------------------------------------------------ S2

TEST_CASE("S2.1 invariants (BST order, heap order on priorities, subtree sizes) on random versions", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 30; ++trial) {
    ALGO_TRIAL(trial, 351);
    Rng r(seed_);
    Map m;
    std::vector<Version> vs{m.empty_version()};
    for (int i = 0; i < 200; ++i) {
      const Version base = vs[r.below(vs.size())];
      vs.push_back(r.coin(0.65) ? m.insert(base, r.range(0, 80), i) : m.erase(base, r.range(0, 80)));
      m.check_invariants(vs.back());
    }
  }
}

TEST_CASE("S2.2 path copying: nodes created by one update <= 2*(depth+1)+2, on average O(lg n)", "[S2][invariant]") {
  Rng r(2);
  Map m;
  Version v = m.empty_version();
  for (int i = 0; i < 50000; ++i) v = m.insert(v, r.range(0, 1 << 30), i);
  double total = 0;
  const int updates = 20000;
  for (int i = 0; i < updates; ++i) {
    v = r.coin(0.5) ? m.insert(v, r.range(0, 1 << 30), i) : m.erase(v, r.range(0, 1 << 30));
    const auto created = m.nodes_created_last_update();
    REQUIRE(created <= 2 * (m.depth(v) + 1) + 2);
    total += static_cast<double>(created);
  }
  const double n = static_cast<double>(m.size(v));
  REQUIRE(total / updates <= 6.0 * std::log2(n) + 6.0);
}

TEST_CASE("S2.3 depth stays O(lg n) even for sorted, reverse and bit-reversal key orders", "[S2][adversarial]") {
  const int n = 1 << 16;
  for (int order = 0; order < 3; ++order) {
    Map m;
    Version v = m.empty_version();
    for (int i = 0; i < n; ++i) {
      std::int64_t k = i;
      if (order == 1) k = n - i;
      if (order == 2) { std::uint32_t x = static_cast<std::uint32_t>(i), y = 0; for (int b = 0; b < 16; ++b) { y = (y << 1) | (x & 1); x >>= 1; } k = y; }
      v = m.insert(v, k, 0);
    }
    INFO("order=" << order << " depth=" << m.depth(v));
    REQUIRE(m.depth(v) <= static_cast<std::size_t>(6 * 16 + 10));  // a plain BST would have depth 65536
  }
}

TEST_CASE("S2.4 structural sharing: total nodes grow O(lg n) per version, not O(n)", "[S2][invariant]") {
  Rng r(4);
  Map m;
  Version v = m.empty_version();
  for (int i = 0; i < 10000; ++i) v = m.insert(v, i, i);
  const std::size_t before = m.node_count();
  const int updates = 10000;
  for (int i = 0; i < updates; ++i) v = m.insert(v, r.range(0, 9999), i + 100000);  // each keeps a full 10^4-key version alive
  const std::size_t grown = m.node_count() - before;
  INFO("nodes added for 10^4 versions of a 10^4-key map: " << grown);
  REQUIRE(grown <= static_cast<std::size_t>(updates) * 80);        // O(lg n) each
  REQUIRE(grown * 20 < static_cast<std::size_t>(updates) * 10000);  // vs 10^8 for naive copying
}

TEST_CASE("S2.5 full persistence: a fan of 10000 versions updated from version 0 and from one common parent", "[S2][adversarial]") {
  Map m;
  const Version e = m.empty_version();
  Version base = e;
  for (int i = 0; i < 100; ++i) base = m.insert(base, i, i);
  std::vector<Version> fan;
  for (int i = 0; i < 10000; ++i) fan.push_back(m.insert(base, 1000 + i, i));  // siblings, all branching from `base`
  for (int i = 0; i < 10000; i += 97) {
    REQUIRE(m.size(fan[static_cast<std::size_t>(i)]) == 101);
    REQUIRE(m.find(fan[static_cast<std::size_t>(i)], 1000 + i) == i);
    REQUIRE_FALSE(m.contains(fan[static_cast<std::size_t>(i)], 1000 + i + 1));  // siblings do not see each other
  }
  REQUIRE(m.size(base) == 100);
  const Version v0_branch = m.insert(e, 7, 7);  // updating the very first version still works
  REQUIRE(m.size(v0_branch) == 1);
  REQUIRE(m.size(e) == 0);
}

TEST_CASE("S2.6 drain to empty repeatedly; versions at size 0 and 1 behave", "[S2][structural]") {
  Rng r(6);
  Map m;
  Version v = m.empty_version();
  for (int round = 0; round < 20; ++round) {
    std::vector<std::int64_t> keys;
    for (int i = 0; i < 100; ++i) { keys.push_back(i); v = m.insert(v, i, round); }
    r.shuffle(keys.begin(), keys.end());
    for (auto k : keys) { v = m.erase(v, k); m.check_invariants(v); }
    REQUIRE(m.size(v) == 0);
    REQUIRE(m.depth(v) == 0);
    REQUIRE(m.to_vector(v).empty());
  }
}

TEST_CASE("S2.7 query cost: find/rank/kth on a 10^6-key version agree with a sorted vector", "[S2][scale]") {
  Rng r(7);
  Map m;
  Version v = m.empty_version();
  std::vector<std::int64_t> keys;
  for (std::int64_t i = 0; i < 1'000'000; ++i) keys.push_back(i * 2);
  r.shuffle(keys.begin(), keys.end());
  for (auto k : keys) v = m.insert(v, k, k);
  std::sort(keys.begin(), keys.end());
  REQUIRE(m.size(v) == keys.size());
  for (int i = 0; i < 100000; ++i) {
    const auto idx = r.below(keys.size());
    REQUIRE(m.kth(v, idx) == std::optional(std::make_pair(keys[idx], keys[idx])));
    const auto probe = r.range(-5, 2'000'005);
    REQUIRE(m.rank(v, probe) == static_cast<std::size_t>(std::lower_bound(keys.begin(), keys.end(), probe) - keys.begin()));
    REQUIRE(m.contains(v, probe) == (probe >= 0 && probe % 2 == 0 && probe < 2'000'000));
  }
  m.check_invariants(v);
}

TEST_CASE("S2.8 version bookkeeping: ids dense, counts consistent, node_count never shrinks", "[S2][invariant]") {
  Rng r(8);
  Map m;
  REQUIRE(m.versions() == 1);
  std::size_t prev_nodes = m.node_count();
  Version last = m.empty_version();
  for (int i = 0; i < 2000; ++i) {
    const Version base = static_cast<Version>(r.below(m.versions()));
    last = r.coin() ? m.insert(base, r.range(0, 50), i) : m.erase(base, r.range(0, 50));
    REQUIRE(last + 1 == m.versions());
    REQUIRE(m.node_count() >= prev_nodes);
    REQUIRE(m.node_count() - prev_nodes == m.nodes_created_last_update());
    prev_nodes = m.node_count();
  }
}
