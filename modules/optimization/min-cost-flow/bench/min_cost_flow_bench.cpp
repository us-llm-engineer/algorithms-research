// Min-cost flow on identical random networks (capacities 1..20, costs 1..100, sparse):
//   Bellman-Ford SSP    successive shortest paths, Bellman-Ford every augmentation (O(F V E))
//   SPFA SSP            queue-based Bellman-Ford (practical baseline)
//   module              Dijkstra with Johnson potentials
//   greedy cheapest-edge heuristic: repeatedly pushes along the Dijkstra path on ORIGINAL costs ignoring
//                       residual reverse arcs (never undoes a choice) - fast but not optimal (ratio > 1).
// `extra` records flow, cost and ratio = cost / optimal cost at equal flow.
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "min_cost_flow.hpp"
#include "rng.hpp"

using algo::bench::measure;
using V = std::int64_t;
constexpr V INF = std::numeric_limits<V>::max() / 4;

struct Edge { int u, v; V cap, cost; };
struct Arc { int to, rev; V cap, cost; };

static std::vector<std::vector<Arc>> build(int n, const std::vector<Edge>& es) {
  std::vector<std::vector<Arc>> g(n);
  for (auto& e : es) {
    g[e.u].push_back({e.v, (int)g[e.v].size(), e.cap, e.cost});
    g[e.v].push_back({e.u, (int)g[e.u].size() - 1, 0, -e.cost});
  }
  return g;
}

// mode 0: Bellman-Ford rounds, 1: SPFA, 2: Dijkstra on non-negative original costs, reverse arcs forbidden.
static std::pair<V, V> ssp(int n, const std::vector<Edge>& es, int s, int t, V want, int mode) {
  auto g = build(n, es);
  V flow = 0, cost = 0;
  while (flow < want) {
    std::vector<V> dist(n, INF);
    std::vector<int> pv(n, -1), pe(n, -1);
    dist[s] = 0;
    if (mode == 0) {
      for (int round = 0; round < n; ++round) {
        bool ch = false;
        for (int u = 0; u < n; ++u) if (dist[u] < INF)
          for (int i = 0; i < (int)g[u].size(); ++i) {
            auto& a = g[u][i];
            if (a.cap > 0 && dist[u] + a.cost < dist[a.to]) { dist[a.to] = dist[u] + a.cost; pv[a.to] = u; pe[a.to] = i; ch = true; }
          }
        if (!ch) break;
      }
    } else if (mode == 1) {
      std::deque<int> q{s};
      std::vector<char> in(n, 0);
      in[s] = 1;
      while (!q.empty()) {
        int u = q.front(); q.pop_front(); in[u] = 0;
        for (int i = 0; i < (int)g[u].size(); ++i) {
          auto& a = g[u][i];
          if (a.cap > 0 && dist[u] + a.cost < dist[a.to]) {
            dist[a.to] = dist[u] + a.cost; pv[a.to] = u; pe[a.to] = i;
            if (!in[a.to]) { in[a.to] = 1; q.push_back(a.to); }
          }
        }
      }
    } else {
      using P = std::pair<V, int>;
      std::priority_queue<P, std::vector<P>, std::greater<>> q;
      q.push({0, s});
      while (!q.empty()) {
        auto [d, u] = q.top(); q.pop();
        if (d > dist[u]) continue;
        for (int i = 0; i < (int)g[u].size(); ++i) {
          auto& a = g[u][i];
          if (a.cap > 0 && a.cost >= 0 && d + a.cost < dist[a.to]) { dist[a.to] = d + a.cost; pv[a.to] = u; pe[a.to] = i; q.push({dist[a.to], a.to}); }
        }
      }
    }
    if (dist[t] >= INF) break;
    V push = want - flow;
    for (int v = t; v != s; v = pv[v]) push = std::min(push, g[pv[v]][pe[v]].cap);
    for (int v = t; v != s; v = pv[v]) { auto& a = g[pv[v]][pe[v]]; a.cap -= push; g[v][a.rev].cap += push; }
    flow += push;
    cost += push * dist[t];
  }
  return {flow, cost};
}

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/optimization/min-cost-flow/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  for (int n : {50, 100, 200, 400, 800, 1600}) {
    algo::Rng rng(n);
    std::vector<Edge> es;
    for (int v = 0; v + 1 < n; ++v) es.push_back({v, v + 1, rng.range(1, 20), rng.range(1, 100)});
    for (int i = 0; i < 4 * n; ++i) {
      int u = (int)rng.below(n), v = (int)rng.below(n);
      if (u != v) es.push_back({u, v, rng.range(1, 20), rng.range(1, 100)});
    }
    const V want = 3 * n / 10 + 5;
    V f_mod = 0, c_mod = 0;
    auto sm = measure({}, [&] {
      algo::MinCostFlow g(n);
      for (auto& e : es) g.add_edge(e.u, e.v, e.cap, e.cost);
      auto r = g.min_cost_flow(0, n - 1, want);
      f_mod = r.flow; c_mod = r.cost;
      algo::bench::do_not_optimize(c_mod);
    }, 5);
    csv.row("Dijkstra + potentials (module)", "random-sparse", n, sm, "flow=" + std::to_string(f_mod) + ";cost=" + std::to_string(c_mod) + ";ratio=1.000000");
    for (int mode : {0, 1}) {
      std::pair<V, V> r{};
      auto s = measure({}, [&] { r = ssp(n, es, 0, n - 1, want, mode); algo::bench::do_not_optimize(r); }, 3);
      if (r.first != f_mod || r.second != c_mod) { std::cerr << "SSP mismatch mode " << mode << "\n"; return 3; }
      csv.row(mode == 0 ? "Bellman-Ford SSP (brute force)" : "SPFA SSP (baseline)", "random-sparse", n, s,
              "flow=" + std::to_string(r.first) + ";cost=" + std::to_string(r.second) + ";ratio=1.000000");
    }
    std::pair<V, V> gr{};
    auto sg = measure({}, [&] { gr = ssp(n, es, 0, n - 1, want, 2); algo::bench::do_not_optimize(gr); }, 3);
    // Compare cost per unit of flow because the greedy may route less flow than the optimum.
    const double ratio = gr.first ? (double(gr.second) / gr.first) / (double(c_mod) / f_mod) : 0;
    csv.row("greedy no-undo (heuristic)", "random-sparse", n, sg,
            "flow=" + std::to_string(gr.first) + ";cost=" + std::to_string(gr.second) + ";ratio=" + std::to_string(ratio));
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
