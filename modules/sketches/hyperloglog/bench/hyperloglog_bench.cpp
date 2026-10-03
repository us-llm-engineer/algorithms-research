// Distinct counting over a stream of n items with n/2 distinct values:
//   sort + unique       brute force, exact, O(n log n), O(n) memory
//   hash set            exact, expected O(n), O(distinct) memory
//   linear counting     bitmap of 2^16 bits: near-optimal while the bitmap is not saturated
//   HyperLogLog p=10 / p=14   registers = 2^p bytes, standard error ~ 1.04/sqrt(2^p)
// `extra` records memory and ratio = estimate / true cardinality.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "bench_harness.hpp"
#include "hyperloglog.hpp"
#include "rng.hpp"

using algo::bench::measure;

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/sketches/hyperloglog/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  for (std::size_t n : {1u << 12, 1u << 14, 1u << 16, 1u << 18, 1u << 20, 1u << 22}) {
    std::vector<std::uint64_t> stream(n);
    for (std::size_t i = 0; i < n; ++i) stream[i] = algo::mix64(i % (n / 2));
    algo::Rng rng(n);
    rng.shuffle(stream.begin(), stream.end());
    const double truth = double(n / 2);
    const int reps = n >= (1u << 20) ? 3 : 5;

    double est = 0;
    std::vector<std::uint64_t> tmp;
    auto s1 = measure({}, [&] { tmp = stream; std::sort(tmp.begin(), tmp.end()); est = double(std::unique(tmp.begin(), tmp.end()) - tmp.begin()); algo::bench::do_not_optimize(est); }, reps);
    csv.row("sort + unique (brute force)", "distinct-count", n, s1, "memory_kb=" + std::to_string(n * 8 / 1024) + ";ratio=" + std::to_string(est / truth));
    auto s2 = measure({}, [&] { std::unordered_set<std::uint64_t> h(stream.begin(), stream.end()); est = double(h.size()); algo::bench::do_not_optimize(est); }, reps);
    csv.row("hash set (exact)", "distinct-count", n, s2, "memory_kb=" + std::to_string(n / 2 * 40 / 1024) + ";ratio=" + std::to_string(est / truth));

    constexpr std::size_t M = 1u << 16;
    auto s3 = measure({}, [&] {
      std::vector<bool> bits(M, false);
      for (auto x : stream) bits[x % M] = true;
      std::size_t zeros = std::count(bits.begin(), bits.end(), false);
      est = zeros ? -double(M) * std::log(double(zeros) / M) : double(M) * std::log(double(M));  // saturated: capped
      algo::bench::do_not_optimize(est);
    }, reps);
    csv.row("linear counting 64 Kbit (heuristic)", "distinct-count", n, s3, "memory_kb=8;ratio=" + std::to_string(est / truth));

    for (std::uint8_t p : {10, 14}) {
      auto s = measure({}, [&] { algo::HyperLogLog h(p); for (auto x : stream) h.add(x); est = h.estimate(); algo::bench::do_not_optimize(est); }, reps);
      csv.row("HyperLogLog p=" + std::to_string(p), "distinct-count", n, s,
              "memory_kb=" + std::to_string((1u << p) / 1024.0) + ";ratio=" + std::to_string(est / truth));
    }
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
