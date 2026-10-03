// Strongly-connected-component strategies on identical digraphs:
//   brute force: bitset transitive closure (Warshall, O(V^3/64)), u~v iff u reaches v and v reaches u
//   Kosaraju: two DFS passes over the graph and its transpose
//   Tarjan (this module): one DFS with low-links
// All strategies must agree on the component count (checked).
#include <algorithm>
#include <bitset>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "rng.hpp"
#include "tarjan_scc.hpp"

using algo::bench::measure;
using Adj = std::vector<std::vector<int>>;

static int kosaraju(const Adj& g) {
  const int n = static_cast<int>(g.size());
  Adj rev(n);
  for (int u = 0; u < n; ++u) for (int v : g[u]) rev[v].push_back(u);
  std::vector<char> seen(n, 0);
  std::vector<int> order;
  std::vector<std::pair<int, std::size_t>> st;
  for (int s = 0; s < n; ++s) {
    if (seen[s]) continue;
    seen[s] = 1; st.push_back({s, 0});
    while (!st.empty()) {
      auto& [u, i] = st.back();
      if (i < g[u].size()) { int v = g[u][i++]; if (!seen[v]) { seen[v] = 1; st.push_back({v, 0}); } }
      else { order.push_back(u); st.pop_back(); }
    }
  }
  std::fill(seen.begin(), seen.end(), 0);
  int comps = 0;
  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    if (seen[*it]) continue;
    ++comps;
    std::vector<int> stack{*it}; seen[*it] = 1;
    while (!stack.empty()) {
      int u = stack.back(); stack.pop_back();
      for (int v : rev[u]) if (!seen[v]) { seen[v] = 1; stack.push_back(v); }
    }
  }
  return comps;
}

constexpr int kMaxClosure = 1024;
static int closure_scc(const Adj& g) {
  const int n = static_cast<int>(g.size());
  std::vector<std::bitset<kMaxClosure>> reach(n);
  for (int u = 0; u < n; ++u) { reach[u][u] = 1; for (int v : g[u]) reach[u][v] = 1; }
  for (int k = 0; k < n; ++k) for (int i = 0; i < n; ++i) if (reach[i][k]) reach[i] |= reach[k];
  std::vector<char> done(n, 0);
  int comps = 0;
  for (int u = 0; u < n; ++u) {
    if (done[u]) continue;
    ++comps;
    for (int v = 0; v < n; ++v) if (reach[u][v] && reach[v][u]) done[v] = 1;
  }
  return comps;
}

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/graph-flow/tarjan-scc/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  for (int n : {128, 256, 512, 1024, 4096, 16384, 65536, 262144}) {
    algo::Rng rng(n);
    Adj g(n);
    // Average out-degree 1.5: a giant SCC plus many small ones, the interesting regime.
    for (int i = 0; i < n + n / 2; ++i) g[rng.below(n)].push_back(static_cast<int>(rng.below(n)));
    const int reps = n >= 65536 ? 5 : 9;
    int ct = 0, ck = 0, cb = 0;
    auto st = measure({}, [&] {
      algo::TarjanScc t(n);
      for (int u = 0; u < n; ++u) for (int v : g[u]) t.add_edge(u, v);
      ct = static_cast<int>(t.run().components.size());
      algo::bench::do_not_optimize(ct);
    }, reps);
    auto sk = measure({}, [&] { ck = kosaraju(g); algo::bench::do_not_optimize(ck); }, reps);
    csv.row("Tarjan", "random-digraph", n, st, "components=" + std::to_string(ct));
    csv.row("Kosaraju (two-pass baseline)", "random-digraph", n, sk, "components=" + std::to_string(ck));
    if (ct != ck) { std::cerr << "component count mismatch\n"; return 3; }
    if (n <= kMaxClosure) {
      auto sb = measure({}, [&] { cb = closure_scc(g); algo::bench::do_not_optimize(cb); }, 3);
      csv.row("brute-force transitive closure", "random-digraph", n, sb, "components=" + std::to_string(cb));
      if (cb != ct) { std::cerr << "closure mismatch\n"; return 3; }
    }
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
