// Contract for a 0-indexed bipartite matcher: maximum_matching(), match arrays,
// and a Konig minimum vertex cover certificate.
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <queue>
#include <stdexcept>
#include <vector>
#include "hopcroft_karp.hpp"
#include "test_support.hpp"

using algo::HopcroftKarp; using algo::Rng;
namespace {
HopcroftKarp make(int l,int r,const std::vector<std::pair<int,int>>& e){HopcroftKarp h(l,r);for(auto [u,v]:e)h.add_edge(u,v);return h;}
int brute(int l,int r,const std::vector<std::pair<int,int>>& e){std::vector<std::vector<int>> a(l);for(auto [u,v]:e)a[u].push_back(v);std::vector<int> mr(r,-1);std::function<bool(int,std::vector<char>&)> go=[&](int u,std::vector<char>& seen){for(int v:a[u])if(!seen[v]){seen[v]=1;if(mr[v]<0||go(mr[v],seen)){mr[v]=u;return true;}}return false;};int ans=0;for(int u=0;u<l;++u){std::vector<char>s(r);if(go(u,s))++ans;}return ans;}
void verify(HopcroftKarp& h,int l,int r,const std::vector<std::pair<int,int>>& e){int n=h.maximum_matching();auto lm=h.left_match(),rm=h.right_match();REQUIRE(lm.size()==std::size_t(l));REQUIRE(rm.size()==std::size_t(r));int c=0;for(int u=0;u<l;++u)if(lm[u]>=0){++c;REQUIRE(lm[u]<r);REQUIRE(rm[lm[u]]==u);REQUIRE(std::find(e.begin(),e.end(),std::pair{u,lm[u]})!=e.end());}REQUIRE(c==n);auto cover=h.min_vertex_cover();REQUIRE(cover.left.size()==std::size_t(l));REQUIRE(cover.right.size()==std::size_t(r));int cs=std::count(cover.left.begin(),cover.left.end(),true)+std::count(cover.right.begin(),cover.right.end(),true);REQUIRE(cs==n);for(auto [u,v]:e)REQUIRE((cover.left[u]||cover.right[v]));}
}
TEST_CASE("S1.1 textbook matching has its documented cardinality", "[S1][given]"){auto e=std::vector<std::pair<int,int>>{{0,0},{0,1},{1,1},{2,1},{2,2}};auto h=make(3,3,e);verify(h,3,3,e);REQUIRE(h.maximum_matching()==3);}
TEST_CASE("S1.2 empty sides, isolated vertices, duplicates and self-indexed edges", "[S1][boundary]"){auto a=make(0,3,{});verify(a,0,3,{});auto b=make(3,0,{});verify(b,3,0,{});auto e=std::vector<std::pair<int,int>>{{0,0},{0,0},{2,2}};auto h=make(4,4,e);verify(h,4,4,e);REQUIRE(h.maximum_matching()==2);}
TEST_CASE("S1.3 invalid endpoints are rejected and leave matching usable", "[S1][boundary]"){HopcroftKarp h(2,2);REQUIRE_THROWS_AS(h.add_edge(-1,0),std::out_of_range);REQUIRE_THROWS_AS(h.add_edge(0,2),std::out_of_range);h.add_edge(0,1);verify(h,2,2,{{0,1}});}
TEST_CASE("S1.4 complete balanced bipartite graph matches every left vertex", "[S1][structural]"){std::vector<std::pair<int,int>>e;for(int u=0;u<20;++u)for(int v=0;v<20;++v)e.push_back({u,v});auto h=make(20,20,e);verify(h,20,20,e);REQUIRE(h.maximum_matching()==20);}
TEST_CASE("S1.5 Hall-deficient graph exposes the exact deficiency", "[S1][structural]"){auto e=std::vector<std::pair<int,int>>{{0,0},{1,0},{2,0},{3,1}};auto h=make(4,2,e);verify(h,4,2,e);REQUIRE(h.maximum_matching()==2);}
TEST_CASE("S1.6 random small graphs agree with brute-force augmenting paths", "[S1][stress]"){for(std::uint64_t t=0;t<200;++t){ALGO_TRIAL(t,801);Rng x(seed_);int l=1+x.below(8),r=1+x.below(8);std::vector<std::pair<int,int>>e;for(int i=0;i<l*r;++i)if(x.coin(.4))e.push_back({int(x.below(l)),int(x.below(r))});auto h=make(l,r,e);verify(h,l,r,e);REQUIRE(h.maximum_matching()==brute(l,r,e));}}
TEST_CASE("S1.7 augmenting path reassignment reaches a non-greedy optimum", "[S1][adversarial]"){auto e=std::vector<std::pair<int,int>>{{0,0},{0,1},{1,0}};auto h=make(2,2,e);verify(h,2,2,e);REQUIRE(h.maximum_matching()==2);}
TEST_CASE("S1.8 repeated calls are idempotent and edges may be added between calls", "[S1][invariant]"){HopcroftKarp h(3,3);h.add_edge(0,0);REQUIRE(h.maximum_matching()==1);REQUIRE(h.maximum_matching()==1);h.add_edge(1,1);h.add_edge(2,2);REQUIRE(h.maximum_matching()==3);}
TEST_CASE("S2.1 match arrays are symmetric and every matched edge exists", "[S2][invariant]"){auto e=std::vector<std::pair<int,int>>{{0,1},{1,0},{1,2},{2,1}};auto h=make(3,3,e);verify(h,3,3,e);}
TEST_CASE("S2.2 Konig cover size equals matching size and covers every edge", "[S2][invariant]"){auto e=std::vector<std::pair<int,int>>{{0,0},{0,1},{1,1},{2,1},{2,2},{3,2}};auto h=make(4,3,e);verify(h,4,3,e);}
TEST_CASE("S2.3 permuting left labels preserves cardinality", "[S2][metamorphic]"){auto e=std::vector<std::pair<int,int>>{{0,0},{0,2},{1,1},{2,0},{3,2}};auto a=make(4,3,e).maximum_matching();std::vector<int>p{2,0,3,1};for(auto&x:e)x.first=p[x.first];REQUIRE(make(4,3,e).maximum_matching()==a);}
TEST_CASE("S2.4 alternating-layer adversary requires multiple BFS phases", "[S2][adversarial]"){std::vector<std::pair<int,int>>e;for(int i=0;i<200;++i){e.push_back({i,i});if(i)e.push_back({i,i-1});}auto h=make(200,200,e);verify(h,200,200,e);REQUIRE(h.maximum_matching()==200);}
TEST_CASE("S2.5 sparse random 1000 by 1000 graph matches a flow-sized lower bound", "[S2][scale]"){Rng x(9);std::vector<std::pair<int,int>>e;for(int u=0;u<1000;++u)for(int k=0;k<4;++k)e.push_back({u,int(x.below(1000))});auto h=make(1000,1000,e);verify(h,1000,1000,e);}
TEST_CASE("S2.6 unbalanced graph never matches more than the smaller partition", "[S2][boundary]"){std::vector<std::pair<int,int>>e;for(int u=0;u<100;++u)for(int v=0;v<3;++v)e.push_back({u,v});auto h=make(100,3,e);verify(h,100,3,e);REQUIRE(h.maximum_matching()==3);}
TEST_CASE("S2.7 adding edges cannot reduce maximum cardinality", "[S2][metamorphic]"){auto e=std::vector<std::pair<int,int>>{{0,0},{1,0}};auto a=make(2,2,e).maximum_matching();e.push_back({1,1});REQUIRE(make(2,2,e).maximum_matching()>=a);}
TEST_CASE("S2.8 a returned cover remains valid after a fresh recomputation", "[S2][invariant]"){auto e=std::vector<std::pair<int,int>>{{0,0},{1,0},{1,1},{2,1}};auto h=make(3,2,e);verify(h,3,2,e);verify(h,3,2,e);}
