#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

#include <catch2/catch_approx.hpp>

#include "christofides.hpp"
#include "test_support.hpp"

namespace {
double optimum(const std::vector<std::vector<double>>& d) {
  const int n = static_cast<int>(d.size()); std::vector<int> p(n - 1); std::iota(p.begin(), p.end(), 1); double best = std::numeric_limits<double>::infinity();
  do { double c = d[0][p[0]]; for (int i=1;i<n-1;++i)c += d[p[i-1]][p[i]]; c += d[p.back()][0]; best = std::min(best,c); } while (std::next_permutation(p.begin(),p.end())); return best;
}
void check_tour(const algo::ChristofidesTour& r, const std::vector<std::vector<double>>& d) {
  const auto n = d.size(); REQUIRE(r.tour.size() == n + 1); REQUIRE(r.tour.front() == r.tour.back());
  std::vector<bool> seen(n, false); for (std::size_t i=0;i<n;++i){REQUIRE(r.tour[i] < n); REQUIRE_FALSE(seen[r.tour[i]]); seen[r.tour[i]]=true;}
  double c=0; for(std::size_t i=1;i<r.tour.size();++i)c += d[r.tour[i-1]][r.tour[i]]; REQUIRE(r.cost == Catch::Approx(c));
}
std::vector<std::vector<double>> euclidean(int n, std::uint64_t seed) { algo::Rng r(seed); std::vector<std::pair<double,double>> p(n); for(auto& q:p){q={r.real()*100,r.real()*100};} std::vector<std::vector<double>> d(n,std::vector<double>(n)); for(int i=0;i<n;++i)for(int j=0;j<n;++j)d[i][j]=std::hypot(p[i].first-p[j].first,p[i].second-p[j].second); return d; }
}

TEST_CASE("S1.1 unit square has a four-edge tour", "[S1][given]") { auto d=std::vector<std::vector<double>>{{0,1,1.41421356237,1},{1,0,1,1.41421356237},{1.41421356237,1,0,1},{1,1.41421356237,1,0}}; auto r=algo::christofides(d); check_tour(r,d); REQUIRE(r.cost==Catch::Approx(4.0)); }
TEST_CASE("S1.2 triangle is exact", "[S1][given]") { auto d=std::vector<std::vector<double>>{{0,2,3},{2,0,4},{3,4,0}}; auto r=algo::christofides(d); check_tour(r,d); REQUIRE(r.cost==Catch::Approx(9.0)); }
TEST_CASE("S1.3 line metric returns the perimeter tour", "[S1][given]") { std::vector<std::vector<double>> d(5,std::vector<double>(5)); for(int i=0;i<5;++i)for(int j=0;j<5;++j)d[i][j]=std::abs(i-j); auto r=algo::christofides(d); check_tour(r,d); REQUIRE(r.cost==Catch::Approx(8.0)); }
TEST_CASE("S1.4 singleton has zero cost", "[S1][boundary]") { auto r=algo::christofides({{0.0}}); REQUIRE(r.tour==std::vector<std::size_t>{0,0}); REQUIRE(r.cost==0.0); }
TEST_CASE("S1.5 empty matrix is rejected", "[S1][invalid]") { REQUIRE_THROWS_AS(algo::christofides({}), std::invalid_argument); }
TEST_CASE("S1.6 nonsquare matrix is rejected", "[S1][invalid]") { REQUIRE_THROWS_AS(algo::christofides({{0,1},{1}}), std::invalid_argument); }
TEST_CASE("S1.7 negative and asymmetric edges are rejected", "[S1][invalid]") { REQUIRE_THROWS_AS(algo::christofides({{0,-1},{-1,0}}), std::invalid_argument); REQUIRE_THROWS_AS(algo::christofides({{0,1},{2,0}}), std::invalid_argument); }
TEST_CASE("S1.8 random Euclidean instance returns a valid tour", "[S1][stress]") { auto d=euclidean(20,81); auto r=algo::christofides(d); check_tour(r,d); REQUIRE(std::isfinite(r.cost)); }

TEST_CASE("S2.1 six-node metric satisfies the three-halves bound", "[S2][quality]") { auto d=euclidean(6,91); auto r=algo::christofides(d); REQUIRE(r.cost <= 1.5*optimum(d)+1e-8); }
TEST_CASE("S2.2 repeated random metrics satisfy the approximation bound", "[S2][quality]") { for(int t=0;t<8;++t){auto d=euclidean(7,100+t); auto r=algo::christofides(d); REQUIRE(r.cost <= 1.5*optimum(d)+1e-7);} }
TEST_CASE("S2.3 every vertex appears exactly once", "[S2][invariant]") { auto d=euclidean(31,111); auto r=algo::christofides(d); check_tour(r,d); }
TEST_CASE("S2.4 reported cost equals the selected edges", "[S2][invariant]") { auto d=euclidean(12,121); auto r=algo::christofides(d); double c=0; for(std::size_t i=1;i<r.tour.size();++i)c+=d[r.tour[i-1]][r.tour[i]]; REQUIRE(r.cost==Catch::Approx(c)); }
TEST_CASE("S2.5 same input gives deterministic tour", "[S2][determinism]") { auto d=euclidean(25,131); auto a=algo::christofides(d), b=algo::christofides(d); REQUIRE(a.tour==b.tour); REQUIRE(a.cost==Catch::Approx(b.cost)); }
TEST_CASE("S2.6 zero-length duplicate points remain valid", "[S2][boundary]") { std::vector<std::vector<double>> d(8,std::vector<double>(8)); for(int i=0;i<8;++i)for(int j=0;j<8;++j)d[i][j]=std::abs(i/2-j/2); auto r=algo::christofides(d); check_tour(r,d); }
TEST_CASE("S2.7 larger instance completes with finite cost", "[S2][scale]") { auto d=euclidean(120,141); auto r=algo::christofides(d); check_tour(r,d); REQUIRE(r.cost>0); }
TEST_CASE("S2.8 symmetric metric with ties is accepted", "[S2][boundary]") { auto d=std::vector<std::vector<double>>(10,std::vector<double>(10,2.0)); for(int i=0;i<10;++i)d[i][i]=0; auto r=algo::christofides(d); check_tour(r,d); REQUIRE(r.cost==Catch::Approx(20.0)); }
