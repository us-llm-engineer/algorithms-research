// Hard tests for algo::VebTree (dense, O(U) space) and algo::SparseVebTree (lazily allocated clusters).
//   [S1] exact agreement with std::set, exhaustively on tiny universes and randomly on large ones
//   [S2] recursion-depth / call-count evidence for T(U) = T(sqrt U) + O(1), invariants and adversarial layouts
#include <algorithm>
#include <catch2/catch_template_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

#include "test_support.hpp"
#include "van_emde_boas.hpp"

using algo::Rng;
using algo::SparseVebTree;
using algo::VebTree;
using Opt = std::optional<std::uint32_t>;

namespace {
template <class T> constexpr unsigned kMaxBits = 0;
template <> constexpr unsigned kMaxBits<VebTree> = 24;
template <> constexpr unsigned kMaxBits<SparseVebTree> = 32;

Opt ref_succ(const std::set<std::uint32_t>& s, std::uint32_t x) { auto it = s.upper_bound(x); return it == s.end() ? Opt() : Opt(*it); }
Opt ref_pred(const std::set<std::uint32_t>& s, std::uint32_t x) { auto it = s.lower_bound(x); return it == s.begin() ? Opt() : Opt(*std::prev(it)); }
unsigned depth_bound(unsigned bits) { return static_cast<unsigned>(std::ceil(std::log2(static_cast<double>(bits)) - 1e-12)) + 1; }

template <class T>
void require_same(const T& v, const std::set<std::uint32_t>& ref, std::uint32_t probe) {
  REQUIRE(v.size() == ref.size());
  REQUIRE(v.contains(probe) == (ref.count(probe) == 1));
  REQUIRE(v.successor(probe) == ref_succ(ref, probe));
  REQUIRE(v.predecessor(probe) == ref_pred(ref, probe));
  REQUIRE(v.min() == (ref.empty() ? Opt() : Opt(*ref.begin())));
  REQUIRE(v.max() == (ref.empty() ? Opt() : Opt(*ref.rbegin())));
}
std::uint32_t pick(Rng& r, unsigned bits) { return static_cast<std::uint32_t>(r.below(1ULL << bits)); }
}  // namespace

// ------------------------------------------------------------------ S1

TEMPLATE_TEST_CASE("S1.1 CLRS example on U=16", "[S1][given]", VebTree, SparseVebTree) {
  TestType v(4);
  REQUIRE(v.universe() == 16);
  for (std::uint32_t k : {2, 3, 4, 5, 7, 14, 15}) REQUIRE(v.insert(k));
  REQUIRE(v.min() == 2);
  REQUIRE(v.max() == 15);
  REQUIRE(v.successor(5) == 7);
  REQUIRE(v.successor(7) == 14);
  REQUIRE(v.successor(15) == std::nullopt);
  REQUIRE(v.predecessor(14) == 7);
  REQUIRE(v.predecessor(2) == std::nullopt);
  REQUIRE(v.erase(2));
  REQUIRE(v.min() == 3);
  REQUIRE(v.erase(15));
  REQUIRE(v.max() == 14);
  v.check_invariants();
}

TEMPLATE_TEST_CASE("S1.2 exhaustive: every subset of the universes of 1, 2, 3 and 4 bits", "[S1][boundary]", VebTree, SparseVebTree) {
  for (unsigned bits = 1; bits <= 4; ++bits) {
    const std::uint32_t u = 1u << bits;
    for (std::uint32_t mask = 0; mask < (1u << u); ++mask) {
      TestType v(bits);
      std::set<std::uint32_t> ref;
      for (std::uint32_t x = 0; x < u; ++x)
        if (mask >> x & 1) { REQUIRE(v.insert(x)); ref.insert(x); }
      for (std::uint32_t x = 0; x < u; ++x) require_same(v, ref, x);
      // delete in ascending order, re-checking after each removal
      for (std::uint32_t x = 0; x < u; ++x)
        if (ref.count(x)) {
          REQUIRE(v.erase(x));
          ref.erase(x);
          require_same(v, ref, x);
          require_same(v, ref, u - 1 - x);
        }
      v.check_invariants();
    }
  }
}

TEMPLATE_TEST_CASE("S1.3 range errors, duplicates, absent erase, constructor limits", "[S1][boundary]", VebTree, SparseVebTree) {
  REQUIRE_THROWS_AS(TestType(0), std::invalid_argument);
  REQUIRE_THROWS_AS(TestType(kMaxBits<TestType> + 1), std::invalid_argument);
  TestType v(5);
  REQUIRE(v.empty());
  REQUIRE(v.min() == std::nullopt);
  REQUIRE(v.max() == std::nullopt);
  REQUIRE(v.successor(0) == std::nullopt);
  REQUIRE(v.predecessor(31) == std::nullopt);
  REQUIRE_FALSE(v.erase(3));
  REQUIRE_FALSE(v.contains(3));
  REQUIRE(v.insert(3));
  REQUIRE_FALSE(v.insert(3));
  REQUIRE(v.size() == 1);
  REQUIRE(v.min() == 3);
  REQUIRE(v.max() == 3);
  for (std::uint32_t bad : {32u, 33u, 1000u, 0xFFFFFFFFu}) {
    REQUIRE_THROWS_AS(v.insert(bad), std::out_of_range);
    REQUIRE_THROWS_AS(v.erase(bad), std::out_of_range);
    REQUIRE_THROWS_AS(v.contains(bad), std::out_of_range);
    REQUIRE_THROWS_AS(v.successor(bad), std::out_of_range);
    REQUIRE_THROWS_AS(v.predecessor(bad), std::out_of_range);
  }
  REQUIRE(v.size() == 1);  // failed operations changed nothing
  REQUIRE(v.successor(31) == std::nullopt);
  REQUIRE(v.predecessor(0) == std::nullopt);
  REQUIRE(v.predecessor(31) == 3);
  v.check_invariants();
}

TEMPLATE_TEST_CASE("S1.4 random operations equal std::set on many universe sizes (100 seeds each)", "[S1][stress]", VebTree, SparseVebTree) {
  const std::vector<unsigned> sizes = std::is_same_v<TestType, VebTree> ? std::vector<unsigned>{5, 7, 12, 16, 20}
                                                                        : std::vector<unsigned>{5, 7, 12, 17, 31, 32};
  for (unsigned bits : sizes)
    for (std::uint64_t trial = 0; trial < 100; ++trial) {
      ALGO_TRIAL(trial, 100 + bits);
      Rng r(seed_);
      TestType v(bits);
      std::set<std::uint32_t> ref;
      for (int i = 0; i < 200; ++i) {
        // mostly draw from a small hot set so inserts/erases collide; sometimes from the whole universe
        const std::uint32_t x = r.coin(0.7) ? static_cast<std::uint32_t>(r.below(std::min<std::uint64_t>(1ULL << bits, 24))) : pick(r, bits);
        if (r.coin(0.55)) { REQUIRE(v.insert(x) == ref.insert(x).second); }
        else { REQUIRE(v.erase(x) == (ref.erase(x) == 1)); }
        require_same(v, ref, pick(r, bits));
      }
      v.check_invariants();
    }
}

TEMPLATE_TEST_CASE("S1.5 min/max maintenance: drain from either end and interleave", "[S1][structural]", VebTree, SparseVebTree) {
  Rng r(5);
  for (int rep = 0; rep < 30; ++rep) {
    TestType v(14);
    std::set<std::uint32_t> ref;
    for (int i = 0; i < 400; ++i) { const auto x = pick(r, 14); v.insert(x); ref.insert(x); }
    while (!ref.empty()) {
      const std::uint32_t victim = r.coin() ? *ref.begin() : *ref.rbegin();
      REQUIRE(v.erase(victim));
      ref.erase(victim);
      REQUIRE(v.min() == (ref.empty() ? Opt() : Opt(*ref.begin())));
      REQUIRE(v.max() == (ref.empty() ? Opt() : Opt(*ref.rbegin())));
      if (r.coin(0.15)) { const auto x = pick(r, 14); v.insert(x); ref.insert(x); }
    }
    REQUIRE(v.empty());
    REQUIRE(v.min() == std::nullopt);
  }
}

TEMPLATE_TEST_CASE("S1.6 odd bit-lengths split unevenly: results still exact", "[S1][structural]", VebTree, SparseVebTree) {
  for (unsigned bits : {3u, 5u, 9u, 11u, 13u, 15u, 17u, 19u, 21u, 23u}) {
    Rng r(bits);
    TestType v(bits);
    std::set<std::uint32_t> ref;
    const std::uint32_t top = static_cast<std::uint32_t>((1ULL << bits) - 1);
    for (std::uint32_t x : {0u, top, top / 2, top / 2 + 1}) { v.insert(x); ref.insert(x); }
    for (int i = 0; i < 3000; ++i) {
      const auto x = pick(r, bits);
      if (r.coin()) { v.insert(x); ref.insert(x); } else { v.erase(x); ref.erase(x); }
      for (std::uint32_t probe : {x, static_cast<std::uint32_t>(0), top, static_cast<std::uint32_t>(r.below(top + 1ULL))}) require_same(v, ref, probe);
    }
  }
}

TEMPLATE_TEST_CASE("S1.7 completely full universe, then erase everything in random order", "[S1][structural]", VebTree, SparseVebTree) {
  const unsigned bits = 12;
  const std::uint32_t u = 1u << bits;
  TestType v(bits);
  std::set<std::uint32_t> ref;
  for (std::uint32_t x = 0; x < u; ++x) { REQUIRE(v.insert(x)); ref.insert(x); }
  REQUIRE(v.size() == u);
  for (std::uint32_t x = 0; x + 1 < u; ++x) REQUIRE(v.successor(x) == x + 1);
  Rng r(7);
  std::vector<int> order = r.permutation(static_cast<int>(u));
  for (int x : order) {
    REQUIRE(v.erase(static_cast<std::uint32_t>(x)));
    ref.erase(static_cast<std::uint32_t>(x));
    if (x % 37 == 0) require_same(v, ref, static_cast<std::uint32_t>(x));
  }
  REQUIRE(v.empty());
  v.check_invariants();
}

TEST_CASE("S1.8 memory: dense is Theta(U) bytes; sparse is proportional to n, not U", "[S1][scale]") {
  VebTree dense(20);
  const std::size_t empty_bytes = dense.memory_bytes();
  REQUIRE(empty_bytes >= (1u << 20) / 8);        // must at least hold a bit per universe element
  REQUIRE(empty_bytes <= 64u * (1u << 20));      // ... and stay O(U)
  SparseVebTree sparse(32);
  REQUIRE(sparse.memory_bytes() < 4096);         // empty 2^32 universe costs almost nothing
  Rng r(3);
  const int n = 100000;
  std::set<std::uint32_t> ref;
  for (int i = 0; i < n; ++i) { const auto x = static_cast<std::uint32_t>(r()); sparse.insert(x); ref.insert(x); }
  REQUIRE(sparse.size() == ref.size());
  REQUIRE(sparse.memory_bytes() <= static_cast<std::size_t>(n) * 1024);  // O(n loglog U) words with room for hashing overhead
  REQUIRE(sparse.memory_bytes() < (1ULL << 32) / 8);  // far below a bitmap of the universe
}

// ------------------------------------------------------------------ S2

TEMPLATE_TEST_CASE("S2.1 invariants (min/max rules, summary consistency, sizes) after every operation", "[S2][invariant]", VebTree, SparseVebTree) {
  for (unsigned bits : {2u, 6u, 11u, 16u}) {
    for (std::uint64_t trial = 0; trial < 25; ++trial) {
      ALGO_TRIAL(trial, 200 + bits);
      Rng r(seed_);
      TestType v(bits);
      for (int i = 0; i < 200; ++i) {
        const auto x = static_cast<std::uint32_t>(r.below(std::min<std::uint64_t>(1ULL << bits, 40)));
        if (r.coin()) v.insert(x); else v.erase(x);
        v.check_invariants();
      }
    }
  }
}

TEMPLATE_TEST_CASE("S2.2 recursion depth <= ceil(lg bits)+1 and calls <= 2x that, for every operation", "[S2][invariant]", VebTree, SparseVebTree) {
  const unsigned top = kMaxBits<TestType>;
  for (unsigned bits = 1; bits <= top; bits += (bits < 8 ? 1 : 3)) {
    Rng r(bits);
    TestType v(bits);
    const unsigned bound = depth_bound(bits);
    for (int i = 0; i < 1500; ++i) {
      const auto x = pick(r, bits);
      switch (r.below(5)) {
        case 0: case 1: v.insert(x); break;
        case 2: v.erase(x); break;
        case 3: v.successor(x); break;
        default: v.predecessor(x);
      }
      INFO("bits=" << bits << " depth=" << v.last_depth() << " calls=" << v.last_calls() << " bound=" << bound);
      REQUIRE(v.last_depth() <= bound);
      REQUIRE(v.last_calls() <= 2 * bound);
    }
  }
}

TEST_CASE("S2.3 calls per operation grow like lg(bits), not like bits (a second recursion would double per level)", "[S2][adversarial]") {
  auto mean_calls = [](unsigned bits) {
    Rng r(bits);
    SparseVebTree v(bits);
    double total = 0;
    int ops = 0;
    for (int i = 0; i < 6000; ++i) {
      const auto x = pick(r, bits);
      if (r.coin(0.6)) v.insert(x); else v.erase(x);
      total += v.last_calls(); ++ops;
      v.successor(pick(r, bits)); total += v.last_calls(); ++ops;
      v.predecessor(pick(r, bits)); total += v.last_calls(); ++ops;
    }
    return total / ops;
  };
  const double c8 = mean_calls(8), c16 = mean_calls(16), c32 = mean_calls(32);
  INFO("mean calls: k=8 -> " << c8 << ", k=16 -> " << c16 << ", k=32 -> " << c32);
  REQUIRE(c32 / c8 <= 2.5);
  REQUIRE(c32 <= 12.0);
}

TEMPLATE_TEST_CASE("S2.4 adversarial layouts: one cluster, one element per cluster, alternating clusters", "[S2][adversarial]", VebTree, SparseVebTree) {
  const unsigned bits = 16;  // clusters of 2^8
  std::vector<std::vector<std::uint32_t>> layouts(3);
  for (std::uint32_t i = 0; i < 256; ++i) layouts[0].push_back(5 * 256 + i);          // everything in cluster 5
  for (std::uint32_t c = 0; c < 256; ++c) layouts[1].push_back(c * 256 + (c * 7) % 256);  // one per cluster
  for (std::uint32_t c = 0; c < 256; c += 2) { layouts[2].push_back(c * 256); layouts[2].push_back(c * 256 + 255); }
  for (const auto& layout : layouts) {
    TestType v(bits);
    std::set<std::uint32_t> ref(layout.begin(), layout.end());
    for (auto x : layout) v.insert(x);
    // walking successors from the minimum must enumerate the set in order
    std::vector<std::uint32_t> walked;
    for (Opt x = v.min(); x; x = v.successor(*x)) walked.push_back(*x);
    REQUIRE(walked == std::vector<std::uint32_t>(ref.begin(), ref.end()));
    std::vector<std::uint32_t> back;
    for (Opt x = v.max(); x; x = v.predecessor(*x)) back.push_back(*x);
    std::reverse(back.begin(), back.end());
    REQUIRE(back == walked);
    v.check_invariants();
  }
}

TEMPLATE_TEST_CASE("S2.5 emptying a whole cluster must update the summary (successor skips empty clusters)", "[S2][adversarial]", VebTree, SparseVebTree) {
  const unsigned bits = 12;  // 64 clusters of 64
  TestType v(bits);
  std::set<std::uint32_t> ref;
  for (std::uint32_t c : {3u, 4u, 5u, 40u, 41u})
    for (std::uint32_t i = 0; i < 64; ++i) { v.insert(c * 64 + i); ref.insert(c * 64 + i); }
  for (std::uint32_t c : {4u, 41u}) {
    for (std::uint32_t i = 0; i < 64; ++i) { REQUIRE(v.erase(c * 64 + i)); ref.erase(c * 64 + i); v.check_invariants(); }
  }
  REQUIRE(v.successor(3 * 64 + 63) == 5 * 64);
  REQUIRE(v.predecessor(5 * 64) == 3 * 64 + 63);
  REQUIRE(v.successor(5 * 64 + 63) == 40 * 64);
  REQUIRE(v.successor(40 * 64 + 63) == std::nullopt);
  for (std::uint32_t x = 0; x < (1u << bits); x += 17) require_same(v, ref, x);
}

TEST_CASE("S2.6 dense and sparse implementations return identical answers for identical histories", "[S2][stress]") {
  for (std::uint64_t trial = 0; trial < 60; ++trial) {
    ALGO_TRIAL(trial, 260);
    Rng r(seed_);
    const unsigned bits = 3 + static_cast<unsigned>(r.below(18));
    VebTree d(bits);
    SparseVebTree s(bits);
    for (int i = 0; i < 300; ++i) {
      const auto x = pick(r, bits), q = pick(r, bits);
      if (r.coin(0.6)) { REQUIRE(d.insert(x) == s.insert(x)); } else { REQUIRE(d.erase(x) == s.erase(x)); }
      REQUIRE(d.size() == s.size());
      REQUIRE(d.successor(q) == s.successor(q));
      REQUIRE(d.predecessor(q) == s.predecessor(q));
      REQUIRE(d.min() == s.min());
      REQUIRE(d.max() == s.max());
    }
  }
}

TEST_CASE("S2.7 scale: 2^22 dense with 1M ops and 2^32 sparse with 1M ops against std::set", "[S2][scale]") {
  Rng r(9);
  {
    VebTree v(22);
    std::set<std::uint32_t> ref;
    for (int i = 0; i < 1'000'000; ++i) {
      const auto x = pick(r, 22);
      if (r.coin(0.6)) { REQUIRE(v.insert(x) == ref.insert(x).second); } else { REQUIRE(v.erase(x) == (ref.erase(x) == 1)); }
      if (i % 50000 == 0) require_same(v, ref, pick(r, 22));
    }
    v.check_invariants();
  }
  {
    SparseVebTree v(32);
    std::set<std::uint32_t> ref;
    for (int i = 0; i < 1'000'000; ++i) {
      const auto x = static_cast<std::uint32_t>(r());
      if (r.coin(0.6)) { REQUIRE(v.insert(x) == ref.insert(x).second); } else { REQUIRE(v.erase(x) == (ref.erase(x) == 1)); }
      if (i % 50000 == 0) require_same(v, ref, static_cast<std::uint32_t>(r()));
    }
    v.check_invariants();
  }
}

TEMPLATE_TEST_CASE("S2.8 copies are deep and independent; moved-from objects stay usable", "[S2][structural]", VebTree, SparseVebTree) {
  TestType a(10);
  for (std::uint32_t x : {1u, 500u, 1023u}) a.insert(x);
  TestType b = a;  // copy
  b.insert(7);
  b.erase(500);
  REQUIRE(a.contains(500));
  REQUIRE_FALSE(a.contains(7));
  REQUIRE(a.size() == 3);
  REQUIRE(b.size() == 3);
  TestType c = std::move(b);
  REQUIRE(c.contains(7));
  REQUIRE(c.successor(7) == 1023);
  a = c;  // copy assignment replaces contents
  REQUIRE(a.contains(7));
  REQUIRE_FALSE(a.contains(500));
  a.check_invariants();
  c.check_invariants();
}
