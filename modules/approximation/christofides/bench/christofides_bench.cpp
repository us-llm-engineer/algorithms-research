// Metric TSP strategies on random Euclidean instances, from brute force to near-optimal heuristics:
//   brute force (permutations)  exact, O(n!)           n <= 10
//   Held-Karp DP                exact, O(2^n n^2)      n <= 16
//   nearest neighbour           heuristic, O(n^2)
//   double-tree (MST 2-approx)  2-approximation, O(n^2)
//   nearest neighbour + 2-opt   local-search heuristic
//   christofides                3/2-approximation (this module)
// `extra` records cost and ratio = cost / best cost found at that n (the optimum whenever an exact solver ran).
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "christofides.hpp"
#include "rng.hpp"

using Matrix = std::vector<std::vector<double>>;
using algo::bench::do_not_optimize;
using algo::bench::measure;

static double tour_cost(const Matrix& d, const std::vector<std::size_t>& t) {
  double c = 0;
  for (std::size_t i = 0; i < t.size(); ++i) c += d[t[i]][t[(i + 1) % t.size()]];
  return c;
}

static double brute_force(const Matrix& d) {
  std::vector<std::size_t> p(d.size());
  std::iota(p.begin(), p.end(), 0);
  double best = std::numeric_limits<double>::infinity();
  do { best = std::min(best, tour_cost(d, p)); } while (std::next_permutation(p.begin() + 1, p.end()));
  return best;
}

static double held_karp(const Matrix& d) {
  const std::size_t n = d.size();
  const double inf = std::numeric_limits<double>::infinity();
  std::vector<double> dp((std::size_t{1} << n) * n, inf);
  dp[1 * n + 0] = 0;
  for (std::size_t mask = 1; mask < (std::size_t{1} << n); mask += 2)
    for (std::size_t last = 0; last < n; ++last) {
      const double cur = dp[mask * n + last];
      if (cur == inf) continue;
      for (std::size_t nx = 1; nx < n; ++nx)
        if (!(mask >> nx & 1)) {
          double& slot = dp[(mask | std::size_t{1} << nx) * n + nx];
          slot = std::min(slot, cur + d[last][nx]);
        }
    }
  double best = inf;
  for (std::size_t last = 1; last < n; ++last) best = std::min(best, dp[((std::size_t{1} << n) - 1) * n + last] + d[last][0]);
  return best;
}

static std::vector<std::size_t> nearest_neighbour(const Matrix& d) {
  const std::size_t n = d.size();
  std::vector<std::size_t> t{0};
  std::vector<char> used(n, 0);
  used[0] = 1;
  while (t.size() < n) {
    std::size_t best = n;
    for (std::size_t j = 0; j < n; ++j)
      if (!used[j] && (best == n || d[t.back()][j] < d[t.back()][best])) best = j;
    used[best] = 1;
    t.push_back(best);
  }
  return t;
}

static std::vector<std::size_t> two_opt(const Matrix& d, std::vector<std::size_t> t) {
  const std::size_t n = t.size();
  for (bool improved = true; improved;) {
    improved = false;
    for (std::size_t i = 0; i + 1 < n; ++i)
      for (std::size_t j = i + 2; j < n; ++j) {
        const std::size_t a = t[i], b = t[i + 1], c = t[j], e = t[(j + 1) % n];
        if (a == e) continue;
        if (d[a][c] + d[b][e] < d[a][b] + d[c][e] - 1e-12) {
          std::reverse(t.begin() + i + 1, t.begin() + j + 1);
          improved = true;
        }
      }
  }
  return t;
}

// 2-approximation: preorder walk of a minimum spanning tree (Prim, O(n^2)).
static std::vector<std::size_t> double_tree(const Matrix& d) {
  const std::size_t n = d.size();
  std::vector<double> key(n, std::numeric_limits<double>::infinity());
  std::vector<std::size_t> parent(n, 0);
  std::vector<char> in(n, 0);
  key[0] = 0;
  for (std::size_t it = 0; it < n; ++it) {
    std::size_t u = n;
    for (std::size_t v = 0; v < n; ++v) if (!in[v] && (u == n || key[v] < key[u])) u = v;
    in[u] = 1;
    for (std::size_t v = 0; v < n; ++v) if (!in[v] && d[u][v] < key[v]) { key[v] = d[u][v]; parent[v] = u; }
  }
  std::vector<std::vector<std::size_t>> kids(n);
  for (std::size_t v = 1; v < n; ++v) kids[parent[v]].push_back(v);
  std::vector<std::size_t> order, stack{0};
  while (!stack.empty()) {
    const auto u = stack.back(); stack.pop_back();
    order.push_back(u);
    for (auto it = kids[u].rbegin(); it != kids[u].rend(); ++it) stack.push_back(*it);
  }
  return order;
}

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/approximation/christofides/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);

  for (std::size_t n : {6u, 8u, 9u, 10u, 12u, 14u, 16u, 32u, 64u, 128u, 256u}) {
    algo::Rng rng(n);
    std::vector<std::pair<double, double>> p(n);
    for (auto& q : p) q = {rng.real(), rng.real()};
    Matrix d(n, std::vector<double>(n));
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = 0; j < n; ++j) d[i][j] = std::hypot(p[i].first - p[j].first, p[i].second - p[j].second);
    const int reps = n <= 10 ? 3 : 5;

    struct Row { std::string name; algo::bench::Stats stats; double cost; };
    std::vector<Row> rows;
    auto run = [&](const std::string& name, auto&& fn) {
      double cost = 0;
      auto s = measure({}, [&] { cost = fn(); do_not_optimize(cost); }, reps, 0);
      rows.push_back({name, s, cost});
    };
    if (n <= 10) run("brute force (permutations)", [&] { return brute_force(d); });
    if (n <= 16) run("Held-Karp DP (exact)", [&] { return held_karp(d); });
    run("nearest neighbour (heuristic)", [&] { return tour_cost(d, nearest_neighbour(d)); });
    run("double-tree MST (2-approx)", [&] { return tour_cost(d, double_tree(d)); });
    run("nearest neighbour + 2-opt (heuristic)", [&] { return tour_cost(d, two_opt(d, nearest_neighbour(d))); });
    run("christofides", [&] { return algo::christofides(d).cost; });
    double best = std::numeric_limits<double>::infinity();
    for (auto& r : rows) best = std::min(best, r.cost);
    for (auto& r : rows) csv.row(r.name, "random-euclid", n, r.stats, "cost=" + std::to_string(r.cost) + ";ratio=" + std::to_string(r.cost / best));
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
