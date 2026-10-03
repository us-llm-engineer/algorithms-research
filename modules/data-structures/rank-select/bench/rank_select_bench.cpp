// Succinct bit-vector queries over n random bits (density 1/2), 100000 queries per measurement:
//   linear scan (brute force)   popcount from the start: O(n / 64) per rank, per select
//   rank-select Compact         two-level directory, small overhead
//   rank-select Fast            denser directory, fewer probes
//   prefix-count array          one 64-bit counter per word + binary search for select: fastest but ~100% overhead
// `extra` records directory overhead as a percentage of n.
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "rank_select.hpp"
#include "rng.hpp"

using algo::bench::measure;
constexpr std::size_t Q = 100000;

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/data-structures/rank-select/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  for (std::size_t lg : {14, 16, 18, 20, 22, 24, 26}) {
    const std::uint64_t n = std::uint64_t{1} << lg;
    algo::Rng rng(lg);
    std::vector<std::uint64_t> words(n / 64);
    for (auto& w : words) w = rng();
    algo::RankSelect compact(words, n, algo::RankSelect::Layout::Compact), fast(words, n, algo::RankSelect::Layout::Fast);
    std::vector<std::uint64_t> prefix(words.size() + 1, 0);
    for (std::size_t i = 0; i < words.size(); ++i) prefix[i + 1] = prefix[i] + std::popcount(words[i]);
    const std::uint64_t ones = prefix.back();
    std::vector<std::uint64_t> pos(Q), kth(Q);
    for (std::size_t i = 0; i < Q; ++i) { pos[i] = rng.below(n); kth[i] = rng.below(ones); }
    const int reps = 5;

    auto scan_rank = [&](std::uint64_t i) {
      std::uint64_t r = 0, w = i / 64;
      for (std::uint64_t j = 0; j < w; ++j) r += std::popcount(words[j]);
      if (i % 64) r += std::popcount(words[w] & ((std::uint64_t{1} << (i % 64)) - 1));
      return r;
    };
    auto scan_select = [&](std::uint64_t k) {
      std::uint64_t seen = 0;
      for (std::size_t j = 0; j < words.size(); ++j) {
        auto c = std::popcount(words[j]);
        if (seen + c > k) { auto w = words[j]; for (std::uint64_t t = k - seen; t > 0; --t) w &= w - 1; return j * 64 + std::countr_zero(w); }
        seen += c;
      }
      return n;
    };
    auto cell = [&](const char* name, const char* op, auto&& rank, auto&& select, const std::string& extra) {
      std::uint64_t acc = 0;
      csv.row(name, "rank", lg, measure({}, [&] { for (auto p : pos) acc += rank(p); algo::bench::do_not_optimize(acc); }, reps), extra);
      csv.row(name, "select", lg, measure({}, [&] { for (auto k : kth) acc += select(k); algo::bench::do_not_optimize(acc); }, reps), extra);
      (void)op;
    };
    auto pct = [&](std::uint64_t bits) { return "overhead_pct=" + std::to_string(100.0 * bits / n); };
    cell("rank-select Compact", "", [&](auto p) { return compact.rank1(p); }, [&](auto k) { return compact.select1(k); }, pct(compact.overhead_bits()));
    cell("rank-select Fast", "", [&](auto p) { return fast.rank1(p); }, [&](auto k) { return fast.select1(k); }, pct(fast.overhead_bits()));
    cell("prefix-count array", "", [&](auto p) { return prefix[p / 64] + (p % 64 ? std::popcount(words[p / 64] & ((std::uint64_t{1} << (p % 64)) - 1)) : 0); },
         [&](auto k) {
           std::size_t j = std::upper_bound(prefix.begin(), prefix.end(), k) - prefix.begin() - 1;
           auto w = words[j];
           for (std::uint64_t t = k - prefix[j]; t > 0; --t) w &= w - 1;
           return j * 64 + std::countr_zero(w);
         }, pct(64ull * prefix.size()));
    if (lg <= 18) cell("linear scan (brute force)", "", scan_rank, scan_select, "overhead_pct=0");
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
