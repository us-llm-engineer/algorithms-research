// Hard tests for algo::RankSelect: an immutable bit vector with O(1) rank and fast select on top of
// o(n)-style auxiliary directories. Two layouts: Compact (small overhead) and Fast (rank9-style).
//   [S1] exact agreement with naive prefix sums on boundaries, padding, densities, both layouts
//   [S2] space, probe-count (time) evidence, adversarial distributions, scale
#include <algorithm>
#include <catch2/catch_template_test_macros.hpp>
#include <cstdint>
#include <vector>

#include "rank_select.hpp"
#include "test_support.hpp"

using algo::RankSelect;
using Layout = RankSelect::Layout;
using algo::Rng;

namespace {
// Naive reference: bit array, prefix counts and the positions of ones/zeros.
struct Naive {
  std::vector<bool> bits;
  std::vector<std::uint64_t> pre;  // pre[i] = ones in [0,i)
  std::vector<std::uint64_t> ones, zeros;
  explicit Naive(std::vector<bool> b) : bits(std::move(b)), pre(bits.size() + 1, 0) {
    for (std::size_t i = 0; i < bits.size(); ++i) {
      pre[i + 1] = pre[i] + (bits[i] ? 1 : 0);
      (bits[i] ? ones : zeros).push_back(i);
    }
  }
};

std::vector<std::uint64_t> pack(const std::vector<bool>& bits, bool garbage_padding = false) {
  std::vector<std::uint64_t> w((bits.size() + 63) / 64, 0);
  for (std::size_t i = 0; i < bits.size(); ++i) if (bits[i]) w[i / 64] |= 1ULL << (i % 64);
  if (garbage_padding && bits.size() % 64) w.back() |= ~0ULL << (bits.size() % 64);  // junk above nbits must be ignored
  return w;
}

std::vector<bool> random_bits(Rng& r, std::size_t n, double density) {
  std::vector<bool> b(n);
  for (std::size_t i = 0; i < n; ++i) b[i] = r.coin(density);
  return b;
}

void check_all(const RankSelect& rs, const Naive& ref) {
  const std::uint64_t n = ref.bits.size();
  REQUIRE(rs.size() == n);
  REQUIRE(rs.ones() == ref.ones.size());
  for (std::uint64_t i = 0; i <= n; ++i) {
    REQUIRE(rs.rank1(i) == ref.pre[i]);
    REQUIRE(rs.rank0(i) == i - ref.pre[i]);
  }
  for (std::uint64_t i = 0; i < n; ++i) REQUIRE(rs.access(i) == ref.bits[i]);
  for (std::uint64_t k = 0; k < ref.ones.size(); ++k) REQUIRE(rs.select1(k) == ref.ones[k]);
  for (std::uint64_t k = 0; k < ref.zeros.size(); ++k) REQUIRE(rs.select0(k) == ref.zeros[k]);
}
}  // namespace

// ------------------------------------------------------------------ S1

TEMPLATE_TEST_CASE_SIG("S1.1 hand-computed example", "[S1][given]", ((Layout L), L), Layout::Compact, Layout::Fast) {
  // bits: 1 0 1 1 0 0 1 0 1 0   (positions of ones: 0 2 3 6 8)
  const std::vector<bool> b{1, 0, 1, 1, 0, 0, 1, 0, 1, 0};
  RankSelect rs(b, L);
  REQUIRE(rs.rank1(0) == 0);
  REQUIRE(rs.rank1(4) == 3);
  REQUIRE(rs.rank1(10) == 5);
  REQUIRE(rs.select1(0) == 0);
  REQUIRE(rs.select1(3) == 6);
  REQUIRE(rs.select1(4) == 8);
  REQUIRE(rs.select0(0) == 1);
  REQUIRE(rs.select0(4) == 9);
  REQUIRE(rs.ones() == 5);
}

TEMPLATE_TEST_CASE_SIG("S1.2 empty and tiny vectors", "[S1][boundary]", ((Layout L), L), Layout::Compact, Layout::Fast) {
  RankSelect e(std::vector<bool>{}, L);
  REQUIRE(e.size() == 0);
  REQUIRE(e.ones() == 0);
  REQUIRE(e.rank1(0) == 0);
  REQUIRE(e.rank0(0) == 0);
  REQUIRE_THROWS_AS(e.access(0), std::out_of_range);
  REQUIRE_THROWS_AS(e.select1(0), std::out_of_range);
  REQUIRE_THROWS_AS(e.select0(0), std::out_of_range);
  REQUIRE_THROWS_AS(e.rank1(1), std::out_of_range);
  for (bool bit : {false, true}) {
    const std::vector<bool> one{bit};
    RankSelect rs(one, L);
    check_all(rs, Naive(one));
  }
}

TEMPLATE_TEST_CASE_SIG("S1.3 every length around word/block/superblock boundaries and four fill patterns", "[S1][boundary]", ((Layout L), L), Layout::Compact, Layout::Fast) {
  std::vector<std::size_t> lengths;
  for (std::size_t base : {64u, 128u, 512u, 1024u, 4096u, 8192u})
    for (long d = -2; d <= 2; ++d) if (static_cast<long>(base) + d > 0) lengths.push_back(static_cast<std::size_t>(static_cast<long>(base) + d));
  for (std::size_t n : lengths) {
    for (int pattern = 0; pattern < 4; ++pattern) {
      std::vector<bool> b(n);
      for (std::size_t i = 0; i < n; ++i) {
        switch (pattern) {
          case 0: b[i] = false; break;
          case 1: b[i] = true; break;
          case 2: b[i] = (i % 2 == 0); break;
          default: b[i] = (i == 0 || i == n - 1 || i % 64 == 63 || i % 512 == 0);  // ones exactly on boundaries
        }
      }
      INFO("n=" << n << " pattern=" << pattern);
      check_all(RankSelect(b, L), Naive(b));
    }
  }
}

TEMPLATE_TEST_CASE_SIG("S1.4 bits above nbits in the last word are ignored (padding must not leak into rank/select/ones)", "[S1][structural]", ((Layout L), L), Layout::Compact, Layout::Fast) {
  for (std::size_t n : {1u, 7u, 63u, 65u, 70u, 127u, 129u, 511u, 513u, 1000u}) {
    Rng r(n);
    const auto b = random_bits(r, n, 0.5);
    const auto words = pack(b, /*garbage_padding=*/true);
    RankSelect rs(words, n, L);
    INFO("n=" << n);
    check_all(rs, Naive(b));
    REQUIRE_THROWS_AS(rs.select1(rs.ones()), std::out_of_range);
    REQUIRE_THROWS_AS(rs.select0(rs.size() - rs.ones()), std::out_of_range);
  }
  // all-ones words but only 70 bits are real
  const std::vector<std::uint64_t> all_ones{~0ULL, ~0ULL};
  RankSelect rs(all_ones, 70, L);
  REQUIRE(rs.ones() == 70);
  REQUIRE(rs.rank1(70) == 70);
  REQUIRE_THROWS_AS(rs.select1(70), std::out_of_range);
  REQUIRE_THROWS_AS(rs.select0(0), std::out_of_range);
}

TEMPLATE_TEST_CASE_SIG("S1.5 random vectors of many densities (sparse to dense), 40 seeds each", "[S1][stress]", ((Layout L), L), Layout::Compact, Layout::Fast) {
  for (double density : {0.0005, 0.01, 0.1, 0.5, 0.9, 0.99, 0.9995})
    for (std::uint64_t trial = 0; trial < 40; ++trial) {
      ALGO_TRIAL(trial, static_cast<std::uint64_t>(density * 1e6));
      Rng r(seed_);
      const std::size_t n = 1 + r.below(20000);
      const auto b = random_bits(r, n, density);
      check_all(RankSelect(b, L), Naive(b));
    }
}

TEMPLATE_TEST_CASE_SIG("S1.6 rank and select are inverse; rank0 + rank1 = i; monotone", "[S1][structural]", ((Layout L), L), Layout::Compact, Layout::Fast) {
  Rng r(6);
  const auto b = random_bits(r, 100000, 0.3);
  RankSelect rs(b, L);
  for (std::uint64_t k = 0; k < rs.ones(); k += 7) {
    REQUIRE(rs.rank1(rs.select1(k)) == k);
    REQUIRE(rs.access(rs.select1(k)));
    REQUIRE(rs.rank1(rs.select1(k) + 1) == k + 1);
  }
  for (std::uint64_t k = 0; k < rs.size() - rs.ones(); k += 7) {
    REQUIRE(rs.rank0(rs.select0(k)) == k);
    REQUIRE_FALSE(rs.access(rs.select0(k)));
  }
  std::uint64_t prev = 0;
  for (std::uint64_t i = 0; i <= rs.size(); i += 13) {
    REQUIRE(rs.rank1(i) + rs.rank0(i) == i);
    REQUIRE(rs.rank1(i) >= prev);
    prev = rs.rank1(i);
  }
}

TEST_CASE("S1.7 both layouts and both constructors give identical answers", "[S1][structural]") {
  for (std::uint64_t trial = 0; trial < 30; ++trial) {
    ALGO_TRIAL(trial, 77);
    Rng r(seed_);
    const std::size_t n = 1 + r.below(30000);
    const auto b = random_bits(r, n, r.real());
    const auto words = pack(b);
    RankSelect a(b, Layout::Compact), c(b, Layout::Fast), d(words, n, Layout::Compact);
    for (int q = 0; q < 300; ++q) {
      const auto i = r.below(n + 1);
      REQUIRE(a.rank1(i) == c.rank1(i));
      REQUIRE(a.rank1(i) == d.rank1(i));
      if (a.ones()) { const auto k = r.below(a.ones()); REQUIRE(a.select1(k) == c.select1(k)); REQUIRE(a.select1(k) == d.select1(k)); }
      if (n - a.ones()) { const auto k = r.below(n - a.ones()); REQUIRE(a.select0(k) == c.select0(k)); }
    }
  }
}

TEMPLATE_TEST_CASE_SIG("S1.8 range errors leave the structure usable", "[S1][boundary]", ((Layout L), L), Layout::Compact, Layout::Fast) {
  const std::vector<bool> b(64, true);
  RankSelect rs(b, L);
  REQUIRE(rs.rank1(64) == 64);  // i == n is legal even when n is a multiple of 64
  REQUIRE_THROWS_AS(rs.rank1(65), std::out_of_range);
  REQUIRE_THROWS_AS(rs.rank0(65), std::out_of_range);
  REQUIRE_THROWS_AS(rs.access(64), std::out_of_range);
  REQUIRE_THROWS_AS(rs.select1(64), std::out_of_range);
  REQUIRE_THROWS_AS(rs.select0(0), std::out_of_range);
  REQUIRE(rs.select1(63) == 63);
  REQUIRE(rs.rank1(32) == 32);
}

// ------------------------------------------------------------------ S2

TEST_CASE("S2.1 space: raw bits + auxiliary overhead; Compact <= 10% and Fast <= 40% for n >= 2^16", "[S2][invariant]") {
  Rng r(1);
  for (int lg = 10; lg <= 24; lg += 2) {
    const std::size_t n = 1ULL << lg;
    const auto b = random_bits(r, n, 0.5);
    RankSelect c(b, Layout::Compact), f(b, Layout::Fast);
    const double oc = static_cast<double>(c.overhead_bits()) / static_cast<double>(n);
    const double of = static_cast<double>(f.overhead_bits()) / static_cast<double>(n);
    INFO("lg n=" << lg << " compact overhead=" << oc << " fast overhead=" << of);
    REQUIRE(c.total_bits() >= n);
    REQUIRE(c.total_bits() == ((n + 63) / 64) * 64 + c.overhead_bits());
    if (lg >= 16) { REQUIRE(oc <= 0.10); REQUIRE(of <= 0.40); }
    REQUIRE(oc < of);  // the compact layout really is the smaller one
  }
}

TEST_CASE("S2.2 rank touches O(1) words independent of n (Compact and Fast)", "[S2][invariant]") {
  Rng r(2);
  auto worst = [&](int lg, Layout layout) {
    const std::size_t n = 1ULL << lg;
    const auto b = random_bits(r, n, 0.5);
    RankSelect rs(b, layout);
    std::uint64_t w = 0;
    for (int q = 0; q < 3000; ++q) { rs.rank1(r.below(n + 1)); w = std::max(w, rs.last_probes()); }
    return w;
  };
  for (Layout layout : {Layout::Compact, Layout::Fast}) {
    const auto small = worst(10, layout), large = worst(24, layout);
    INFO("worst probes: n=2^10 -> " << small << ", n=2^24 -> " << large);
    REQUIRE(large <= 12);
    REQUIRE(large <= small + 2);  // no growth with n
  }
}

TEST_CASE("S2.3 select probe count stays logarithmic on clustered, gappy and extreme distributions", "[S2][adversarial]") {
  const std::size_t n = 1 << 22;
  Rng r(3);
  std::vector<std::vector<bool>> shapes(4, std::vector<bool>(n, false));
  for (std::size_t i = 0; i < n / 100; ++i) shapes[0][i] = true;                     // all ones in the first 1%
  for (std::size_t i = n - n / 100; i < n; ++i) shapes[1][i] = true;                  // ... in the last 1%
  for (std::size_t i = 0; i < n; i += 1 << 20) shapes[2][i] = true;                   // gaps of 10^6 zeros between ones
  for (std::size_t i = 0; i < n; ++i) shapes[3][i] = (i / 100000) % 2 == 0 ? r.coin(0.9) : r.coin(0.0001);  // dense/sparse bands
  for (const auto& b : shapes)
    for (Layout layout : {Layout::Compact, Layout::Fast}) {
      RankSelect rs(b, layout);
      Naive ref(b);
      std::uint64_t worst = 0;
      for (int q = 0; q < 2000 && !ref.ones.empty(); ++q) {
        const auto k = r.below(ref.ones.size());
        REQUIRE(rs.select1(k) == ref.ones[k]);
        worst = std::max(worst, rs.last_probes());
      }
      for (int q = 0; q < 2000 && !ref.zeros.empty(); ++q) {
        const auto k = r.below(ref.zeros.size());
        REQUIRE(rs.select0(k) == ref.zeros[k]);
        worst = std::max(worst, rs.last_probes());
      }
      INFO("worst select probes " << worst);
      REQUIRE(worst <= 200);  // O(log n) probes + O(1) words, never a linear scan of a gap
    }
}

TEST_CASE("S2.4 ones/zeros placed on directory boundaries (63|64, 511|512, 4095|4096) in every combination", "[S2][adversarial]") {
  const std::size_t n = 3 * 4096 + 17;
  for (std::size_t period : {64u, 512u, 4096u})
    for (long shift : {-1L, 0L, 1L})
      for (bool invert : {false, true}) {
        std::vector<bool> b(n, invert);
        for (std::size_t i = 0; i < n; ++i) {
          const long pos = static_cast<long>(i) - shift;
          if (pos >= 0 && static_cast<std::size_t>(pos) % period == 0) b[i] = !invert;
        }
        for (Layout layout : {Layout::Compact, Layout::Fast}) {
          INFO("period=" << period << " shift=" << shift << " invert=" << invert);
          check_all(RankSelect(b, layout), Naive(b));
        }
      }
}

TEST_CASE("S2.5 scale: n = 2^24, 10^6 random queries of each kind, both layouts", "[S2][scale]") {
  Rng r(5);
  const std::size_t n = 1ULL << 24;
  const auto b = random_bits(r, n, 0.37);
  Naive ref(b);
  for (Layout layout : {Layout::Compact, Layout::Fast}) {
    RankSelect rs(b, layout);
    REQUIRE(rs.ones() == ref.ones.size());
    for (int q = 0; q < 1'000'000; ++q) {
      const auto i = r.below(n + 1);
      REQUIRE(rs.rank1(i) == ref.pre[i]);
      const auto k = r.below(ref.ones.size());
      REQUIRE(rs.select1(k) == ref.ones[k]);
      if (q % 4 == 0) { const auto z = r.below(ref.zeros.size()); REQUIRE(rs.select0(z) == ref.zeros[z]); }
      if (q % 4 == 1) { const auto a = r.below(n); REQUIRE(rs.access(a) == ref.bits[a]); }
    }
  }
}

TEST_CASE("S2.6 degenerate extremes: all ones and all zeros at 2^20", "[S2][structural]") {
  const std::size_t n = 1 << 20;
  for (Layout layout : {Layout::Compact, Layout::Fast}) {
    RankSelect ones(std::vector<bool>(n, true), layout), zeros(std::vector<bool>(n, false), layout);
    Rng r(6);
    for (int q = 0; q < 20000; ++q) {
      const auto i = r.below(n + 1), k = r.below(n);
      REQUIRE(ones.rank1(i) == i);
      REQUIRE(ones.select1(k) == k);
      REQUIRE(zeros.rank1(i) == 0);
      REQUIRE(zeros.rank0(i) == i);
      REQUIRE(zeros.select0(k) == k);
    }
    REQUIRE_THROWS_AS(ones.select0(0), std::out_of_range);
    REQUIRE_THROWS_AS(zeros.select1(0), std::out_of_range);
  }
}

TEST_CASE("S2.7 construction copies its input; copies and moves are independent", "[S2][structural]") {
  Rng r(7);
  const auto b = random_bits(r, 5000, 0.5);
  auto words = pack(b);
  RankSelect rs(words, 5000, Layout::Compact);
  const auto r1 = rs.rank1(4999);
  std::fill(words.begin(), words.end(), 0ULL);  // mutate the caller's buffer afterwards
  REQUIRE(rs.rank1(4999) == r1);
  RankSelect copy = rs;
  RankSelect moved = std::move(copy);
  REQUIRE(moved.rank1(4999) == r1);
  REQUIRE(rs.select1(0) == moved.select1(0));
}

TEST_CASE("S2.8 directory consistency check recomputes every counter from the raw bits", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 40; ++trial) {
    ALGO_TRIAL(trial, 88);
    Rng r(seed_);
    const std::size_t n = 1 + r.below(100000);
    const auto b = random_bits(r, n, r.real());
    RankSelect(b, Layout::Compact).check_invariants();
    RankSelect(b, Layout::Fast).check_invariants();
  }
}
