// DP optimization strategies on identical inputs:
//   partition-squared: min sum of squared segment sums over k segments (k = 16)
//     brute force (recursion over every split) n <= 32, O(k n^2) exact DP, divide-and-conquer optimization O(k n log n)
//   optimal-merge: interval merging; naive O(n^3) DP vs the module (monotone-split speedup)
// All strategies must return the same optimum (checked).
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "dp_optimization.hpp"
#include "rng.hpp"

using algo::DpOptimization;
using algo::bench::measure;
using Vec = std::vector<std::int64_t>;

static std::int64_t brute(const Vec& p, std::size_t from, std::size_t k) {
  const std::size_t n = p.size() - 1;
  if (k == 1) { auto s = p[n] - p[from]; return s * s; }
  std::int64_t best = std::numeric_limits<std::int64_t>::max();
  for (std::size_t cut = from + 1; cut + (k - 1) <= n; ++cut) {
    auto s = p[cut] - p[from];
    best = std::min(best, s * s + brute(p, cut, k - 1));
  }
  return best;
}

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/optimization/dp-optimization/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  constexpr std::size_t k = 16;
  for (std::size_t n : {16u, 24u, 32u, 128u, 512u, 2048u, 8192u, 32768u}) {
    algo::Rng rng(n);
    Vec v(n);
    for (auto& x : v) x = rng.range(0, 100);
    const std::size_t segs = std::min<std::size_t>(k, n);
    std::int64_t fast = 0, slow = 0;
    auto sf = measure({}, [&] { fast = DpOptimization::partition_squared(v, segs); algo::bench::do_not_optimize(fast); }, 5);
    csv.row("divide-and-conquer DP (module)", "partition-squared", n, sf, "optimum=" + std::to_string(fast));
    if (n <= 8192) {
      auto ss = measure({}, [&] { slow = DpOptimization::partition_squared_naive(v, segs); algo::bench::do_not_optimize(slow); }, 3);
      csv.row("quadratic DP (baseline)", "partition-squared", n, ss, "optimum=" + std::to_string(slow));
      if (slow != fast) { std::cerr << "partition mismatch\n"; return 3; }
    }
    if (n <= 32) {
      Vec p(n + 1, 0);
      for (std::size_t i = 0; i < n; ++i) p[i + 1] = p[i] + v[i];
      std::int64_t b = 0;
      auto sb = measure({}, [&] { b = brute(p, 0, segs); algo::bench::do_not_optimize(b); }, 3, 0);
      csv.row("brute force (all splits)", "partition-squared", n, sb, "optimum=" + std::to_string(b));
      if (b != fast) { std::cerr << "brute mismatch\n"; return 3; }
    }
  }
  for (std::size_t n : {32u, 64u, 128u, 256u, 512u, 1024u, 2048u}) {
    algo::Rng rng(n + 99);
    Vec w(n);
    for (auto& x : w) x = rng.range(1, 1000);
    std::int64_t fast = 0, slow = 0;
    auto sf = measure({}, [&] { fast = DpOptimization::optimal_merge(w); algo::bench::do_not_optimize(fast); }, 5);
    csv.row("monotone-split DP (module)", "optimal-merge", n, sf, "optimum=" + std::to_string(fast));
    if (n <= 512) {
      auto ss = measure({}, [&] { slow = DpOptimization::optimal_merge_naive(w); algo::bench::do_not_optimize(slow); }, 3);
      csv.row("cubic DP (baseline)", "optimal-merge", n, ss, "optimum=" + std::to_string(slow));
      if (slow != fast) { std::cerr << "merge mismatch\n"; return 3; }
    }
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
