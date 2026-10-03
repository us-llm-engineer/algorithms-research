// Behavioral contract for Tarjan SCC.  `run()` returns component_of for every
// vertex, component lists, and the deduplicated condensation DAG edges.
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <vector>

#include "tarjan_scc.hpp"
#include "test_support.hpp"

using algo::Rng;
using algo::TarjanScc;

namespace {
TarjanScc make(int n, const std::vector<std::pair<int, int>>& edges) {
  TarjanScc g(n); for (auto [u, v] : edges) g.add_edge(u, v); return g;
}
bool reachable(int n, const std::vector<std::pair<int, int>>& e, int s, int t) {
  std::vector<char> seen(n); std::queue<int> q; q.push(s); seen[s] = true;
  while (!q.empty()) { int u = q.front(); q.pop(); for (auto [a,b] : e) if (a == u && !seen[b]) { seen[b] = true; q.push(b); } }
  return seen[t];
}
void verify(const TarjanScc::Result& r, int n, const std::vector<std::pair<int, int>>& e) {
  REQUIRE(r.component_of.size() == static_cast<std::size_t>(n));
  for (int u = 0; u < n; ++u) for (int v = 0; v < n; ++v)
    REQUIRE((r.component_of[u] == r.component_of[v]) == (reachable(n,e,u,v) && reachable(n,e,v,u)));
  std::vector<int> count(r.components.size());
  for (int u = 0; u < n; ++u) { REQUIRE(r.component_of[u] >= 0); REQUIRE(r.component_of[u] < static_cast<int>(r.components.size())); ++count[r.component_of[u]]; }
  for (std::size_t c = 0; c < r.components.size(); ++c) REQUIRE(count[c] == static_cast<int>(r.components[c].size()));
  auto dag = r.condensation_edges; std::sort(dag.begin(), dag.end()); REQUIRE(std::adjacent_find(dag.begin(), dag.end()) == dag.end());
  for (auto [a,b] : dag) { REQUIRE(a != b); REQUIRE(a >= 0); REQUIRE(b >= 0); REQUIRE(a < static_cast<int>(r.components.size())); REQUIRE(b < static_cast<int>(r.components.size())); }
}
}  // namespace

TEST_CASE("S1.1 CLRS example partitions into the documented SCCs", "[S1][given]") { auto e=std::vector<std::pair<int,int>>{{0,1},{1,2},{2,0},{1,3},{3,4},{4,5},{5,3},{5,6},{6,7},{7,6}}; auto r=make(8,e).run(); verify(r,8,e); REQUIRE(r.components.size()==3); }
TEST_CASE("S1.2 empty, singleton, isolated vertices and self loops are valid", "[S1][boundary]") { REQUIRE(make(0,{}).run().components.empty()); auto e=std::vector<std::pair<int,int>>{{0,0},{2,2}}; auto r=make(4,e).run(); verify(r,4,e); REQUIRE(r.components.size()==4); }
TEST_CASE("S1.3 invalid vertices are rejected without changing later results", "[S1][boundary]") { TarjanScc g(3); REQUIRE_THROWS_AS(g.add_edge(-1,0),std::out_of_range); REQUIRE_THROWS_AS(g.add_edge(0,3),std::out_of_range); g.add_edge(0,1); g.add_edge(1,0); verify(g.run(),3,{{0,1},{1,0}}); }
TEST_CASE("S1.4 one directed cycle is one component regardless of insertion order", "[S1][structural]") { std::vector<std::pair<int,int>> e; for(int i=0;i<100;++i)e.push_back({i,(i+1)%100}); auto r=make(100,e).run(); verify(r,100,e); REQUIRE(r.components.size()==1); }
TEST_CASE("S1.5 a directed path has one component per vertex", "[S1][structural]") { std::vector<std::pair<int,int>> e; for(int i=0;i<200;++i)e.push_back({i,i+1}); auto r=make(201,e).run(); verify(r,201,e); REQUIRE(r.components.size()==201); }
TEST_CASE("S1.6 random graphs agree with mutual reachability reference", "[S1][stress]") { for(std::uint64_t t=0;t<100;++t){ALGO_TRIAL(t,701); Rng x(seed_); int n=2+x.below(18); std::vector<std::pair<int,int>> e; for(int i=0;i<n*n/2;++i)e.push_back({int(x.below(n)),int(x.below(n))}); verify(make(n,e).run(),n,e);} }
TEST_CASE("S1.7 duplicate edges do not change the partition or duplicate condensation edges", "[S1][structural]") { auto e=std::vector<std::pair<int,int>>{{0,1},{1,0},{1,2},{1,2},{1,2},{2,3},{3,2}}; auto r=make(4,e).run(); verify(r,4,e); REQUIRE(r.components.size()==2); REQUIRE(r.condensation_edges.size()==1); }
TEST_CASE("S1.8 independent SCC islands preserve every component", "[S1][structural]") { std::vector<std::pair<int,int>> e; for(int b=0;b<20;++b)for(int i=0;i<4;++i)e.push_back({4*b+i,4*b+(i+1)%4}); auto r=make(80,e).run(); verify(r,80,e); REQUIRE(r.components.size()==20); }
TEST_CASE("S2.1 every reported component is strongly connected", "[S2][invariant]") { auto e=std::vector<std::pair<int,int>>{{0,1},{1,0},{2,3},{3,4},{4,2},{1,2}}; verify(make(5,e).run(),5,e); }
TEST_CASE("S2.2 condensation graph is acyclic", "[S2][invariant]") { auto e=std::vector<std::pair<int,int>>{{0,1},{1,0},{1,2},{2,3},{3,2},{3,4}}; auto r=make(5,e).run(); verify(r,5,e); for(auto [a,b]:r.condensation_edges) REQUIRE(a!=b); }
TEST_CASE("S2.3 bridge direction changes partition only when it completes a cycle", "[S2][adversarial]") { auto a=std::vector<std::pair<int,int>>{{0,1},{1,0},{1,2},{2,3},{3,2}}; REQUIRE(make(4,a).run().components.size()==2); a.push_back({2,0}); REQUIRE(make(4,a).run().components.size()==1); }
TEST_CASE("S2.4 repeated run is deterministic and leaves the graph reusable", "[S2][invariant]") { TarjanScc g(4); for(auto e:std::vector<std::pair<int,int>>{{0,1},{1,0},{2,3}})g.add_edge(e.first,e.second); auto a=g.run(),b=g.run(); REQUIRE(a.component_of==b.component_of); REQUIRE(a.condensation_edges==b.condensation_edges); }
TEST_CASE("S2.5 shuffled vertex labels preserve partition cardinalities", "[S2][metamorphic]") { auto e=std::vector<std::pair<int,int>>{{0,1},{1,2},{2,0},{3,4},{4,3},{2,3}}; auto a=make(5,e).run(); std::vector<int> p{2,4,0,3,1}; std::vector<std::pair<int,int>> q; for(auto [u,v]:e)q.push_back({p[u],p[v]}); auto b=make(5,q).run(); REQUIRE(a.components.size()==b.components.size()); }
TEST_CASE("S2.6 large sparse path avoids recursive-stack failure", "[S2][scale]") { std::vector<std::pair<int,int>> e; for(int i=0;i<100000;++i)e.push_back({i,i+1}); auto r=make(100001,e).run(); REQUIRE(r.components.size()==100001); }
TEST_CASE("S2.7 large cycle is one SCC and has no condensation edge", "[S2][scale]") { std::vector<std::pair<int,int>> e; for(int i=0;i<100000;++i)e.push_back({i,(i+1)%100000}); auto r=make(100000,e).run(); REQUIRE(r.components.size()==1); REQUIRE(r.condensation_edges.empty()); }
TEST_CASE("S2.8 an edge maps to either one component or a reported condensation edge", "[S2][invariant]") { auto e=std::vector<std::pair<int,int>>{{0,1},{1,0},{1,2},{2,3},{3,2},{3,4}}; auto r=make(5,e).run(); verify(r,5,e); for(auto [u,v]:e) if(r.component_of[u]!=r.component_of[v]) REQUIRE(std::find(r.condensation_edges.begin(),r.condensation_edges.end(),std::pair{r.component_of[u],r.component_of[v]})!=r.condensation_edges.end()); }
