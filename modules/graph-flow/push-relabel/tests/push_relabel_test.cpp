// Hard tests for algo::MaxFlow (FIFO and highest-label push-relabel, with Edmonds-Karp and Dinic baselines).
// Oracles that do not trust the library: an adjacency-matrix Edmonds-Karp written in this file, brute-force
// enumeration of all s-t cuts, and flow/cut certificates recomputed from flow_on().
//   [S1] correctness on boundaries, brute-force cuts, random and structured graphs, certificates
//   [S2] operation-count bounds of the theory, heuristics, adversarial inputs, overflow, scale
#include <algorithm>
#include <catch2/catch_template_test_macros.hpp>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <queue>
#include <vector>

#include "push_relabel.hpp"
#include "test_support.hpp"

using algo::MaxFlow;
using Alg = MaxFlow::Algorithm;
using Cap = MaxFlow::Cap;
using algo::Rng;

namespace {
const Alg kAll[] = {Alg::FifoPushRelabel, Alg::HighestLabelPushRelabel, Alg::EdmondsKarp, Alg::Dinic};
const Alg kPR[] = {Alg::FifoPushRelabel, Alg::HighestLabelPushRelabel};

struct Edge { int u, v; Cap c; };
struct Graph {
  int n;
  std::vector<Edge> edges;
};

MaxFlow build(const Graph& g) {
  MaxFlow f(g.n);
  for (const auto& e : g.edges) f.add_edge(e.u, e.v, e.c);
  return f;
}

// Independent oracle: Edmonds-Karp on an adjacency matrix (parallel edges summed, self-loops dropped).
Cap oracle_flow(const Graph& g, int s, int t) {
  const auto n = static_cast<std::size_t>(g.n);
  std::vector<std::vector<Cap>> cap(n, std::vector<Cap>(n, 0));
  for (const auto& e : g.edges) if (e.u != e.v) cap[static_cast<std::size_t>(e.u)][static_cast<std::size_t>(e.v)] += e.c;
  Cap total = 0;
  for (;;) {
    std::vector<int> parent(n, -1);
    parent[static_cast<std::size_t>(s)] = s;
    std::queue<int> q;
    q.push(s);
    while (!q.empty() && parent[static_cast<std::size_t>(t)] < 0) {
      const int u = q.front(); q.pop();
      for (int v = 0; v < g.n; ++v)
        if (parent[static_cast<std::size_t>(v)] < 0 && cap[static_cast<std::size_t>(u)][static_cast<std::size_t>(v)] > 0) { parent[static_cast<std::size_t>(v)] = u; q.push(v); }
    }
    if (parent[static_cast<std::size_t>(t)] < 0) return total;
    Cap bottleneck = std::numeric_limits<Cap>::max();
    for (int v = t; v != s; v = parent[static_cast<std::size_t>(v)]) bottleneck = std::min(bottleneck, cap[static_cast<std::size_t>(parent[static_cast<std::size_t>(v)])][static_cast<std::size_t>(v)]);
    for (int v = t; v != s; v = parent[static_cast<std::size_t>(v)]) { cap[static_cast<std::size_t>(parent[static_cast<std::size_t>(v)])][static_cast<std::size_t>(v)] -= bottleneck; cap[static_cast<std::size_t>(v)][static_cast<std::size_t>(parent[static_cast<std::size_t>(v)])] += bottleneck; }
    total += bottleneck;
  }
}

// Brute force: minimum capacity over all vertex sets S with s in S, t not in S.
Cap brute_min_cut(const Graph& g, int s, int t) {
  Cap best = std::numeric_limits<Cap>::max();
  for (std::uint32_t mask = 0; mask < (1u << g.n); ++mask) {
    if (!(mask >> s & 1) || (mask >> t & 1)) continue;
    Cap c = 0;
    for (const auto& e : g.edges) if ((mask >> e.u & 1) && !(mask >> e.v & 1)) c += e.c;
    best = std::min(best, c);
  }
  return best;
}

// Independent certificate: capacities respected, conservation off {s,t}, net flow out of s equals the value and
// the capacity of the cut induced by the residual-reachable side.
void verify_certificate(const MaxFlow& f, const Graph& g, int s, int t, Cap value) {
  std::vector<Cap> net(static_cast<std::size_t>(g.n), 0);
  for (std::size_t i = 0; i < g.edges.size(); ++i) {
    const Cap fl = f.flow_on(static_cast<int>(i));
    REQUIRE(fl >= 0);
    REQUIRE(fl <= g.edges[i].c);
    if (g.edges[i].u == g.edges[i].v) continue;
    net[static_cast<std::size_t>(g.edges[i].u)] -= fl;
    net[static_cast<std::size_t>(g.edges[i].v)] += fl;
  }
  for (int v = 0; v < g.n; ++v) if (v != s && v != t) REQUIRE(net[static_cast<std::size_t>(v)] == 0);
  REQUIRE(-net[static_cast<std::size_t>(s)] == value);
  REQUIRE(net[static_cast<std::size_t>(t)] == value);
  const auto side = f.min_cut_source_side();
  REQUIRE(side[static_cast<std::size_t>(s)]);
  REQUIRE_FALSE(side[static_cast<std::size_t>(t)]);
  Cap cut = 0;
  for (const auto& e : g.edges) if (side[static_cast<std::size_t>(e.u)] && !side[static_cast<std::size_t>(e.v)]) cut += e.c;
  REQUIRE(cut == value);  // max-flow = min-cut certificate
}

Graph random_graph(Rng& r, int n, int m, Cap max_cap, double zero_prob = 0.1) {
  Graph g{n, {}};
  for (int i = 0; i < m; ++i) {
    const int u = static_cast<int>(r.below(static_cast<std::uint64_t>(n))), v = static_cast<int>(r.below(static_cast<std::uint64_t>(n)));
    g.edges.push_back({u, v, r.coin(zero_prob) ? 0 : r.range(1, max_cap)});
  }
  return g;
}

Graph grid(int k, Cap c) {  // k x k grid, edges right/down/left/up, source top-left, sink bottom-right
  Graph g{k * k, {}};
  auto id = [k](int x, int y) { return y * k + x; };
  for (int y = 0; y < k; ++y)
    for (int x = 0; x < k; ++x) {
      if (x + 1 < k) { g.edges.push_back({id(x, y), id(x + 1, y), c}); g.edges.push_back({id(x + 1, y), id(x, y), c}); }
      if (y + 1 < k) { g.edges.push_back({id(x, y), id(x, y + 1), c}); g.edges.push_back({id(x, y + 1), id(x, y), c}); }
    }
  return g;
}
}  // namespace

// ------------------------------------------------------------------ S1

TEST_CASE("S1.1 CLRS Figure 26.1 network has maximum flow 23, for every algorithm", "[S1][given]") {
  Graph g{6, {{0, 1, 16}, {0, 2, 13}, {1, 2, 10}, {2, 1, 4}, {1, 3, 12}, {3, 2, 9}, {2, 4, 14}, {4, 3, 7}, {3, 5, 20}, {4, 5, 4}}};
  for (Alg a : kAll) {
    MaxFlow f = build(g);
    REQUIRE(f.max_flow(0, 5, a) == 23);
    verify_certificate(f, g, 0, 5, 23);
  }
}

TEST_CASE("S1.2 degenerate inputs: s==t, no edges, disconnected, zero capacity, parallel edges, self-loops, bad arguments", "[S1][boundary]") {
  for (Alg a : kAll) {
    MaxFlow empty(2);
    REQUIRE(empty.max_flow(0, 1, a) == 0);
    MaxFlow f(4);
    REQUIRE_THROWS_AS(f.max_flow(1, 1, a), std::invalid_argument);
    REQUIRE_THROWS_AS(f.max_flow(-1, 2, a), std::out_of_range);
    REQUIRE_THROWS_AS(f.max_flow(0, 4, a), std::out_of_range);
    REQUIRE_THROWS_AS(f.add_edge(0, 4, 1), std::out_of_range);
    REQUIRE_THROWS_AS(f.add_edge(-1, 1, 1), std::out_of_range);
    REQUIRE_THROWS_AS(f.add_edge(0, 1, -1), std::invalid_argument);
    f.add_edge(0, 1, 5);
    f.add_edge(2, 3, 5);
    REQUIRE(f.max_flow(0, 3, a) == 0);      // s and t in different components
    f.add_edge(1, 2, 0);                    // zero-capacity bridge
    REQUIRE(f.max_flow(0, 3, a) == 0);
    f.add_edge(1, 2, 3);
    f.add_edge(1, 2, 4);                    // parallel edges add up
    f.add_edge(1, 1, 100);                  // self-loop is harmless
    REQUIRE(f.max_flow(0, 3, a) == 5);
    REQUIRE(f.max_flow(0, 3, a) == 5);      // idempotent: recomputes from scratch
    REQUIRE(f.num_vertices() == 4);
    REQUIRE(f.num_edges() == 6);
  }
}

TEST_CASE("S1.3 max-flow equals brute-force minimum cut on 600 tiny random graphs, all algorithms", "[S1][stress]") {
  for (std::uint64_t trial = 0; trial < 600; ++trial) {
    ALGO_TRIAL(trial, 501);
    Rng r(seed_);
    const int n = 2 + static_cast<int>(r.below(7));
    Graph g = random_graph(r, n, 1 + static_cast<int>(r.below(20)), 12, 0.2);
    const int s = 0, t = n - 1;
    const Cap expect = brute_min_cut(g, s, t);
    for (Alg a : kAll) {
      MaxFlow f = build(g);
      REQUIRE(f.max_flow(s, t, a) == expect);
    }
  }
}

TEST_CASE("S1.4 all four algorithms agree with the matrix Edmonds-Karp oracle (300 seeds, sparse and dense)", "[S1][stress]") {
  for (std::uint64_t trial = 0; trial < 300; ++trial) {
    ALGO_TRIAL(trial, 504);
    Rng r(seed_);
    const int n = 3 + static_cast<int>(r.below(40));
    const int m = static_cast<int>(r.below(static_cast<std::uint64_t>(n * n / 2 + 4)));
    Graph g = random_graph(r, n, m, trial % 3 == 0 ? 1 : trial % 3 == 1 ? 100 : 1'000'000'000);
    const int s = static_cast<int>(r.below(static_cast<std::uint64_t>(n))), t = (s + 1 + static_cast<int>(r.below(static_cast<std::uint64_t>(n - 1)))) % n;
    const Cap expect = oracle_flow(g, s, t);
    for (Alg a : kAll) {
      MaxFlow f = build(g);
      REQUIRE(f.max_flow(s, t, a) == expect);
    }
  }
}

TEST_CASE("S1.5 structured families: grid, layered, complete, path, star, bipartite network, series-parallel", "[S1][structural]") {
  std::vector<std::pair<Graph, std::pair<int, int>>> cases;
  cases.push_back({grid(12, 3), {0, 143}});
  {  // layered: 6 layers of width 8, random capacities between consecutive layers
    Rng r(1);
    Graph g{2 + 6 * 8, {}};
    for (int v = 0; v < 8; ++v) g.edges.push_back({0, 1 + v, 50});
    for (int l = 0; l + 1 < 6; ++l)
      for (int a = 0; a < 8; ++a) for (int b = 0; b < 8; ++b) if (r.coin(0.5)) g.edges.push_back({1 + l * 8 + a, 1 + (l + 1) * 8 + b, r.range(1, 30)});
    for (int v = 0; v < 8; ++v) g.edges.push_back({1 + 5 * 8 + v, 1 + 6 * 8, 50});
    cases.push_back({g, {0, 1 + 6 * 8}});
  }
  {  // complete digraph on 14 vertices
    Graph g{14, {}};
    Rng r(2);
    for (int u = 0; u < 14; ++u) for (int v = 0; v < 14; ++v) if (u != v) g.edges.push_back({u, v, r.range(1, 20)});
    cases.push_back({g, {0, 13}});
  }
  {  // long path with one bottleneck, and a star
    Graph p{200, {}};
    for (int i = 0; i + 1 < 200; ++i) p.edges.push_back({i, i + 1, i == 77 ? 3 : 1000});
    cases.push_back({p, {0, 199}});
    Graph st{100, {}};
    for (int i = 1; i < 99; ++i) { st.edges.push_back({0, i, i}); st.edges.push_back({i, 99, 50}); }
    cases.push_back({st, {0, 99}});
  }
  {  // unit-capacity bipartite matching network: 60 + 60 vertices
    Rng r(3);
    Graph g{122, {}};
    for (int i = 0; i < 60; ++i) { g.edges.push_back({120, i, 1}); g.edges.push_back({60 + i, 121, 1}); }
    for (int i = 0; i < 60; ++i) for (int d = 0; d < 4; ++d) g.edges.push_back({i, 60 + static_cast<int>(r.below(60)), 1});
    cases.push_back({g, {120, 121}});
  }
  {  // series-parallel: diamond ladder
    Graph g{2 + 3 * 40, {}};
    int prev = 0;
    for (int k = 0; k < 40; ++k) {
      const int a = 1 + 3 * k, b = a + 1, c = a + 2;
      g.edges.push_back({prev, a, 7}); g.edges.push_back({prev, b, 5}); g.edges.push_back({a, c, 4}); g.edges.push_back({b, c, 9});
      prev = c;
    }
    g.edges.push_back({prev, 121, 100});
    cases.push_back({g, {0, 121}});
  }
  for (auto& [g, st] : cases) {
    const Cap expect = oracle_flow(g, st.first, st.second);
    for (Alg a : kAll) {
      MaxFlow f = build(g);
      REQUIRE(f.max_flow(st.first, st.second, a) == expect);
      verify_certificate(f, g, st.first, st.second, expect);
    }
  }
}

TEST_CASE("S1.6 independent flow/cut certificate holds on 200 random graphs for each algorithm", "[S1][invariant]") {
  for (std::uint64_t trial = 0; trial < 200; ++trial) {
    ALGO_TRIAL(trial, 506);
    Rng r(seed_);
    const int n = 4 + static_cast<int>(r.below(30));
    Graph g = random_graph(r, n, n * 3, 50);
    for (Alg a : kAll) {
      MaxFlow f = build(g);
      const Cap v = f.max_flow(0, n - 1, a);
      verify_certificate(f, g, 0, n - 1, v);
      f.check_certificate(0, n - 1, v);
    }
  }
}

TEST_CASE("S1.7 reuse: different (s,t) pairs on one network, edges added between runs", "[S1][structural]") {
  Rng r(7);
  Graph g = random_graph(r, 25, 120, 40);
  MaxFlow f = build(g);
  for (int round = 0; round < 40; ++round) {
    const int s = static_cast<int>(r.below(25)), t = (s + 1 + static_cast<int>(r.below(24))) % 25;
    for (Alg a : kAll) REQUIRE(f.max_flow(s, t, a) == oracle_flow(g, s, t));
    const int u = static_cast<int>(r.below(25)), v = static_cast<int>(r.below(25));
    const Cap c = r.range(0, 40);
    g.edges.push_back({u, v, c});
    f.add_edge(u, v, c);
  }
}

TEST_CASE("S1.8 gap heuristic and global relabeling are optimizations only: results identical with them off", "[S1][structural]") {
  for (std::uint64_t trial = 0; trial < 120; ++trial) {
    ALGO_TRIAL(trial, 508);
    Rng r(seed_);
    const int n = 5 + static_cast<int>(r.below(60));
    Graph g = random_graph(r, n, n * 4, 100);
    const Cap expect = oracle_flow(g, 0, n - 1);
    for (Alg a : kPR)
      for (int mask = 0; mask < 4; ++mask) {
        MaxFlow f = build(g);
        f.set_options({(mask & 1) != 0, (mask & 2) != 0});
        REQUIRE(f.max_flow(0, n - 1, a) == expect);
        verify_certificate(f, g, 0, n - 1, expect);
      }
  }
}

// ------------------------------------------------------------------ S2

TEST_CASE("S2.1 final height labeling is valid: h(s)=n, h(t)=0, h(u) <= h(v)+1 on residual arcs, h < 2n", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 120; ++trial) {
    ALGO_TRIAL(trial, 551);
    Rng r(seed_);
    const int n = 5 + static_cast<int>(r.below(50));
    Graph g = random_graph(r, n, n * 4, 30);
    for (Alg a : kPR) {
      MaxFlow f = build(g);
      f.max_flow(0, n - 1, a);
      f.check_labeling();
    }
  }
}

TEST_CASE("S2.2 push-relabel operation counts respect the theory (relabels <= 2n^2, saturating pushes <= 2nm, total pushes <= 2nm + 6n^3)", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 60; ++trial) {
    ALGO_TRIAL(trial, 552);
    Rng r(seed_);
    const int n = 10 + static_cast<int>(r.below(120));
    const int m = n * (2 + static_cast<int>(r.below(6)));
    Graph g = random_graph(r, n, m, trial % 2 ? 1000 : 3);
    for (Alg a : kPR) {
      MaxFlow f = build(g);
      f.set_options({false, false});  // the proofs bound the plain algorithm
      f.max_flow(0, n - 1, a);
      const auto& st = f.stats();
      const double N = n, M = m;
      INFO("n=" << n << " m=" << m << " relabels=" << st.relabels << " sat=" << st.saturating_pushes << " pushes=" << st.pushes);
      REQUIRE(static_cast<double>(st.relabels) <= 2 * N * N);
      REQUIRE(static_cast<double>(st.saturating_pushes) <= 2 * N * M);
      REQUIRE(static_cast<double>(st.pushes) <= 2 * N * M + 6 * N * N * N);
    }
  }
}

TEST_CASE("S2.3 gap heuristic pays off: a dead-end chain behind a bottleneck needs >=4x fewer relabels", "[S2][adversarial]") {
  const int k = 600;  // source feeds a long chain that can never reach the sink
  Graph g{k + 3, {}};
  const int s = 0, t = k + 2;
  g.edges.push_back({s, t, 1});
  g.edges.push_back({s, 1, 1'000'000});
  for (int i = 1; i < k; ++i) g.edges.push_back({i, i + 1, 1'000'000});  // dead end at vertex k
  std::vector<std::uint64_t> relabels;
  for (bool gap : {true, false}) {
    MaxFlow f = build(g);
    f.set_options({gap, false});
    REQUIRE(f.max_flow(s, t, Alg::HighestLabelPushRelabel) == 1);
    relabels.push_back(f.stats().relabels);
  }
  INFO("relabels with gap=" << relabels[0] << " without=" << relabels[1]);
  REQUIRE(relabels[0] * 4 < relabels[1]);
}

TEST_CASE("S2.4 the classic bad case for naive augmenting paths (capacity 10^9, cross edge 1) is solved in few operations", "[S2][adversarial]") {
  const Cap big = 1'000'000'000;
  Graph g{4, {{0, 1, big}, {0, 2, big}, {1, 2, 1}, {1, 3, big}, {2, 3, big}}};
  for (Alg a : kAll) {
    MaxFlow f = build(g);
    REQUIRE(f.max_flow(0, 3, a) == 2 * big);
    if (a == Alg::EdmondsKarp) REQUIRE(f.stats().augmentations <= 10);  // O(VE): shortest paths, not a function of the capacities
    if (a == Alg::Dinic) REQUIRE(f.stats().phases <= 4);
    if (a == Alg::FifoPushRelabel || a == Alg::HighestLabelPushRelabel) REQUIRE(f.stats().pushes <= 50);
  }
}

TEST_CASE("S2.5 baselines obey their own bounds: Edmonds-Karp augmentations <= V*E/2, Dinic phases <= V", "[S2][invariant]") {
  for (std::uint64_t trial = 0; trial < 80; ++trial) {
    ALGO_TRIAL(trial, 555);
    Rng r(seed_);
    const int n = 5 + static_cast<int>(r.below(60));
    const int m = n * 3;
    Graph g = random_graph(r, n, m, 20);
    MaxFlow ek = build(g), dn = build(g);
    ek.max_flow(0, n - 1, Alg::EdmondsKarp);
    dn.max_flow(0, n - 1, Alg::Dinic);
    REQUIRE(static_cast<double>(ek.stats().augmentations) <= 0.5 * n * m + 1);
    REQUIRE(dn.stats().phases <= static_cast<std::uint64_t>(n));
  }
}

TEST_CASE("S2.6 capacities near 2^62: exact arithmetic, and a clear error when the source capacity would overflow", "[S2][boundary]") {
  const Cap huge = Cap{1} << 62;  // 4.6e18
  for (Alg a : kAll) {
    MaxFlow ok(3);
    ok.add_edge(0, 1, huge);
    ok.add_edge(1, 2, huge);
    REQUIRE(ok.max_flow(0, 2, a) == huge);
    MaxFlow two(4);
    two.add_edge(0, 1, huge - 5);
    two.add_edge(0, 2, 4'000'000'000'000'000'000LL);
    two.add_edge(1, 3, huge);
    two.add_edge(2, 3, 4'000'000'000'000'000'000LL);
    REQUIRE(two.max_flow(0, 3, a) == huge - 5 + 4'000'000'000'000'000'000LL);
    MaxFlow bad(3);
    bad.add_edge(0, 1, std::numeric_limits<Cap>::max());
    bad.add_edge(0, 2, std::numeric_limits<Cap>::max());
    bad.add_edge(1, 2, 7);
    REQUIRE_THROWS_AS(bad.max_flow(0, 2, a), std::overflow_error);
  }
}

TEST_CASE("S2.7 scale: 300x300 unit grid and a 20000-node sparse random graph, all algorithms agree and certify", "[S2][scale]") {
  {
    Graph g = grid(300, 1);
    Cap value = -1;
    for (Alg a : {Alg::HighestLabelPushRelabel, Alg::FifoPushRelabel, Alg::Dinic}) {
      MaxFlow f = build(g);
      const Cap v = f.max_flow(0, 300 * 300 - 1, a);
      if (value < 0) value = v;
      REQUIRE(v == value);
      verify_certificate(f, g, 0, 300 * 300 - 1, v);
    }
    REQUIRE(value == 2);  // corner vertices have out-degree 2
  }
  {
    Rng r(9);
    Graph g = random_graph(r, 20000, 100000, 1000, 0.0);
    Cap value = -1;
    for (Alg a : {Alg::HighestLabelPushRelabel, Alg::FifoPushRelabel, Alg::Dinic}) {
      MaxFlow f = build(g);
      const Cap v = f.max_flow(0, 19999, a);
      if (value < 0) value = v;
      REQUIRE(v == value);
      verify_certificate(f, g, 0, 19999, v);
    }
  }
}

TEST_CASE("S2.8 unit-capacity bipartite network of 3000+3000 vertices: flow equals Hopcroft-Karp-style matching size", "[S2][scale]") {
  Rng r(10);
  const int n = 3000;
  Graph g{2 * n + 2, {}};
  const int s = 2 * n, t = 2 * n + 1;
  for (int i = 0; i < n; ++i) { g.edges.push_back({s, i, 1}); g.edges.push_back({n + i, t, 1}); }
  std::vector<std::vector<int>> adj(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i)
    for (int d = 0; d < 3; ++d) { const int j = static_cast<int>(r.below(n)); g.edges.push_back({i, n + j, 1}); adj[static_cast<std::size_t>(i)].push_back(j); }
  // reference matching by simple augmenting paths (Kuhn), iterative-free but depth is small for sparse graphs
  std::vector<int> match_r(static_cast<std::size_t>(n), -1);
  int matching = 0;
  std::vector<int> seen(static_cast<std::size_t>(n));
  std::function<bool(int, int)> try_kuhn = [&](int u, int stamp) {
    for (int v : adj[static_cast<std::size_t>(u)]) {
      if (seen[static_cast<std::size_t>(v)] == stamp) continue;
      seen[static_cast<std::size_t>(v)] = stamp;
      if (match_r[static_cast<std::size_t>(v)] < 0 || try_kuhn(match_r[static_cast<std::size_t>(v)], stamp)) { match_r[static_cast<std::size_t>(v)] = u; return true; }
    }
    return false;
  };
  for (int u = 0; u < n; ++u) if (try_kuhn(u, u + 1)) ++matching;
  for (Alg a : kAll) {
    MaxFlow f = build(g);
    REQUIRE(f.max_flow(s, t, a) == matching);
  }
}
