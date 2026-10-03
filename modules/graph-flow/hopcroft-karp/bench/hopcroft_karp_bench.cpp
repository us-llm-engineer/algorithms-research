// Maximum bipartite matching strategies on identical random graphs:
//   greedy (heuristic)    one pass, no augmentation: fast but only near-optimal (>= 1/2 of optimum)
//   Kuhn O(VE)            textbook augmenting-path DFS from every left vertex
//   Hopcroft-Karp O(E sqrt(V))
// `extra` records matching size and ratio = size / optimum.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "hopcroft_karp.hpp"
#include "rng.hpp"

using algo::bench::measure;
using Adj = std::vector<std::vector<int>>;

static int greedy(const Adj& g, int right) {
  std::vector<char> used(right, 0);
  int m = 0;
  for (const auto& nb : g)
    for (int v : nb) if (!used[v]) { used[v] = 1; ++m; break; }
  return m;
}

static bool kuhn_try(const Adj& g, int u, std::vector<char>& seen, std::vector<int>& match_r) {
  for (int v : g[u]) {
    if (seen[v]) continue;
    seen[v] = 1;
    if (match_r[v] < 0 || kuhn_try(g, match_r[v], seen, match_r)) { match_r[v] = u; return true; }
  }
  return false;
}

static int kuhn(const Adj& g, int right) {
  std::vector<int> match_r(right, -1);
  int m = 0;
  for (int u = 0; u < static_cast<int>(g.size()); ++u) {
    std::vector<char> seen(right, 0);
    m += kuhn_try(g, u, seen, match_r);
  }
  return m;
}

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/graph-flow/hopcroft-karp/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);

  // "random": average degree 3 (near the matching phase transition, long augmenting paths).
  // "staircase": left i adjacent to right i..i+1 listed in the worst order for greedy and Kuhn.
  for (const char* workload : {"random-deg3", "staircase"}) {
    for (int n : {1000, 4000, 16000, 64000, 128000}) {
      algo::Rng rng(n);
      Adj g(n);
      if (workload[0] == 'r') {
        for (int u = 0; u < n; ++u) for (int k = 0; k < 3; ++k) g[u].push_back(static_cast<int>(rng.below(n)));
      } else {
        for (int u = 0; u < n; ++u) { if (u + 1 < n) g[u].push_back(u + 1); g[u].push_back(u); }
      }
      const int reps = n >= 64000 ? 3 : 5;
      int opt = 0, gr = 0, ku = 0;
      auto s_hk = measure({}, [&] {
        algo::HopcroftKarp hk(n, n);
        for (int u = 0; u < n; ++u) for (int v : g[u]) hk.add_edge(u, v);
        opt = hk.maximum_matching();
        algo::bench::do_not_optimize(opt);
      }, reps);
      auto s_gr = measure({}, [&] { gr = greedy(g, n); algo::bench::do_not_optimize(gr); }, reps);
      csv.row("Hopcroft-Karp", workload, n, s_hk, "matching=" + std::to_string(opt) + ";ratio=1.000000");
      csv.row("greedy (near-optimal heuristic)", workload, n, s_gr,
              "matching=" + std::to_string(gr) + ";ratio=" + std::to_string(double(gr) / opt));
      if (n <= 64000) {
        auto s_ku = measure({}, [&] { ku = kuhn(g, n); algo::bench::do_not_optimize(ku); }, reps);
        if (ku != opt) { std::cerr << "Kuhn disagrees with Hopcroft-Karp\n"; return 3; }
        csv.row("Kuhn augmenting DFS (baseline)", workload, n, s_ku, "matching=" + std::to_string(ku) + ";ratio=1.000000");
      }
    }
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
