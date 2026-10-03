#include <algorithm>
#include <numeric>
#include <set>
#include <sstream>

#include "bench_harness.hpp"
#include "test_support.hpp"

using algo::Rng;

TEST_CASE("Rng is deterministic for a given seed and differs across seeds", "[given][common]") {
  Rng a(42), b(42), c(43);
  bool differs = false;
  for (int i = 0; i < 1000; ++i) {
    const auto x = a();
    REQUIRE(x == b());
    differs |= (x != c());
  }
  REQUIRE(differs);
}

TEST_CASE("Rng::below is in range and unbiased on a non-power-of-two modulus", "[boundary][stress][common]") {
  Rng r(7);
  REQUIRE(r.below(1) == 0);
  constexpr int kBuckets = 6, kDraws = 600000;
  int count[kBuckets] = {};
  for (int i = 0; i < kDraws; ++i) {
    const auto v = r.below(kBuckets);
    REQUIRE(v < kBuckets);
    ++count[v];
  }
  // Each bucket ~ Binomial(600000, 1/6): sd ~ 289; allow 6 sd.
  for (int c : count) REQUIRE(std::abs(c - kDraws / kBuckets) < 6 * 289);
}

TEST_CASE("Rng::range covers both endpoints and handles negative bounds", "[boundary][common]") {
  Rng r(9);
  std::set<std::int64_t> seen;
  for (int i = 0; i < 2000; ++i) seen.insert(r.range(-2, 2));
  REQUIRE(seen == std::set<std::int64_t>{-2, -1, 0, 1, 2});
  REQUIRE(r.range(5, 5) == 5);
}

TEST_CASE("Rng::permutation is a permutation", "[structural][common]") {
  Rng r(11);
  for (int n : {0, 1, 2, 17, 1000}) {
    auto p = r.permutation(n);
    std::sort(p.begin(), p.end());
    std::vector<int> id(static_cast<std::size_t>(n));
    std::iota(id.begin(), id.end(), 0);
    REQUIRE(p == id);
  }
}

TEST_CASE("trial_seed gives distinct seeds per trial and per salt", "[structural][common]") {
  std::set<std::uint64_t> seeds;
  for (std::uint64_t t = 0; t < 100; ++t)
    for (std::uint64_t s = 0; s < 5; ++s) seeds.insert(algo::testing::trial_seed(t, s));
  REQUIRE(seeds.size() == 500);
}

TEST_CASE("bench::measure runs setup+body the requested number of times and orders statistics", "[given][common]") {
  int setups = 0, bodies = 0;
  const auto s = algo::bench::measure([&] { ++setups; }, [&] { ++bodies; }, 5, 2);
  REQUIRE(setups == 7);
  REQUIRE(bodies == 7);
  REQUIRE(s.reps == 5);
  REQUIRE(s.min_ms <= s.median_ms);
  REQUIRE(s.median_ms <= s.max_ms);
}

TEST_CASE("bench::Csv writes a header and well-formed rows", "[given][common]") {
  std::ostringstream os;
  algo::bench::Csv csv(os);
  algo::bench::Stats st;
  st.median_ms = 1.5;
  st.min_ms = 1.0;
  st.max_ms = 2.0;
  st.reps = 3;
  csv.row("alg", "random", 1000, st, "ops=5");
  const std::string out = os.str();
  REQUIRE(out.find("algorithm,workload,n,median_ms") == 0);
  REQUIRE(out.find("alg,random,1000,1.500000,1.000000,2.000000,3,ops=5\n") != std::string::npos);
}
