// Max-flow strategies on identical networks: Edmonds-Karp O(VE^2) (textbook baseline), Dinic O(V^2 E),
// FIFO push-relabel O(V^3) and highest-label push-relabel O(V^2 sqrt(E)) with gap + global relabeling.
// All strategies must return the same flow value (checked); `extra` records it.
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "push_relabel.hpp"
#include "rng.hpp"

using algo::MaxFlow;
using algo::bench::measure;

struct Net { int n; std::vector<std::array<std::int64_t, 3>> edges; int s, t; };

static Net random_sparse(int n, std::uint64_t seed) {
  algo::Rng rng(seed);
  Net g{n, {}, 0, n - 1};
  for (int v = 0; v + 1 < n; ++v) g.edges.push_back({v, v + 1, rng.range(1, 100)});  // keep s-t connected
  for (int i = 0; i < 4 * n; ++i) {
    int u = static_cast<int>(rng.below(n)), v = static_cast<int>(rng.below(n));
    if (u != v) g.edges.push_back({u, v, rng.range(1, 100)});
  }
  return g;
}

// k x k grid, edges right/down with random capacities; source top-left, sink bottom-right.
static Net grid(int k, std::uint64_t seed) {
  algo::Rng rng(seed);
  Net g{k * k, {}, 0, k * k - 1};
  for (int r = 0; r < k; ++r)
    for (int c = 0; c < k; ++c) {
      if (c + 1 < k) g.edges.push_back({r * k + c, r * k + c + 1, rng.range(1, 50)});
      if (r + 1 < k) g.edges.push_back({r * k + c, (r + 1) * k + c, rng.range(1, 50)});
    }
  return g;
}

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/graph-flow/push-relabel/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);

  struct Strat { const char* name; MaxFlow::Algorithm a; };
  const Strat strategies[] = {{"Edmonds-Karp (baseline)", MaxFlow::Algorithm::EdmondsKarp},
                              {"Dinic", MaxFlow::Algorithm::Dinic},
                              {"push-relabel FIFO", MaxFlow::Algorithm::FifoPushRelabel},
                              {"push-relabel highest-label", MaxFlow::Algorithm::HighestLabelPushRelabel}};
  for (const char* workload : {"random-sparse", "grid"}) {
    const bool is_grid = workload[0] == 'g';
    for (int size : is_grid ? std::vector<int>{8, 16, 32, 48, 64} : std::vector<int>{200, 800, 3200, 12800, 25600}) {
      const Net net = is_grid ? grid(size, size) : random_sparse(size, size);
      const std::uint64_t n = net.n;
      std::int64_t reference = -1;
      for (const auto& st : strategies) {
        std::int64_t value = 0;
        auto stats = measure({}, [&] {
          MaxFlow g(net.n);
          for (auto& e : net.edges) g.add_edge(static_cast<int>(e[0]), static_cast<int>(e[1]), e[2]);
          value = g.max_flow(net.s, net.t, st.a);
          algo::bench::do_not_optimize(value);
        }, n >= 10000 ? 3 : 5, 1);
        if (reference < 0) reference = value;
        if (value != reference) { std::cerr << "flow mismatch for " << st.name << '\n'; return 3; }
        csv.row(st.name, workload, n, stats, "flow=" + std::to_string(value));
      }
    }
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
