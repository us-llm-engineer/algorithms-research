#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <vector>

#include "skip_list.hpp"
#include "test_support.hpp"

using Set = algo::SkipList<std::int64_t>;

TEST_CASE("S1.1 ordered insertion and membership", "[S1][given]") {
  Set s(7);
  for (auto x : {50, 20, 70, 10, 30, 60, 80}) REQUIRE(s.insert(x));
  REQUIRE(s.to_vector() == std::vector<std::int64_t>{10,20,30,50,60,70,80});
  for (auto x : {10,20,30,50,60,70,80}) REQUIRE(s.contains(x));
  REQUIRE_FALSE(s.contains(0));
  REQUIRE(s.size() == 7);
}

TEST_CASE("S1.2 empty singleton duplicate and absent erase", "[S1][boundary]") {
  Set s(1);
  REQUIRE(s.empty()); REQUIRE_FALSE(s.erase(4));
  REQUIRE(s.insert(4)); REQUIRE_FALSE(s.insert(4)); REQUIRE(s.size() == 1);
  REQUIRE(s.erase(4)); REQUIRE(s.empty()); REQUIRE_FALSE(s.contains(4));
}

TEST_CASE("S1.3 randomized set equivalence", "[S1][stress]") {
  for (std::uint64_t trial = 0; trial < 80; ++trial) {
    ALGO_TRIAL(trial, 501); algo::Rng r(seed_); Set s(seed_); std::set<std::int64_t> ref;
    for (int i = 0; i < 500; ++i) {
      auto x = r.range(-100, 100);
      if (r.below(2) == 0) REQUIRE(s.insert(x) == ref.insert(x).second);
      else REQUIRE(s.erase(x) == (ref.erase(x) == 1));
      REQUIRE(s.size() == ref.size()); REQUIRE(s.to_vector() == std::vector<std::int64_t>(ref.begin(), ref.end()));
    }
  }
}

TEST_CASE("S1.4 sorted reverse and alternating insertion preserve order", "[S1][structural]") {
  for (int mode = 0; mode < 3; ++mode) {
    Set s(20 + mode); for (int i = 0; i < 2000; ++i) { int x = mode == 0 ? i : mode == 1 ? 1999 - i : (i % 2 ? 2000 - i/2 : i/2); s.insert(x); }
    auto v = s.to_vector(); REQUIRE(std::is_sorted(v.begin(), v.end())); REQUIRE(v.size() == 2000);
    s.check_invariants();
  }
}

TEST_CASE("S1.5 erase in random order returns to empty and permits reuse", "[S1][structural]") {
  Set s(21); std::vector<std::int64_t> v(3000); std::iota(v.begin(), v.end(), -1500); for (auto x : v) s.insert(x);
  std::mt19937_64 g(22); std::shuffle(v.begin(), v.end(), g); for (auto x : v) REQUIRE(s.erase(x));
  REQUIRE(s.empty()); REQUIRE(s.insert(9)); REQUIRE(s.contains(9));
}

TEST_CASE("S1.6 deterministic seed gives repeatable height and order", "[S1][determinism]") {
  Set a(99), b(99); for (int i = 0; i < 1000; ++i) { a.insert(i); b.insert(i); }
  REQUIRE(a.to_vector() == b.to_vector()); REQUIRE(a.height() == b.height());
}

TEST_CASE("S1.7 extreme signed keys and clear", "[S1][boundary]") {
  Set s(31); for (auto x : std::vector<std::int64_t>{INT64_MIN, -1, 0, 1, INT64_MAX}) REQUIRE(s.insert(x));
  REQUIRE(s.to_vector().front() == INT64_MIN); REQUIRE(s.to_vector().back() == INT64_MAX);
  s.clear(); REQUIRE(s.empty()); REQUIRE(s.size() == 0);
}

TEST_CASE("S1.8 large unique workload has exact cardinality", "[S1][scale]") {
  Set s(41); for (int i = 0; i < 50000; ++i) REQUIRE(s.insert(static_cast<std::int64_t>(i) * 3));
  REQUIRE(s.size() == 50000); REQUIRE(s.to_vector().front() == 0); REQUIRE(s.to_vector().back() == 149997);
}

TEST_CASE("S2.1 invariants after every mixed operation", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 40; ++trial) { ALGO_TRIAL(trial, 511); algo::Rng r(seed_); Set s(seed_);
    for (int i = 0; i < 300; ++i) { auto x = r.range(-80,80); if (r.coin(0.55)) s.insert(x); else s.erase(x); s.check_invariants(); }
  }
}

TEST_CASE("S2.2 height remains logarithmic on sorted insertion", "[S2][adversarial]") {
  Set s(52); for (int i = 0; i < 1 << 15; ++i) s.insert(i); REQUIRE(s.height() <= 128); s.check_invariants();
}

TEST_CASE("S2.3 height remains logarithmic on reverse insertion", "[S2][adversarial]") {
  Set s(53); for (int i = (1 << 14) - 1; i >= 0; --i) s.insert(i); REQUIRE(s.height() <= 128); s.check_invariants();
}

TEST_CASE("S2.4 duplicate flood does not change size", "[S2][boundary]") {
  Set s(54); for (int i = 0; i < 10000; ++i) { REQUIRE(s.insert(7) == (i == 0)); REQUIRE(s.size() == 1); } s.check_invariants();
}

TEST_CASE("S2.5 alternating insert erase leaves reference contents", "[S2][stress]") {
  Set s(55); std::set<std::int64_t> ref; for (int i = 0; i < 10000; ++i) { auto x = (i * 7919) % 1000; if (i % 3) { s.insert(x); ref.insert(x); } else { s.erase(x); ref.erase(x); } }
  REQUIRE(s.to_vector() == std::vector<std::int64_t>(ref.begin(), ref.end())); s.check_invariants();
}

TEST_CASE("S2.6 repeated clear rebuild cycles preserve correctness", "[S2][recovery]") {
  Set s(56); for (int round = 0; round < 20; ++round) { for (int i = 0; i < 1000; ++i) s.insert(i + round); REQUIRE(s.size() == 1000); s.clear(); REQUIRE(s.empty()); }
}

TEST_CASE("S2.7 node capacity does not grow after erase and reuse", "[S2][scale]") {
  Set s(57); for (int i = 0; i < 10000; ++i) s.insert(i); auto cap = s.node_capacity(); for (int i = 0; i < 10000; ++i) REQUIRE(s.erase(i)); for (int i = 0; i < 10000; ++i) s.insert(i + 20000); REQUIRE(s.node_capacity() == cap);
}

TEST_CASE("S2.8 all keys survive interleaved extrema operations", "[S2][structural]") {
  Set s(58); for (int i = 0; i < 1000; ++i) { s.insert(i); s.insert(-i); } for (int i = 0; i < 1000; ++i) { REQUIRE(s.contains(i)); REQUIRE(s.contains(-i)); } REQUIRE(s.size() == 1999); s.check_invariants();
}
