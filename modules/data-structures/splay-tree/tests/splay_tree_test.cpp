// Hard tests for algo::SplayTree (bottom-up splaying, set semantics).
//   [S1] correctness against std::set and structural behaviour
//   [S2] the splay-tree theorems (balance, sequential access, static optimality, dynamic finger), checked
//        against rotation counts with explicit constants, plus adversarial shapes
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "splay_tree.hpp"
#include "test_support.hpp"

using Tree = algo::SplayTree<std::int64_t>;
using algo::Rng;

namespace {
template <class Set>
std::optional<std::int64_t> opt_lower(const Set& s, std::int64_t k) { auto it = s.lower_bound(k); return it == s.end() ? std::nullopt : std::optional(*it); }
template <class Set>
std::optional<std::int64_t> opt_upper(const Set& s, std::int64_t k) { auto it = s.upper_bound(k); return it == s.end() ? std::nullopt : std::optional(*it); }
template <class Set>
std::optional<std::int64_t> opt_pred(const Set& s, std::int64_t k) { auto it = s.lower_bound(k); return it == s.begin() ? std::nullopt : std::optional(*std::prev(it)); }

Tree path_tree(int n) {  // ascending inserts give a left-leaning path of height n
  Tree t;
  for (int i = 1; i <= n; ++i) t.insert(i);
  return t;
}
double lg(double x) { return std::log2(x); }
}  // namespace

// ------------------------------------------------------------------ S1

TEST_CASE("S1.1 small worked example: in-order contents and root after each access", "[S1][given]") {
  Tree t;
  for (std::int64_t k : {5, 3, 8, 1, 4, 7, 9, 2, 6}) REQUIRE(t.insert(k));
  REQUIRE(t.root_key() == 6);  // last inserted key is splayed to the root
  REQUIRE(t.to_vector() == std::vector<std::int64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9});
  REQUIRE(t.contains(3));
  REQUIRE(t.root_key() == 3);
  REQUIRE(t.erase(3));
  REQUIRE_FALSE(t.contains(3));
  REQUIRE(t.size() == 8);
  t.check_invariants();
}

TEST_CASE("S1.2 empty, one element, duplicate insert, absent erase, extreme keys", "[S1][boundary]") {
  Tree t;
  REQUIRE(t.empty());
  REQUIRE(t.size() == 0);
  REQUIRE(t.height() == 0);
  REQUIRE_FALSE(t.contains(1));
  REQUIRE_FALSE(t.erase(1));
  REQUIRE_FALSE(t.min().has_value());
  REQUIRE_FALSE(t.max().has_value());
  REQUIRE_FALSE(t.lower_bound(0).has_value());
  REQUIRE_FALSE(t.root_key().has_value());
  REQUIRE(t.to_vector().empty());
  REQUIRE(t.insert(5));
  REQUIRE_FALSE(t.insert(5));  // set semantics
  REQUIRE(t.size() == 1);
  REQUIRE(t.height() == 1);
  REQUIRE(t.min() == 5);
  REQUIRE(t.max() == 5);
  REQUIRE(t.erase(5));
  REQUIRE(t.empty());
  const std::int64_t lo = INT64_MIN, hi = INT64_MAX;
  for (auto k : {hi, lo, std::int64_t{0}, hi - 1, lo + 1}) REQUIRE(t.insert(k));
  REQUIRE(t.min() == lo);
  REQUIRE(t.max() == hi);
  REQUIRE(t.upper_bound(hi) == std::nullopt);
  REQUIRE(t.predecessor(lo) == std::nullopt);
  REQUIRE(t.lower_bound(lo + 2) == 0);
  t.check_invariants();
}

TEST_CASE("S1.3 sorted, reverse and zig-zag insertion orders produce the same set", "[S1][structural]") {
  const int n = 3000;
  std::vector<std::vector<std::int64_t>> orders(3);
  for (int i = 0; i < n; ++i) orders[0].push_back(i);
  for (int i = 0; i < n; ++i) orders[1].push_back(n - i);
  for (int i = 0; i < n / 2; ++i) { orders[2].push_back(i); orders[2].push_back(n - 1 - i); }
  for (auto& order : orders) {
    Tree t;
    std::set<std::int64_t> ref;
    for (auto k : order) { REQUIRE(t.insert(k) == ref.insert(k).second); }
    REQUIRE(t.to_vector() == std::vector<std::int64_t>(ref.begin(), ref.end()));
    t.check_invariants();
  }
}

TEST_CASE("S1.4 random operation mix equals std::set (300 seeds, small key range for collisions)", "[S1][stress]") {
  for (std::uint64_t trial = 0; trial < 300; ++trial) {
    ALGO_TRIAL(trial, 61);
    Rng r(seed_);
    Tree t;
    std::set<std::int64_t> ref;
    for (int i = 0; i < 300; ++i) {
      const std::int64_t k = r.range(-40, 40);
      switch (r.below(8)) {
        case 0: case 1: REQUIRE(t.insert(k) == ref.insert(k).second); break;
        case 2: REQUIRE(t.erase(k) == (ref.erase(k) == 1)); break;
        case 3: REQUIRE(t.contains(k) == (ref.count(k) == 1)); break;
        case 4: REQUIRE(t.lower_bound(k) == opt_lower(ref, k)); break;
        case 5: REQUIRE(t.upper_bound(k) == opt_upper(ref, k)); break;
        case 6: REQUIRE(t.predecessor(k) == opt_pred(ref, k)); break;
        default:
          REQUIRE(t.min() == (ref.empty() ? std::nullopt : std::optional(*ref.begin())));
          REQUIRE(t.max() == (ref.empty() ? std::nullopt : std::optional(*ref.rbegin())));
      }
      REQUIRE(t.size() == ref.size());
    }
    REQUIRE(t.to_vector() == std::vector<std::int64_t>(ref.begin(), ref.end()));
    t.check_invariants();
  }
}

TEST_CASE("S1.5 an access leaves the accessed key (or its neighbour on a miss) at the root", "[S1][structural]") {
  Rng r(5);
  Tree t;
  std::set<std::int64_t> ref;
  for (int i = 0; i < 500; ++i) { const auto k = r.range(0, 2000) * 2; t.insert(k); ref.insert(k); }  // even keys only
  for (int i = 0; i < 2000; ++i) {
    const std::int64_t k = r.range(0, 4000);
    const bool hit = t.contains(k);
    REQUIRE(hit == (ref.count(k) == 1));
    const auto root = t.root_key();
    REQUIRE(root.has_value());
    if (hit) REQUIRE(*root == k);
    else {  // the last node visited on a failed search is the predecessor or the successor
      const auto pred = opt_pred(ref, k), succ = opt_lower(ref, k);
      REQUIRE((root == pred || root == succ));
    }
  }
}

TEST_CASE("S1.6 erase in every position: root, min, max, only element, interior", "[S1][structural]") {
  for (std::uint64_t trial = 0; trial < 100; ++trial) {
    ALGO_TRIAL(trial, 66);
    Rng r(seed_);
    Tree t;
    std::set<std::int64_t> ref;
    const int n = 1 + static_cast<int>(r.below(120));
    for (int i = 0; i < n; ++i) { const auto k = r.range(0, 500); t.insert(k); ref.insert(k); }
    while (!ref.empty()) {
      std::int64_t victim;
      switch (r.below(4)) {
        case 0: victim = *ref.begin(); break;
        case 1: victim = *ref.rbegin(); break;
        case 2: victim = *t.root_key() == 0 ? *ref.begin() : *t.root_key(); break;
        default: { auto it = ref.begin(); std::advance(it, static_cast<long>(r.below(ref.size()))); victim = *it; }
      }
      REQUIRE(t.erase(victim));
      ref.erase(victim);
      REQUIRE_FALSE(t.erase(victim));
      REQUIRE(t.to_vector() == std::vector<std::int64_t>(ref.begin(), ref.end()));
      t.check_invariants();
    }
    REQUIRE(t.empty());
    REQUIRE(t.height() == 0);
  }
}

TEST_CASE("S1.7 custom comparators: descending order and string keys", "[S1][structural]") {
  algo::SplayTree<int, std::greater<int>> desc;
  std::set<int, std::greater<int>> ref;
  Rng r(7);
  for (int i = 0; i < 2000; ++i) { const int k = static_cast<int>(r.range(0, 300)); REQUIRE(desc.insert(k) == ref.insert(k).second); }
  REQUIRE(desc.to_vector() == std::vector<int>(ref.begin(), ref.end()));
  REQUIRE(desc.min() == *ref.begin());  // "min" is the first key in comparator order
  algo::SplayTree<std::string> words;
  for (const char* w : {"pear", "apple", "fig", "apple", "zebra", "kiwi"}) words.insert(w);
  REQUIRE(words.to_vector() == std::vector<std::string>{"apple", "fig", "kiwi", "pear", "zebra"});
  REQUIRE(words.lower_bound("g") == std::optional<std::string>("kiwi"));
}

TEST_CASE("S1.8 a 10^6-node path does not overflow the stack and tears down cleanly", "[S1][scale]") {
  Tree t = path_tree(1'000'000);  // height 10^6: any recursive routine would blow an 8 MiB stack
  REQUIRE(t.size() == 1'000'000);
  REQUIRE(t.height() == 1'000'000);
  t.check_invariants();
  const auto v = t.to_vector();
  REQUIRE(v.size() == 1'000'000);
  REQUIRE(std::is_sorted(v.begin(), v.end()));
  REQUIRE(t.contains(1));  // splay from the very bottom of the path
  REQUIRE(t.root_key() == 1);
  REQUIRE(t.height() < 600'000);
}

// ------------------------------------------------------------------ S2

TEST_CASE("S2.1 invariants (BST order, parent links, size) hold after every operation", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 40; ++trial) {
    ALGO_TRIAL(trial, 71);
    Rng r(seed_);
    Tree t;
    for (int i = 0; i < 400; ++i) {
      const auto k = r.range(-60, 60);
      switch (r.below(5)) {
        case 0: case 1: t.insert(k); break;
        case 2: t.erase(k); break;
        case 3: t.contains(k); break;
        default: t.lower_bound(k);
      }
      t.check_invariants();
    }
  }
}

TEST_CASE("S2.2 rotation accounting: root=0, child=1 (zig), grandchild=2 (zig-zig / zig-zag)", "[S2][given]") {
  Tree t;
  t.insert(1);
  t.insert(2);
  t.insert(3);  // shape: 3 -> left 2 -> left 1
  REQUIRE(t.root_key() == 3);
  t.reset_stats();
  REQUIRE(t.contains(3));
  REQUIRE(t.stats().rotations == 0);
  REQUIRE(t.stats().accesses == 1);
  REQUIRE(t.contains(2));
  REQUIRE(t.stats().rotations == 1);  // zig
  t.contains(3);                      // now 3 is a child of 2; make the path 3 -> 2 -> ... again
  Tree u;
  u.insert(1);
  u.insert(2);
  u.insert(3);
  u.reset_stats();
  REQUIRE(u.contains(1));  // depth 2 on a left-left path: zig-zig
  REQUIRE(u.stats().rotations == 2);
  Tree v;
  v.insert(1);
  v.insert(3);
  v.reset_stats();
  v.insert(2);  // BST-inserted below 1 on the 3 -> 1 -> 2 path: zig-zag
  REQUIRE(v.stats().rotations == 2);
  REQUIRE(v.root_key() == 2);
}

TEST_CASE("S2.3 splaying halves the depth of the access path (rejects move-to-root)", "[S2][adversarial]") {
  const int n = 1 << 12;
  Tree t = path_tree(n);
  REQUIRE(t.height() == static_cast<std::size_t>(n));
  REQUIRE(t.contains(1));  // deepest node
  // After splaying a node at depth d, every node that was on the path has depth roughly d/2 (Sleator-Tarjan).
  REQUIRE(t.height() <= static_cast<std::size_t>(n) / 2 + 2);
  REQUIRE(t.contains(2));
  REQUIRE(t.height() <= static_cast<std::size_t>(n) / 4 + 4);
  t.check_invariants();
}

TEST_CASE("S2.4 sequential access theorem: n in-order accesses cost O(n) rotations", "[S2][invariant]") {
  for (int n : {1 << 8, 1 << 12, 1 << 15}) {
    Tree t = path_tree(n);
    t.reset_stats();
    for (int i = 1; i <= n; ++i) REQUIRE(t.contains(i));
    INFO("n=" << n << " rotations=" << t.stats().rotations);
    REQUIRE(t.stats().rotations <= 10ULL * static_cast<std::uint64_t>(n));
  }
}

TEST_CASE("S2.5 balance theorem: total rotations <= 3 m lg n + m + n lg n from the worst initial shape", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 12; ++trial) {
    ALGO_TRIAL(trial, 75);
    Rng r(seed_);
    const int n = 1 << 13, m = 4 * n;
    Tree t = path_tree(n);
    t.reset_stats();
    for (int i = 0; i < m; ++i) {
      std::int64_t k;
      switch (trial % 3) {
        case 0: k = r.range(1, n); break;                 // uniform
        case 1: k = (i % 2 == 0) ? 1 : n; break;          // alternate extremes
        default: k = 1 + static_cast<std::int64_t>((static_cast<std::int64_t>(i) * 7919) % n);  // stride permutation
      }
      REQUIRE(t.contains(k));
    }
    const double bound = 3.0 * m * lg(n) + m + n * lg(n);
    INFO("rotations=" << t.stats().rotations << " bound=" << bound);
    REQUIRE(static_cast<double>(t.stats().rotations) <= bound);
  }
}

TEST_CASE("S2.6 static optimality: total rotations <= 3 sum m_i lg(m/m_i) + m + n lg m on skewed access", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 6; ++trial) {
    ALGO_TRIAL(trial, 76);
    Rng r(seed_);
    const int n = 2048;
    std::vector<double> weight(static_cast<std::size_t>(n));
    const double s = 0.8 + 0.25 * static_cast<double>(trial);  // Zipf exponents 0.8 .. 2.05
    double total = 0;
    for (int i = 0; i < n; ++i) { weight[static_cast<std::size_t>(i)] = 1.0 / std::pow(i + 1, s); total += weight[static_cast<std::size_t>(i)]; }
    const int m = 60000;
    std::vector<int> count(static_cast<std::size_t>(n), 0);
    std::vector<int> seq;
    for (int i = 0; i < n; ++i) seq.push_back(i);  // every key is accessed at least once (m_i >= 1)
    for (int i = n; i < m; ++i) {
      double x = r.real() * total;
      int j = 0;
      while (j + 1 < n && x > weight[static_cast<std::size_t>(j)]) { x -= weight[static_cast<std::size_t>(j)]; ++j; }
      seq.push_back(j);
    }
    r.shuffle(seq.begin(), seq.end());
    for (int j : seq) ++count[static_cast<std::size_t>(j)];
    Tree t = path_tree(n);
    t.reset_stats();
    for (int j : seq) REQUIRE(t.contains(j + 1));
    double bound = m + n * lg(m);
    for (int c : count) bound += 3.0 * c * lg(static_cast<double>(m) / c);
    INFO("zipf s=" << s << " rotations=" << t.stats().rotations << " bound=" << bound);
    REQUIRE(static_cast<double>(t.stats().rotations) <= bound);
  }
}

TEST_CASE("S2.7 dynamic finger: cost tracks sum lg(|delta|+1); local walks are far cheaper than random ones", "[S2][invariant]") {
  const int n = 1 << 14, m = 40000;
  Rng r(77);
  auto run = [&](const std::vector<int>& seq) {
    Tree t;
    std::vector<int> ord = r.permutation(n);  // build a balanced-ish random tree first
    for (int k : ord) t.insert(k + 1);
    t.reset_stats();
    for (int k : seq) REQUIRE(t.contains(k));
    return t.stats().rotations;
  };
  std::vector<int> local, random;
  int pos = n / 2;
  double finger = 0;
  for (int i = 0; i < m; ++i) {
    const int step = static_cast<int>(r.range(-3, 3));
    const int next = std::clamp(pos + step, 1, n);
    finger += lg(std::abs(next - pos) + 1);
    pos = next;
    local.push_back(pos);
    random.push_back(static_cast<int>(r.range(1, n)));
  }
  const auto local_cost = run(local), random_cost = run(random);
  INFO("local=" << local_cost << " random=" << random_cost << " finger_sum=" << finger);
  REQUIRE(static_cast<double>(local_cost) <= 20.0 * (finger + m + n));  // Cole et al.: O(m + n + sum lg(d_j + 1))
  REQUIRE(local_cost * 3 < random_cost);
}

TEST_CASE("S2.8 scale: 10^6 mixed operations stay consistent and within the balance bound", "[S2][scale]") {
  Rng r(81);
  Tree t;
  std::set<std::int64_t> ref;
  const int ops = 1'000'000;
  for (int i = 0; i < ops; ++i) {
    const std::int64_t k = r.range(0, 50000);
    if (r.below(10) < 6) { REQUIRE(t.insert(k) == ref.insert(k).second); }
    else if (r.below(2) == 0) { REQUIRE(t.erase(k) == (ref.erase(k) == 1)); }
    else { REQUIRE(t.contains(k) == (ref.count(k) == 1)); }
    if (i % 250000 == 0) t.check_invariants();
  }
  REQUIRE(t.to_vector() == std::vector<std::int64_t>(ref.begin(), ref.end()));
  const double n = static_cast<double>(std::max<std::size_t>(ref.size(), 2));
  // every operation is O(1) accesses of the tree; generous amortized ceiling: 8 lg n + 8 rotations per op
  REQUIRE(static_cast<double>(t.stats().rotations) <= ops * (8.0 * lg(n) + 8.0) + n * lg(n));
}
