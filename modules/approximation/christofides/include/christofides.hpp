#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algo {

struct ChristofidesTour {
  std::vector<std::size_t> tour;
  double cost = 0.0;
};

inline ChristofidesTour christofides(const std::vector<std::vector<double>>& distance) {
  const std::size_t n = distance.size();
  if (n == 0) throw std::invalid_argument("distance matrix must be non-empty");
  for (std::size_t i = 0; i < n; ++i) {
    if (distance[i].size() != n) throw std::invalid_argument("distance matrix must be square");
    if (!std::isfinite(distance[i][i]) || std::abs(distance[i][i]) > 1e-9) throw std::invalid_argument("diagonal must be zero");
    for (std::size_t j = i + 1; j < n; ++j) {
      if (!std::isfinite(distance[i][j]) || distance[i][j] < 0 || std::abs(distance[i][j] - distance[j][i]) > 1e-9) throw std::invalid_argument("distance matrix must be finite, nonnegative and symmetric");
    }
  }
  if (n == 1) return {{0, 0}, 0.0};

  std::vector<bool> used(n, false);
  std::vector<std::pair<int,int>> tree;
  std::vector<double> best(n, std::numeric_limits<double>::infinity());
  std::vector<std::size_t> parent(n, 0); best[0] = 0;
  for (std::size_t it = 0; it < n; ++it) {
    std::size_t u = n;
    for (std::size_t v = 0; v < n; ++v) if (!used[v] && (u == n || best[v] < best[u])) u = v;
    used[u] = true;
    if (u != 0) tree.emplace_back(static_cast<int>(parent[u]), static_cast<int>(u));
    for (std::size_t v = 0; v < n; ++v) if (!used[v] && distance[u][v] < best[v]) { best[v] = distance[u][v]; parent[v] = u; }
  }

  std::vector<int> degree(n, 0); for (auto [u,v] : tree) { ++degree[u]; ++degree[v]; }
  std::vector<int> odd; for (std::size_t i=0;i<n;++i) if (degree[i]&1) odd.push_back(static_cast<int>(i));
  std::vector<std::pair<int,int>> matching;
  if (odd.size() <= 22) {
    const std::size_t states = std::size_t{1} << odd.size();
    std::vector<double> dp(states, std::numeric_limits<double>::infinity()); std::vector<int> choice(states, -1); dp[0]=0;
    for (std::size_t mask=0; mask<states; ++mask) if (std::isfinite(dp[mask]) && mask != states-1) {
      std::size_t i=0; while (mask & (std::size_t{1}<<i)) ++i;
      for (std::size_t j=i+1;j<odd.size();++j) if (!(mask & (std::size_t{1}<<j))) { const auto nm=mask|(std::size_t{1}<<i)|(std::size_t{1}<<j); const double val=dp[mask]+distance[odd[i]][odd[j]]; if(val<dp[nm]){dp[nm]=val; choice[nm]=static_cast<int>((i<<16)|j);} }
    }
    auto mask=states-1; while(mask){ const auto pair=choice[mask]; const auto i=static_cast<std::size_t>((pair>>16)&0xffff); const auto j=static_cast<std::size_t>(pair&0xffff); matching.emplace_back(odd[i],odd[j]); mask &= ~(std::size_t{1}<<i); mask &= ~(std::size_t{1}<<j); }
  } else {
    std::vector<bool> taken(odd.size(), false); for (std::size_t i=0;i<odd.size();++i) if(!taken[i]) { std::size_t bestj=odd.size(); double bd=std::numeric_limits<double>::infinity(); for(std::size_t k=i+1;k<odd.size();++k) if(!taken[k]&&distance[odd[i]][odd[k]]<bd){bd=distance[odd[i]][odd[k]];bestj=k;} if(bestj<odd.size()){taken[i]=taken[bestj]=true; matching.emplace_back(odd[i],odd[bestj]);} }
  }

  const std::size_t edge_count = tree.size() + matching.size();
  std::vector<std::vector<std::pair<int,std::size_t>>> graph(n); std::size_t eid=0;
  auto add_edge=[&](int u,int v){graph[u].emplace_back(v,eid); graph[v].emplace_back(u,eid); ++eid;};
  for (auto [u, v] : tree) add_edge(u, v);
  for (auto [u, v] : matching) add_edge(u, v);
  std::vector<bool> used_edge(edge_count,false); std::vector<std::size_t> stack{0}, euler;
  while(!stack.empty()){auto u=stack.back(); while(!graph[u].empty()&&used_edge[graph[u].back().second])graph[u].pop_back(); if(graph[u].empty()){euler.push_back(u);stack.pop_back();}else{auto [v,id]=graph[u].back();graph[u].pop_back();if(!used_edge[id]){used_edge[id]=true;stack.push_back(static_cast<std::size_t>(v));}}}
  std::reverse(euler.begin(),euler.end()); std::vector<bool> seen(n,false); ChristofidesTour result; result.tour.reserve(n+1); for(auto v:euler)if(!seen[v]){seen[v]=true;result.tour.push_back(v);} result.tour.push_back(0);
  for(std::size_t i=1;i<result.tour.size();++i)result.cost+=distance[result.tour[i-1]][result.tour[i]];
  return result;
}

}  // namespace algo
