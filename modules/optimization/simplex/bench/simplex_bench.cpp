// Linear programs max c.x s.t. Ax <= b, x >= 0 with random positive data (always feasible and bounded):
//   brute force   vertex enumeration: solve every n x n basis of tight constraints, keep the best feasible one
//   greedy        raise variables in order of c_j / column-sum until constraints block (near-optimal heuristic)
//   simplex       this module
// `extra` records the objective and ratio = objective / optimum (checked against the module for brute force).
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "rng.hpp"
#include "simplex.hpp"

using algo::Simplex;
using algo::bench::measure;
using Vec = std::vector<double>;
using Mat = std::vector<Vec>;

static double greedy(const Mat& a, const Vec& b, const Vec& c) {
  const std::size_t m = a.size(), n = c.size();
  std::vector<std::size_t> order(n);
  std::iota(order.begin(), order.end(), 0);
  auto density = [&](std::size_t j) { double s = 0; for (std::size_t i = 0; i < m; ++i) s += a[i][j]; return c[j] / s; };
  std::sort(order.begin(), order.end(), [&](auto x, auto y) { return density(x) > density(y); });
  Vec slack = b;
  double obj = 0;
  for (auto j : order) {
    double t = 1e300;
    for (std::size_t i = 0; i < m; ++i) if (a[i][j] > 1e-12) t = std::min(t, slack[i] / a[i][j]);
    for (std::size_t i = 0; i < m; ++i) slack[i] -= a[i][j] * t;
    obj += c[j] * t;
  }
  return obj;
}

// Every basis picks n of the m + n tight constraints (rows of A or x_j = 0).
static double vertex_enumeration(const Mat& a, const Vec& b, const Vec& c) {
  const std::size_t m = a.size(), n = c.size(), total = m + n;
  std::vector<std::size_t> idx(n);
  std::iota(idx.begin(), idx.end(), 0);
  double best = -1e300;
  while (true) {
    Mat sys(n, Vec(n + 1, 0.0));
    for (std::size_t r = 0; r < n; ++r) {
      if (idx[r] < m) { for (std::size_t j = 0; j < n; ++j) sys[r][j] = a[idx[r]][j]; sys[r][n] = b[idx[r]]; }
      else sys[r][idx[r] - m] = 1.0;
    }
    bool ok = true;
    for (std::size_t col = 0; col < n && ok; ++col) {
      std::size_t piv = col;
      for (std::size_t r = col + 1; r < n; ++r) if (std::abs(sys[r][col]) > std::abs(sys[piv][col])) piv = r;
      if (std::abs(sys[piv][col]) < 1e-10) { ok = false; break; }
      std::swap(sys[piv], sys[col]);
      for (std::size_t r = 0; r < n; ++r) if (r != col) {
        const double f = sys[r][col] / sys[col][col];
        for (std::size_t j = col; j <= n; ++j) sys[r][j] -= f * sys[col][j];
      }
    }
    if (ok) {
      Vec x(n);
      for (std::size_t j = 0; j < n; ++j) { x[j] = sys[j][n] / sys[j][j]; if (x[j] < -1e-9) ok = false; }
      for (std::size_t i = 0; i < m && ok; ++i) {
        double s = 0; for (std::size_t j = 0; j < n; ++j) s += a[i][j] * x[j];
        if (s > b[i] + 1e-9) ok = false;
      }
      if (ok) { double o = 0; for (std::size_t j = 0; j < n; ++j) o += c[j] * x[j]; best = std::max(best, o); }
    }
    std::size_t i = n;
    while (i-- > 0 && idx[i] == total - n + i) {}
    if (i >= n) break;  // wrapped
    ++idx[i];
    for (std::size_t j = i + 1; j < n; ++j) idx[j] = idx[j - 1] + 1;
  }
  return best;
}

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/optimization/simplex/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  for (std::size_t n : {4u, 6u, 8u, 10u, 16u, 32u, 64u, 128u, 256u}) {
    algo::Rng rng(n);
    Mat a(n, Vec(n));
    Vec b(n), c(n);
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) a[i][j] = 0.1 + rng.real();
      b[i] = 1 + 9 * rng.real();
      c[i] = 1 + 9 * rng.real();
    }
    double opt = 0;
    auto ss = measure({}, [&] { opt = Simplex::solve(a, b, c).objective; algo::bench::do_not_optimize(opt); }, 5);
    csv.row("simplex (module)", "random-dense-lp", n, ss, "objective=" + std::to_string(opt) + ";ratio=1.000000");
    double g = 0;
    auto sg = measure({}, [&] { g = greedy(a, b, c); algo::bench::do_not_optimize(g); }, 5);
    csv.row("greedy density (heuristic)", "random-dense-lp", n, sg, "objective=" + std::to_string(g) + ";ratio=" + std::to_string(g / opt));
    if (n <= 10) {
      double v = 0;
      auto sv = measure({}, [&] { v = vertex_enumeration(a, b, c); algo::bench::do_not_optimize(v); }, 3, 0);
      if (std::abs(v - opt) > 1e-6 * std::max(1.0, std::abs(opt))) { std::cerr << "vertex enumeration mismatch " << v << " vs " << opt << "\n"; return 3; }
      csv.row("vertex enumeration (brute force)", "random-dense-lp", n, sv, "objective=" + std::to_string(v) + ";ratio=1.000000");
    }
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
