// Seeded pseudo-random generators. All tests and benchmarks use these so that
// every run is reproducible from a single 64-bit seed.
#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace algo {

__extension__ typedef unsigned __int128 u128;  // GCC/Clang extension; used for overflow-free 64x64 products

// SplitMix64 (Steele, Lea, Flood 2014): a bijective 64-bit mixer. Also used as a hash finalizer.
constexpr std::uint64_t splitmix64(std::uint64_t& state) noexcept {
  std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

constexpr std::uint64_t mix64(std::uint64_t x) noexcept { return splitmix64(x); }

// xoshiro256** (Blackman, Vigna 2018): fast, 256-bit state, passes BigCrush.
class Rng {
 public:
  using result_type = std::uint64_t;
  explicit Rng(std::uint64_t seed = 1) noexcept {
    std::uint64_t sm = seed;
    for (auto& w : s_) w = splitmix64(sm);
  }
  static constexpr result_type min() noexcept { return 0; }
  static constexpr result_type max() noexcept { return ~0ULL; }

  result_type operator()() noexcept {
    const std::uint64_t result = rotl(s_[1] * 5, 7) * 9;
    const std::uint64_t t = s_[1] << 17;
    s_[2] ^= s_[0];
    s_[3] ^= s_[1];
    s_[1] ^= s_[2];
    s_[0] ^= s_[3];
    s_[2] ^= t;
    s_[3] = rotl(s_[3], 45);
    return result;
  }

  // Unbiased integer in [0, n) (Lemire's multiply-and-reject). Requires n > 0.
  std::uint64_t below(std::uint64_t n) noexcept {
    u128 m = static_cast<u128>((*this)()) * n;
    auto low = static_cast<std::uint64_t>(m);
    if (low < n) {
      const std::uint64_t threshold = (0 - n) % n;
      while (low < threshold) {
        m = static_cast<u128>((*this)()) * n;
        low = static_cast<std::uint64_t>(m);
      }
    }
    return static_cast<std::uint64_t>(m >> 64);
  }

  // Uniform integer in the closed interval [lo, hi].
  std::int64_t range(std::int64_t lo, std::int64_t hi) noexcept {
    return lo + static_cast<std::int64_t>(below(static_cast<std::uint64_t>(hi - lo) + 1));
  }

  // Uniform double in [0, 1).
  double real() noexcept { return static_cast<double>((*this)() >> 11) * 0x1.0p-53; }

  bool coin(double p = 0.5) noexcept { return real() < p; }

  template <class It>
  void shuffle(It first, It last) {
    const auto n = static_cast<std::uint64_t>(last - first);
    for (std::uint64_t i = n; i > 1; --i) std::iter_swap(first + (i - 1), first + below(i));
  }

  // A uniformly random permutation of 0..n-1.
  std::vector<int> permutation(int n) {
    std::vector<int> p(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) p[static_cast<std::size_t>(i)] = i;
    shuffle(p.begin(), p.end());
    return p;
  }

 private:
  static constexpr std::uint64_t rotl(std::uint64_t x, int k) noexcept { return (x << k) | (x >> (64 - k)); }
  std::uint64_t s_[4];
};

}  // namespace algo
