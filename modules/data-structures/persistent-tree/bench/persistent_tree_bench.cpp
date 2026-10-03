// Versioned ordered map. n updates, every update yields a new queryable version.
//   full copy per version (brute force)   std::map copied on each update: O(n) time and space per version
//   persistent map (module)               path copying: O(log n) new nodes per version
//   ephemeral std::map                    lower bound only: keeps no history, cannot answer old-version queries
// Workloads: build (n updates), query-old (n lookups against uniformly random past versions).
// `extra` records heap nodes allocated for history.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "persistent_tree.hpp"
#include "rng.hpp"

using algo::bench::measure;

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/data-structures/persistent-tree/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  for (std::size_t n : {256u, 512u, 1024u, 2048u, 4096u, 16384u, 65536u, 262144u}) {
    algo::Rng rng(n);
    std::vector<std::uint64_t> keys(n), probe(n), which(n);
    for (std::size_t i = 0; i < n; ++i) { keys[i] = rng.below(~0ULL >> 1); probe[i] = keys[rng.below(n)]; which[i] = rng.below(n); }
    const int reps = n >= 65536 ? 3 : 5;

    algo::PersistentMap<std::uint64_t, std::uint64_t> pm;
    std::vector<algo::PersistentMap<std::uint64_t, std::uint64_t>::Version> vers;
    auto sb = measure({}, [&] {
      pm = {};
      vers.assign(1, pm.empty_version());
      for (auto k : keys) vers.push_back(pm.insert(vers.back(), k, k));
      algo::bench::do_not_optimize(vers);
    }, reps);
    csv.row("persistent map (module)", "build-versions", n, sb, "nodes=" + std::to_string(pm.node_count()));
    auto sq = measure({}, [&] {
      std::size_t hits = 0;
      for (std::size_t i = 0; i < n; ++i) hits += pm.contains(vers[1 + which[i]], probe[i]);
      algo::bench::do_not_optimize(hits);
    }, reps);
    csv.row("persistent map (module)", "query-old-versions", n, sq, "nodes=" + std::to_string(pm.node_count()));

    if (n <= 4096) {
      std::vector<std::map<std::uint64_t, std::uint64_t>> copies;
      auto sc = measure({}, [&] {
        copies.clear();
        copies.reserve(n);
        std::map<std::uint64_t, std::uint64_t> cur;
        for (auto k : keys) { cur[k] = k; copies.push_back(cur); }
        algo::bench::do_not_optimize(copies);
      }, reps);
      csv.row("full copy per version (brute force)", "build-versions", n, sc, "nodes=" + std::to_string(n * (n + 1) / 2));
      auto sq2 = measure({}, [&] {
        std::size_t hits = 0;
        for (std::size_t i = 0; i < n; ++i) hits += copies[which[i]].count(probe[i]);
        algo::bench::do_not_optimize(hits);
      }, reps);
      csv.row("full copy per version (brute force)", "query-old-versions", n, sq2, "nodes=" + std::to_string(n * (n + 1) / 2));
    }
    auto se = measure({}, [&] {
      std::map<std::uint64_t, std::uint64_t> cur;
      for (auto k : keys) cur[k] = k;
      algo::bench::do_not_optimize(cur);
    }, reps);
    csv.row("ephemeral std::map (no history)", "build-versions", n, se, "nodes=" + std::to_string(n));
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
