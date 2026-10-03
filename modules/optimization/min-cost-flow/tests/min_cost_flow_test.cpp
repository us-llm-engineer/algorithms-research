#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "min_cost_flow.hpp"

using algo::MinCostFlow;
namespace { using I = std::int64_t; MinCostFlow graph(int n, const std::vector<std::tuple<int,int,I,I>>& e) { MinCostFlow f(n); for (auto [u,v,c,w]:e) f.add_edge(u,v,c,w); return f; } }

TEST_CASE("S1.1 sends a unit along the cheapest of two paths", "[S1]") { auto f=graph(4,{{0,1,2,2},{1,3,2,2},{0,2,2,1},{2,3,2,1}}); auto r=f.min_cost_flow(0,3,2); REQUIRE(r.flow==2); REQUIRE(r.cost==4); }
TEST_CASE("S1.2 parallel arcs choose cheap capacity before expensive capacity", "[S1]") { auto f=graph(2,{{0,1,2,1},{0,1,3,7}}); auto r=f.min_cost_flow(0,1,4); REQUIRE(r.flow==4); REQUIRE(r.cost==16); }
TEST_CASE("S1.3 insufficient capacity returns maximal feasible flow and its exact cost", "[S1]") { auto f=graph(3,{{0,1,1,2},{1,2,1,3}}); auto r=f.min_cost_flow(0,2,4); REQUIRE(r.flow==1); REQUIRE(r.cost==5); }
TEST_CASE("S1.4 zero requested flow is zero cost and preserves a reusable network", "[S1]") { auto f=graph(2,{{0,1,2,3}}); REQUIRE(f.min_cost_flow(0,1,0).cost==0); REQUIRE(f.min_cost_flow(0,1,2).cost==6); }
TEST_CASE("S1.5 negative-cost acyclic edges are handled exactly", "[S1]") { auto f=graph(3,{{0,1,2,-4},{1,2,2,7},{0,2,2,9}}); auto r=f.min_cost_flow(0,2,2); REQUIRE(r.flow==2); REQUIRE(r.cost==6); }
TEST_CASE("S1.6 reverse residual edges repair an initially expensive route", "[S1]") { auto f=graph(4,{{0,1,1,0},{0,2,1,0},{1,2,1,1},{1,3,1,10},{2,3,1,0}}); auto r=f.min_cost_flow(0,3,2); REQUIRE(r.flow==2); REQUIRE(r.cost==10); }
TEST_CASE("S1.7 self loops and zero capacity edges do not affect result", "[S1]") { auto f=graph(3,{{0,0,99,-99},{0,1,0,1},{0,1,1,2},{1,2,1,3}}); auto r=f.min_cost_flow(0,2,1); REQUIRE(r.cost==5); }
TEST_CASE("S1.8 invalid vertices, negative capacity, and same terminals reject", "[S1]") { MinCostFlow f(2); REQUIRE_THROWS_AS(f.add_edge(-1,1,1,0),std::out_of_range); REQUIRE_THROWS_AS(f.add_edge(0,1,-1,0),std::invalid_argument); REQUIRE_THROWS_AS(f.min_cost_flow(0,0,1),std::invalid_argument); }
TEST_CASE("S2.1 returned edge flows satisfy capacity and conservation", "[S2]") { auto f=graph(4,{{0,1,3,2},{0,2,2,1},{1,3,3,1},{2,3,2,4}}); auto r=f.min_cost_flow(0,3,4); REQUIRE(r.flow==4); REQUIRE(f.check_certificate(0,3,r)); }
TEST_CASE("S2.2 cost is invariant under edge insertion permutation", "[S2]") { auto a=graph(3,{{0,1,2,1},{1,2,2,2},{0,2,2,8}}); auto b=graph(3,{{0,2,2,8},{1,2,2,2},{0,1,2,1}}); REQUIRE(a.min_cost_flow(0,2,2).cost==b.min_cost_flow(0,2,2).cost); }
TEST_CASE("S2.3 requesting more flow cannot reduce total cost with nonnegative costs", "[S2]") { auto a=graph(2,{{0,1,5,3}}); auto b=graph(2,{{0,1,5,3}}); REQUIRE(a.min_cost_flow(0,1,4).cost>=b.min_cost_flow(0,1,2).cost); }
TEST_CASE("S2.4 repeated calls recompute from original capacities", "[S2]") { auto f=graph(2,{{0,1,3,5}}); REQUIRE(f.min_cost_flow(0,1,3).cost==15); REQUIRE(f.min_cost_flow(0,1,3).cost==15); }
TEST_CASE("S2.5 large costs use 64-bit arithmetic", "[S2]") { auto f=graph(2,{{0,1,3,1'000'000'000'000LL}}); REQUIRE(f.min_cost_flow(0,1,3).cost==3'000'000'000'000LL); }
TEST_CASE("S2.6 disconnected graphs expose no false path", "[S2]") { auto f=graph(4,{{0,1,2,1},{2,3,2,1}}); auto r=f.min_cost_flow(0,3,2); REQUIRE(r.flow==0); REQUIRE(r.cost==0); }
TEST_CASE("S2.7 bottleneck expansion has predictable marginal cost", "[S2]") { auto f=graph(3,{{0,1,1,1},{0,1,2,9},{1,2,3,2}}); auto r=f.min_cost_flow(0,2,3); REQUIRE(r.cost==1+9*2+2*3); }
TEST_CASE("S2.8 potentials certify nonnegative reduced costs after solve", "[S2]") { auto f=graph(3,{{0,1,2,-1},{1,2,2,3},{0,2,2,5}}); auto r=f.min_cost_flow(0,2,2); REQUIRE(f.reduced_costs_nonnegative(r)); }
