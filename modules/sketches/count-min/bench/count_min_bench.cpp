// Frequency estimation over a Zipf(1.1) stream of n items from a universe of n/4 keys:
//   exact hash map     exact counts, memory grows with the number of distinct keys
//   sort + run-length  brute force: sort the stream, count runs (exact, O(n log n))
//   Misra-Gries        deterministic heavy-hitter summary with k = 1024 counters (near-optimal, underestimates)
//   Count-Min          width 2048 x depth 4 (64 KiB), never underestimates
// `extra` records memory and ratio = mean (estimate / true count) over the 1000 most frequent keys.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "bench_harness.hpp"
#include "count_min.hpp"
#include "rng.hpp"

using algo::bench::measure;

static std::vector<std::uint64_t> zipf_stream(std::size_t n, std::size_t universe, algo::Rng& rng) {
  std::vector<double> cdf(universe);
  double sum = 0;
  for (std::size_t i = 0; i < universe; ++i) cdf[i] = (sum += 1.0 / std::pow(double(i + 1), 1.1));
  std::vector<std::uint64_t> s(n);
  for (auto& x : s) x = std::lower_bound(cdf.begin(), cdf.end(), rng.real() * sum) - cdf.begin();
  return s;
}

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/sketches/count-min/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  for (std::size_t n : {1u << 14, 1u << 16, 1u << 18, 1u << 20, 1u << 22}) {
    algo::Rng rng(n);
    const auto stream = zipf_stream(n, n / 4, rng);
    std::unordered_map<std::uint64_t, std::uint64_t> truth;
    for (auto x : stream) ++truth[x];
    std::vector<std::pair<std::uint64_t, std::uint64_t>> top(truth.begin(), truth.end());
    std::sort(top.begin(), top.end(), [](auto& a, auto& b) { return a.second > b.second; });
    top.resize(std::min<std::size_t>(1000, top.size()));
    auto mean_ratio = [&](auto&& est) {
      double r = 0;
      for (auto& [k, c] : top) r += double(est(k)) / double(c);
      return r / top.size();
    };
    const int reps = n >= (1u << 20) ? 3 : 5;

    std::unordered_map<std::uint64_t, std::uint64_t> exact;
    auto s1 = measure({}, [&] { exact.clear(); for (auto x : stream) ++exact[x]; algo::bench::do_not_optimize(exact); }, reps);
    csv.row("exact hash map", "zipf-stream", n, s1,
            "memory_kb=" + std::to_string(exact.size() * 40 / 1024) + ";ratio=" + std::to_string(mean_ratio([&](auto k) { return exact[k]; })));

    std::vector<std::uint64_t> sorted;
    auto s2 = measure({}, [&] { sorted = stream; std::sort(sorted.begin(), sorted.end()); algo::bench::do_not_optimize(sorted); }, reps);
    csv.row("sort + count runs (brute force)", "zipf-stream", n, s2,
            "memory_kb=" + std::to_string(n * 8 / 1024) + ";ratio=1.000000");

    constexpr std::size_t K = 1024;
    std::unordered_map<std::uint64_t, std::uint64_t> mg;
    auto s3 = measure({}, [&] {
      mg.clear();
      for (auto x : stream) {
        auto it = mg.find(x);
        if (it != mg.end()) ++it->second;
        else if (mg.size() < K) mg[x] = 1;
        else for (auto i = mg.begin(); i != mg.end();) { if (--i->second == 0) i = mg.erase(i); else ++i; }
      }
      algo::bench::do_not_optimize(mg);
    }, reps);
    csv.row("Misra-Gries k=1024 (heuristic)", "zipf-stream", n, s3,
            "memory_kb=" + std::to_string(K * 16 / 1024) + ";ratio=" + std::to_string(mean_ratio([&](auto k) { auto it = mg.find(k); return it == mg.end() ? 0 : it->second; })));

    algo::CountMinSketch<std::uint64_t> cm(2048, 4, 7);
    auto s4 = measure({}, [&] { cm.clear(); for (auto x : stream) cm.add(x); algo::bench::do_not_optimize(cm); }, reps);
    csv.row("Count-Min 2048x4", "zipf-stream", n, s4,
            "memory_kb=" + std::to_string(2048 * 4 * 8 / 1024) + ";ratio=" + std::to_string(mean_ratio([&](auto k) { return cm.estimate(k); })));
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
