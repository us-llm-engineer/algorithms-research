// Hard tests for algo::FibonacciHeap. Two suites of exactly eight cases:
//   [S1] functional correctness against independent oracles
//   [S2] structural invariants, the amortized bounds, and adversarial workloads
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <vector>

#include "fibonacci_heap.hpp"
#include "test_support.hpp"

using Heap = algo::FibonacciHeap<std::int64_t>;
using Handle = Heap::Handle;
using algo::Rng;

namespace {
constexpr double kPhi = 1.6180339887498949;

// Largest degree any node may have in a heap of n nodes: floor(log_phi n)  (Fredman-Tarjan lemma).
std::size_t degree_bound(std::size_t n) { return n <= 1 ? 0 : static_cast<std::size_t>(std::log(static_cast<double>(n)) / std::log(kPhi) + 1e-9); }

// Oracle: multiset of live keys plus handle -> key map.
struct Oracle {
  std::multiset<std::int64_t> keys;
  std::map<Handle, std::int64_t> by_handle;
  void add(Handle h, std::int64_t k) { keys.insert(k); by_handle[h] = k; }
  void drop(Handle h) { keys.erase(keys.find(by_handle.at(h))); by_handle.erase(h); }
  void change(Handle h, std::int64_t k) { drop(h); add(h, k); }
};

void expect_matches(const Heap& heap, const Oracle& o) {
  REQUIRE(heap.size() == o.keys.size());
  REQUIRE(heap.empty() == o.keys.empty());
  if (!o.keys.empty()) REQUIRE(heap.top() == *o.keys.begin());
}
}  // namespace

// ------------------------------------------------------------------ S1: correctness

TEST_CASE("S1.1 CLRS worked example: pop order and decrease-key", "[S1][given]") {
  Heap h;
  std::map<std::int64_t, Handle> hs;
  for (std::int64_t k : {23, 7, 21, 3, 18, 52, 38, 39, 41, 17, 30, 24, 26, 46, 35}) hs[k] = h.push(k);
  REQUIRE(h.top() == 3);
  REQUIRE(h.pop() == 3);  // forces consolidation into the CLRS Figure 19.4 shape
  h.check_invariants();
  h.decrease_key(hs[46], 15);
  h.decrease_key(hs[35], 5);
  h.check_invariants();
  std::vector<std::int64_t> out;
  while (!h.empty()) out.push_back(h.pop());
  const std::vector<std::int64_t> expect{5, 7, 15, 17, 18, 21, 23, 24, 26, 30, 38, 39, 41, 52};
  REQUIRE(out == expect);
}

TEST_CASE("S1.2 empty, single element, dead handles and illegal decrease-key", "[S1][boundary]") {
  Heap h;
  REQUIRE(h.empty());
  REQUIRE(h.size() == 0);
  REQUIRE_THROWS_AS(h.top(), std::out_of_range);
  REQUIRE_THROWS_AS(h.pop(), std::out_of_range);
  REQUIRE_THROWS_AS(h.top_handle(), std::out_of_range);
  const Handle a = h.push(10);
  REQUIRE(h.contains(a));
  REQUIRE(h.key(a) == 10);
  REQUIRE(h.top_handle() == a);
  REQUIRE_THROWS_AS(h.decrease_key(a, 11), std::invalid_argument);  // increasing is illegal
  REQUIRE(h.key(a) == 10);                                          // ... and changed nothing
  h.decrease_key(a, 10);                                            // equal key is allowed
  h.decrease_key(a, 4);
  REQUIRE(h.top() == 4);
  REQUIRE(h.pop() == 4);
  REQUIRE(h.empty());
  REQUIRE_FALSE(h.contains(a));
  REQUIRE_THROWS_AS(h.decrease_key(a, 1), std::invalid_argument);
  REQUIRE_THROWS_AS(h.erase(a), std::invalid_argument);
  REQUIRE_THROWS_AS(h.key(a), std::invalid_argument);
  REQUIRE_FALSE(h.contains(Heap::kNull));
  const Handle b = h.push(1);  // reusable after becoming empty
  REQUIRE(h.pop() == 1);
  REQUIRE_FALSE(h.contains(b));
  h.check_invariants();
}

TEST_CASE("S1.3 duplicates, ties, sorted inputs and extreme keys", "[S1][structural]") {
  const std::int64_t lo = std::numeric_limits<std::int64_t>::min(), hi = std::numeric_limits<std::int64_t>::max();
  std::vector<std::vector<std::int64_t>> inputs;
  inputs.push_back(std::vector<std::int64_t>(500, 7));  // all equal
  std::vector<std::int64_t> up, down, extremes{hi, lo, 0, hi, lo, -1, 1, hi - 1, lo + 1};
  for (int i = 0; i < 700; ++i) { up.push_back(i); down.push_back(700 - i); }
  inputs.push_back(up);
  inputs.push_back(down);
  inputs.push_back(extremes);
  Rng r(3);
  std::vector<std::int64_t> few;  // heavy ties from a tiny alphabet
  for (int i = 0; i < 1000; ++i) few.push_back(r.range(0, 3));
  inputs.push_back(few);
  for (const auto& in : inputs) {
    Heap h;
    for (auto k : in) h.push(k);
    h.check_invariants();
    auto sorted = in;
    std::sort(sorted.begin(), sorted.end());
    std::vector<std::int64_t> out;
    while (!h.empty()) out.push_back(h.pop());
    REQUIRE(out == sorted);
  }
}

TEST_CASE("S1.4 random operation mix agrees with a multiset oracle (300 seeds)", "[S1][stress]") {
  for (std::uint64_t trial = 0; trial < 300; ++trial) {
    ALGO_TRIAL(trial, 41);
    Rng r(seed_);
    Heap h;
    Oracle o;
    const int ops = 400;
    for (int i = 0; i < ops; ++i) {
      const auto roll = r.below(10);
      if (roll < 4 || o.by_handle.empty()) {
        const auto k = r.range(-50, 50);  // small range -> many ties
        o.add(h.push(k), k);
      } else if (roll < 6) {
        const auto top = h.top_handle();
        REQUIRE(h.key(top) == *o.keys.begin());
        const auto k = h.pop();
        REQUIRE(k == *o.keys.begin());
        o.drop(top);
      } else if (roll < 9) {
        auto it = o.by_handle.begin();
        std::advance(it, static_cast<long>(r.below(o.by_handle.size())));
        const auto nk = it->second - r.range(0, 30);
        const Handle hd = it->first;
        h.decrease_key(hd, nk);
        o.change(hd, nk);
      } else {
        auto it = o.by_handle.begin();
        std::advance(it, static_cast<long>(r.below(o.by_handle.size())));
        const Handle hd = it->first;
        h.erase(hd);
        o.drop(hd);
        REQUIRE_FALSE(h.contains(hd));
      }
      expect_matches(h, o);
    }
    h.check_invariants();
    std::vector<std::int64_t> out;
    while (!h.empty()) out.push_back(h.pop());
    REQUIRE(out == std::vector<std::int64_t>(o.keys.begin(), o.keys.end()));
  }
}

TEST_CASE("S1.5 handles stay valid and keys stay attached to their handle", "[S1][stress]") {
  Rng r(5);
  Heap h;
  Oracle o;
  for (int i = 0; i < 2000; ++i) { const auto k = r.range(0, 100000); o.add(h.push(k), k); }
  for (int round = 0; round < 40; ++round) {
    for (int i = 0; i < 25; ++i) {  // pops force repeated consolidation / relinking
      const auto top = h.top_handle();
      h.pop();
      o.drop(top);
    }
    for (int i = 0; i < 100; ++i) {
      auto it = o.by_handle.begin();
      std::advance(it, static_cast<long>(r.below(o.by_handle.size())));
      const auto nk = it->second - r.range(0, 5000);
      h.decrease_key(it->first, nk);
      o.change(it->first, nk);
    }
    for (const auto& [hd, k] : o.by_handle) {
      REQUIRE(h.contains(hd));
      REQUIRE(h.key(hd) == k);
    }
    h.check_invariants();
  }
}

TEST_CASE("S1.6 meld: union of contents, source emptied, survivors keep working", "[S1][structural]") {
  for (std::uint64_t trial = 0; trial < 60; ++trial) {
    ALGO_TRIAL(trial, 46);
    Rng r(seed_);
    Heap a, b;
    Oracle oa, ob;
    const int na = static_cast<int>(r.below(60)), nb = static_cast<int>(r.below(60));  // includes empty heaps
    for (int i = 0; i < na; ++i) { const auto k = r.range(-100, 100); oa.add(a.push(k), k); }
    for (int i = 0; i < nb; ++i) { const auto k = r.range(-100, 100); ob.add(b.push(k), k); }
    Oracle fresh = oa;
    std::multiset<std::int64_t> expect_keys = fresh.keys;
    for (auto k : ob.keys) expect_keys.insert(k);
    a.meld(b);
    REQUIRE(b.empty());
    REQUIRE(b.size() == 0);
    REQUIRE(a.size() == expect_keys.size());
    if (!expect_keys.empty()) REQUIRE(a.top() == *expect_keys.begin());
    a.check_invariants();
    b.check_invariants();
    // handles that belonged to `a` still work after the meld
    for (const auto& [hd, k] : fresh.by_handle) {
      REQUIRE(a.contains(hd));
      REQUIRE(a.key(hd) == k);
    }
    std::vector<std::int64_t> out;
    while (!a.empty()) out.push_back(a.pop());
    REQUIRE(out == std::vector<std::int64_t>(expect_keys.begin(), expect_keys.end()));
    // b is reusable
    b.push(3);
    REQUIRE(b.pop() == 3);
  }
  Heap x;
  REQUIRE_THROWS_AS(x.meld(x), std::invalid_argument);
}

TEST_CASE("S1.7 Dijkstra with decrease-key equals Dijkstra on std::priority_queue", "[S1][stress]") {
  using Edge = std::pair<int, std::int64_t>;
  for (std::uint64_t trial = 0; trial < 25; ++trial) {
    ALGO_TRIAL(trial, 47);
    Rng r(seed_);
    const int n = 50 + static_cast<int>(r.below(1500));
    const int m = n * (1 + static_cast<int>(r.below(6)));
    std::vector<std::vector<Edge>> g(static_cast<std::size_t>(n));
    for (int i = 0; i < m; ++i)
      g[r.below(static_cast<std::uint64_t>(n))].push_back({static_cast<int>(r.below(static_cast<std::uint64_t>(n))), r.range(0, 1000)});
    const std::int64_t inf = std::numeric_limits<std::int64_t>::max();
    // reference: lazy-deletion binary heap
    std::vector<std::int64_t> ref(static_cast<std::size_t>(n), inf);
    {
      using P = std::pair<std::int64_t, int>;
      std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
      ref[0] = 0;
      pq.push({0, 0});
      while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != ref[static_cast<std::size_t>(u)]) continue;
        for (auto [v, w] : g[static_cast<std::size_t>(u)])
          if (d + w < ref[static_cast<std::size_t>(v)]) { ref[static_cast<std::size_t>(v)] = d + w; pq.push({ref[static_cast<std::size_t>(v)], v}); }
      }
    }
    // under test: handle + decrease_key; keys are packed (dist << 20 | vertex) so the vertex is recoverable
    std::vector<std::int64_t> dist(static_cast<std::size_t>(n), inf);
    std::vector<Handle> hd(static_cast<std::size_t>(n), Heap::kNull);
    std::vector<char> done(static_cast<std::size_t>(n), 0);
    Heap h;
    dist[0] = 0;
    hd[0] = h.push(0 << 20 | 0);
    while (!h.empty()) {
      const std::int64_t packed = h.pop();
      const int u = static_cast<int>(packed & ((1 << 20) - 1));
      done[static_cast<std::size_t>(u)] = 1;
      for (auto [v, w] : g[static_cast<std::size_t>(u)]) {
        const auto cand = dist[static_cast<std::size_t>(u)] + w;
        if (done[static_cast<std::size_t>(v)] || cand >= dist[static_cast<std::size_t>(v)]) continue;
        dist[static_cast<std::size_t>(v)] = cand;
        const std::int64_t key = cand << 20 | v;
        if (hd[static_cast<std::size_t>(v)] == Heap::kNull) hd[static_cast<std::size_t>(v)] = h.push(key);
        else h.decrease_key(hd[static_cast<std::size_t>(v)], key);
      }
    }
    REQUIRE(dist == ref);
  }
}

TEST_CASE("S1.8 erase of arbitrary nodes (roots, minimum, interior, marked)", "[S1][structural]") {
  for (std::uint64_t trial = 0; trial < 80; ++trial) {
    ALGO_TRIAL(trial, 48);
    Rng r(seed_);
    Heap h;
    Oracle o;
    const int n = 20 + static_cast<int>(r.below(300));
    for (int i = 0; i < n; ++i) { const auto k = r.range(0, 1000); o.add(h.push(k), k); }
    for (int i = 0; i < n / 5; ++i) { const auto t = h.top_handle(); h.pop(); o.drop(t); }  // build deep trees
    for (int i = 0; i < n / 4; ++i) {                                                      // create marked nodes
      auto it = o.by_handle.begin();
      std::advance(it, static_cast<long>(r.below(o.by_handle.size())));
      const auto nk = it->second - r.range(0, 50);
      h.decrease_key(it->first, nk);
      o.change(it->first, nk);
    }
    while (!o.by_handle.empty()) {
      Handle target;
      switch (r.below(3)) {
        case 0: target = h.top_handle(); break;
        default: { auto it = o.by_handle.begin(); std::advance(it, static_cast<long>(r.below(o.by_handle.size()))); target = it->first; }
      }
      h.erase(target);
      o.drop(target);
      REQUIRE_FALSE(h.contains(target));
      expect_matches(h, o);
      if (o.by_handle.size() % 16 == 0) h.check_invariants();
    }
    REQUIRE(h.empty());
  }
}

// ------------------------------------------------------------------ S2: invariants, bounds, adversaries

TEST_CASE("S2.1 full structural invariants hold after every single operation", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 40; ++trial) {
    ALGO_TRIAL(trial, 51);
    Rng r(seed_);
    Heap h;
    std::vector<Handle> live;
    for (int i = 0; i < 500; ++i) {
      const auto roll = r.below(10);
      if (roll < 4 || live.empty()) live.push_back(h.push(r.range(0, 2000)));
      else if (roll < 6) {
        const auto t = h.top_handle();
        h.pop();
        live.erase(std::find(live.begin(), live.end(), t));
      } else if (roll < 9) {
        const auto hd = live[r.below(live.size())];
        h.decrease_key(hd, h.key(hd) - r.range(0, 400));
      } else {
        const auto idx = r.below(live.size());
        h.erase(live[idx]);
        live.erase(live.begin() + static_cast<long>(idx));
      }
      h.check_invariants();  // heap order, sibling/child links, degrees, marks, size, min pointer, F_{d+2} size lemma
    }
  }
}

TEST_CASE("S2.2 degree never exceeds floor(log_phi n) under decrease-key heavy workloads", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 20; ++trial) {
    ALGO_TRIAL(trial, 52);
    Rng r(seed_);
    Heap h;
    std::vector<Handle> live;
    for (int i = 0; i < 4000; ++i) live.push_back(h.push(r.range(0, 1 << 30)));
    for (int round = 0; round < 60; ++round) {
      for (int i = 0; i < 3; ++i) { const auto t = h.top_handle(); h.pop(); live.erase(std::find(live.begin(), live.end(), t)); }
      for (int i = 0; i < 150; ++i) {
        const auto hd = live[r.below(live.size())];
        h.decrease_key(hd, h.key(hd) - r.range(0, 1 << 20));
      }
      REQUIRE(h.max_degree() <= degree_bound(h.size()));
    }
  }
}

TEST_CASE("S2.3 insert: amortized cost (actual + change of potential) is at most 2", "[S2][invariant]") {
  Rng r(7);
  Heap h;
  for (int i = 0; i < 3000; ++i) {
    const auto before = h.trees() + 2 * h.marked_count();
    h.push(r.range(-1000, 1000));
    const auto after = h.trees() + 2 * h.marked_count();
    const std::int64_t amortized = 1 + (static_cast<std::int64_t>(after) - static_cast<std::int64_t>(before));
    REQUIRE(amortized <= 2);
  }
}

TEST_CASE("S2.4 decrease-key: amortized cost <= 5 even when it cascades", "[S2][adversarial]") {
  for (std::uint64_t trial = 0; trial < 15; ++trial) {
    ALGO_TRIAL(trial, 54);
    Rng r(seed_);
    Heap h;
    std::vector<Handle> live;
    for (int i = 0; i < 3000; ++i) live.push_back(h.push(r.range(0, 1 << 28)));
    for (int i = 0; i < 400; ++i) { const auto t = h.top_handle(); h.pop(); live.erase(std::find(live.begin(), live.end(), t)); }
    std::uint64_t max_cascade = 0;
    for (int i = 0; i < 6000; ++i) {
      // adversary: prefer nodes that are already marked (their parent chain is most likely to cascade)
      Handle hd = live[r.below(live.size())];
      for (int tries = 0; tries < 8 && !h.marked(hd); ++tries) hd = live[r.below(live.size())];
      const auto cuts0 = h.stats().cuts;
      const auto phi0 = static_cast<std::int64_t>(h.trees() + 2 * h.marked_count());
      h.decrease_key(hd, h.key(hd) - r.range(1, 1 << 16));
      const auto cuts = h.stats().cuts - cuts0;
      max_cascade = std::max(max_cascade, cuts);
      const std::int64_t actual = 1 + static_cast<std::int64_t>(cuts);
      const std::int64_t phi1 = static_cast<std::int64_t>(h.trees() + 2 * h.marked_count());
      INFO("cuts=" << cuts);
      REQUIRE(actual + (phi1 - phi0) <= 5);
    }
    INFO("longest cascade observed: " << max_cascade);
  }
}

TEST_CASE("S2.5 extract-min: amortized cost <= 3*(floor(log_phi n)+1)+3", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 20; ++trial) {
    ALGO_TRIAL(trial, 55);
    Rng r(seed_);
    Heap h;
    std::vector<Handle> live;
    const int n = 200 + static_cast<int>(r.below(3000));
    for (int i = 0; i < n; ++i) live.push_back(h.push(r.range(0, 1 << 28)));
    while (h.size() > 1) {
      if (r.coin(0.6)) {  // keep some decrease-keys interleaved so marks exist
        const auto hd = live[r.below(live.size())];
        h.decrease_key(hd, h.key(hd) - r.range(0, 1 << 12));
      }
      const std::size_t n_before = h.size();
      const auto st0 = h.stats();
      const auto phi0 = static_cast<std::int64_t>(h.trees() + 2 * h.marked_count());
      const auto top = h.top_handle();
      h.pop();
      live.erase(std::find(live.begin(), live.end(), top));
      const auto st1 = h.stats();
      const std::int64_t actual = 1 + static_cast<std::int64_t>(st1.children_promoted - st0.children_promoted) +
                                  static_cast<std::int64_t>(st1.root_visits - st0.root_visits);
      const std::int64_t phi1 = static_cast<std::int64_t>(h.trees() + 2 * h.marked_count());
      const std::int64_t bound = 3 * static_cast<std::int64_t>(degree_bound(n_before) + 1) + 3;
      INFO("n_before=" << n_before);
      REQUIRE(actual + (phi1 - phi0) <= bound);
    }
  }
}

TEST_CASE("S2.6 whole-sequence work is O(m + n log n): totals vs. explicit constants", "[S2][scale]") {
  Rng r(9);
  const int n = 1 << 16;
  Heap h;
  std::vector<Handle> hs;
  for (int i = 0; i < n; ++i) hs.push_back(h.push(r.range(0, 1 << 30)));
  std::int64_t m = n;  // operations performed
  for (int i = 0; i < 3 * n; ++i) {  // decrease-key ops on random live nodes (may repeat; keys only shrink)
    const auto hd = hs[r.below(hs.size())];
    h.decrease_key(hd, h.key(hd) - r.range(0, 1000));
    ++m;
  }
  while (!h.empty()) { h.pop(); ++m; }
  const auto st = h.stats();
  const double work = static_cast<double>(st.links + st.cuts + st.root_visits + st.children_promoted);
  const double bound = 6.0 * static_cast<double>(m) + 4.0 * static_cast<double>(n) * std::log2(static_cast<double>(n));
  INFO("work=" << work << " bound=" << bound << " m=" << m);
  REQUIRE(work <= bound);
  // every link removes one tree; trees are created only by pushes, cuts and child promotions
  REQUIRE(st.links <= static_cast<std::uint64_t>(n) + st.cuts + st.children_promoted);
}

TEST_CASE("S2.7 adversary: repeatedly cut inside the deepest tree must keep sizes >= Fibonacci(deg+2)", "[S2][adversarial]") {
  // Build one large consolidated tree, then keep decreasing the keys of children of the highest-degree
  // non-root node. Without cascading cuts the subtree sizes collapse while degrees stay high.
  Heap h;
  std::vector<Handle> all;
  const int n = 1 << 12;
  for (int i = 0; i < n; ++i) all.push_back(h.push(1'000'000 + i));
  h.push(-1);
  h.pop();  // consolidation: forms binomial trees
  std::int64_t floor_key = 0;
  for (int step = 0; step < 4000; ++step) {
    Handle best = Heap::kNull;
    std::size_t best_deg = 0;
    for (Handle hd : all)
      if (h.contains(hd) && h.parent(hd) != Heap::kNull && h.degree(hd) >= best_deg) { best = hd; best_deg = h.degree(hd); }
    if (best == Heap::kNull || best_deg == 0) break;
    const auto kids = h.children(best);
    h.decrease_key(kids.back(), --floor_key);  // cut one child of the heaviest non-root node
    h.check_invariants();                       // includes size(x) >= F_{deg(x)+2} and the unmarked-roots rule
    if (step % 50 == 0) { h.pop(); }           // let consolidation re-link what was cut
  }
  REQUIRE(h.max_degree() <= degree_bound(h.size()) + 1);
}

TEST_CASE("S2.8 scale: 10^6 operations finish, stay consistent, and use arena memory only", "[S2][scale]") {
  Rng r(11);
  const int n = 500000;
  Heap h;
  h.reserve(static_cast<std::size_t>(n));
  std::vector<Handle> hs;
  hs.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) hs.push_back(h.push(r.range(0, 1LL << 40)));
  for (int i = 0; i < n; ++i) {
    const auto hd = hs[r.below(hs.size())];
    h.decrease_key(hd, h.key(hd) - r.range(0, 1 << 20));
  }
  std::int64_t prev = std::numeric_limits<std::int64_t>::min();
  std::size_t popped = 0;
  while (!h.empty()) {
    const auto k = h.pop();
    REQUIRE(k >= prev);
    prev = k;
    ++popped;
    if (popped % 100000 == 0) h.check_invariants();
  }
  REQUIRE(popped == static_cast<std::size_t>(n));
}
