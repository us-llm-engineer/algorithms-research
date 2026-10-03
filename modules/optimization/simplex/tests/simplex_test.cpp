#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <limits>
#include <vector>
#include "simplex.hpp"
using algo::Simplex; using Vec=std::vector<double>; using Mat=std::vector<Vec>;
using Catch::Approx;
TEST_CASE("S1.1 solves a bounded two-variable LP", "[S1]"){auto r=Simplex::solve({{1,1},{1,0},{0,1}},{4,3,2},{3,2});REQUIRE(r.status==Simplex::Status::Optimal);REQUIRE(r.objective==Approx(11));}
TEST_CASE("S1.2 zero objective has zero optimum", "[S1]"){auto r=Simplex::solve({{1,0},{0,1}},{3,4},{0,0});REQUIRE(r.status==Simplex::Status::Optimal);REQUIRE(r.objective==Approx(0));}
TEST_CASE("S1.3 one constraint picks the greatest density", "[S1]"){auto r=Simplex::solve({{2,1}},{4},{3,1});REQUIRE(r.status==Simplex::Status::Optimal);REQUIRE(r.x[0]==Approx(2));}
TEST_CASE("S1.4 infeasible constraints are reported", "[S1]"){auto r=Simplex::solve({{1},{-1}},{1,-2},{1});REQUIRE(r.status==Simplex::Status::Infeasible);}
TEST_CASE("S1.5 unbounded objective is reported", "[S1]"){auto r=Simplex::solve({}, {}, {1});REQUIRE(r.status==Simplex::Status::Unbounded);}
TEST_CASE("S1.6 zero rows and zero columns are accepted", "[S1]"){auto r=Simplex::solve({}, {}, {});REQUIRE(r.status==Simplex::Status::Optimal);}
TEST_CASE("S1.7 redundant constraints preserve optimum", "[S1]"){auto r=Simplex::solve({{1},{2},{1}},{3,6,3},{4});REQUIRE(r.status==Simplex::Status::Optimal);REQUIRE(r.objective==Approx(12));}
TEST_CASE("S1.8 exact zero capacities force zero variables", "[S1]"){auto r=Simplex::solve({{1,0},{0,1}},{0,5},{4,7});REQUIRE(r.status==Simplex::Status::Optimal);REQUIRE(r.x[0]==Approx(0));}
TEST_CASE("S2.1 primal solution satisfies every inequality", "[S2]"){auto r=Simplex::solve({{1,1},{1,0},{0,1}},{4,3,2},{3,2});REQUIRE(r.status==Simplex::Status::Optimal);REQUIRE(Simplex::feasible(r));}
TEST_CASE("S2.2 dual certificate bounds the primal objective", "[S2]"){auto r=Simplex::solve({{1,1}},{4},{3,2});REQUIRE(r.status==Simplex::Status::Optimal);REQUIRE(Simplex::certificate_valid(r));}
TEST_CASE("S2.3 scaling all capacities scales the optimum", "[S2]"){auto a=Simplex::solve({{1,1}},{4},{3,2});auto b=Simplex::solve({{1,1}},{8},{3,2});REQUIRE(b.objective==Approx(2*a.objective));}
TEST_CASE("S2.4 scaling all objective coefficients scales the optimum", "[S2]"){auto a=Simplex::solve({{1,1}},{4},{3,2});auto b=Simplex::solve({{1,1}},{4},{6,4});REQUIRE(b.objective==Approx(2*a.objective));}
TEST_CASE("S2.5 permutation of constraints preserves the result", "[S2]"){auto a=Simplex::solve({{1,1},{1,0}},{4,3},{3,2});auto b=Simplex::solve({{1,0},{1,1}},{3,4},{3,2});REQUIRE(b.objective==Approx(a.objective));}
TEST_CASE("S2.6 near-degenerate pivots remain numerically feasible", "[S2]"){auto r=Simplex::solve({{1,1},{1,1.0000001}},{2,2.0000001},{1,1});REQUIRE(r.status==Simplex::Status::Optimal);REQUIRE(Simplex::feasible(r,1e-7));}
TEST_CASE("S2.7 large coefficients do not overflow double arithmetic", "[S2]"){auto r=Simplex::solve({{1,1}},{1e12},{2,1});REQUIRE(r.status==Simplex::Status::Optimal);REQUIRE(std::isfinite(r.objective));}
TEST_CASE("S2.8 repeated solves are deterministic", "[S2]"){auto a=Simplex::solve({{1,2},{2,1}},{8,8},{3,4});auto b=Simplex::solve({{1,2},{2,1}},{8,8},{3,4});REQUIRE(a.status==b.status);REQUIRE(a.objective==Approx(b.objective));}
