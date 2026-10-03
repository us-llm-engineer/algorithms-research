#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace algo {

class MinCostFlow {
 public:
  using Value = std::int64_t;
  struct Result {
    Value flow = 0;
    Value cost = 0;
    std::vector<Value> edge_flows;
    std::vector<Value> potentials;
  };

  explicit MinCostFlow(int vertices) : n_(vertices) {
    if (vertices < 0) throw std::invalid_argument("vertex count must be nonnegative");
  }

  void add_edge(int u, int v, Value capacity, Value cost) {
    check_vertex(u);
    check_vertex(v);
    if (capacity < 0) throw std::invalid_argument("capacity must be nonnegative");
    edges_.emplace_back(u, v, capacity, cost);
  }

  Result min_cost_flow(int source, int sink, Value requested) const {
    check_vertex(source);
    check_vertex(sink);
    if (source == sink) throw std::invalid_argument("source and sink must differ");
    if (requested < 0) throw std::invalid_argument("requested flow must be nonnegative");

    struct Arc { int to, rev; Value cap, cost; int original; };
    std::vector<std::vector<Arc>> g(static_cast<std::size_t>(n_));
    auto add_arc = [&](int u, int v, Value cap, Value cost, int id) {
      int fi = static_cast<int>(g[static_cast<std::size_t>(u)].size());
      int ri = static_cast<int>(g[static_cast<std::size_t>(v)].size());
      if (u == v) {
        g[static_cast<std::size_t>(u)].push_back({v, fi + 1, cap, cost, id});
        g[static_cast<std::size_t>(u)].push_back({u, fi, 0, -cost, -1});
      } else {
        g[static_cast<std::size_t>(u)].push_back({v, ri, cap, cost, id});
        g[static_cast<std::size_t>(v)].push_back({u, fi, 0, -cost, -1});
      }
    };
    for (std::size_t i = 0; i < edges_.size(); ++i) {
      const auto [u, v, cap, cost] = edges_[i];
      add_arc(u, v, cap, cost, static_cast<int>(i));
    }

    const Value inf = std::numeric_limits<Value>::max();
    std::vector<Value> potential(static_cast<std::size_t>(n_), 0);
    // Bellman-Ford establishes feasible reduced costs when input costs are negative.
    std::vector<Value> initial(static_cast<std::size_t>(n_), inf);
    initial[static_cast<std::size_t>(source)] = 0;
    for (int pass = 0; pass < n_ - 1; ++pass) {
      bool changed = false;
      for (int u = 0; u < n_; ++u) {
        Value du = initial[static_cast<std::size_t>(u)];
        if (du == inf) continue;
        for (const Arc& a : g[static_cast<std::size_t>(u)]) {
          if (a.cap <= 0 || a.original < 0 || a.to == u) continue;
          Value nd = checked_add(du, a.cost);
          if (nd < initial[static_cast<std::size_t>(a.to)]) {
            initial[static_cast<std::size_t>(a.to)] = nd;
            changed = true;
          }
        }
      }
      if (!changed) break;
    }
    for (int v = 0; v < n_; ++v) if (initial[static_cast<std::size_t>(v)] != inf) potential[static_cast<std::size_t>(v)] = initial[static_cast<std::size_t>(v)];

    Result result;
    result.edge_flows.assign(edges_.size(), 0);
    while (result.flow < requested) {
      std::vector<Value> dist(static_cast<std::size_t>(n_), inf);
      std::vector<int> prev_v(static_cast<std::size_t>(n_), -1), prev_e(static_cast<std::size_t>(n_), -1);
      using QueueItem = std::pair<Value, int>;
      std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pq;
      dist[static_cast<std::size_t>(source)] = 0;
      pq.push({0, source});
      while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d != dist[static_cast<std::size_t>(u)]) continue;
        const auto& adj = g[static_cast<std::size_t>(u)];
        for (std::size_t i = 0; i < adj.size(); ++i) {
          const Arc& a = adj[i];
          if (a.cap <= 0 || a.to == u) continue;
          Value reduced = checked_add(a.cost, checked_add(potential[static_cast<std::size_t>(u)], -potential[static_cast<std::size_t>(a.to)]));
          if (reduced < 0) throw std::logic_error("negative reduced cost");
          Value nd = checked_add(d, reduced);
          if (nd < dist[static_cast<std::size_t>(a.to)]) {
            dist[static_cast<std::size_t>(a.to)] = nd;
            prev_v[static_cast<std::size_t>(a.to)] = u;
            prev_e[static_cast<std::size_t>(a.to)] = static_cast<int>(i);
            pq.push({nd, a.to});
          }
        }
      }
      if (dist[static_cast<std::size_t>(sink)] == inf) break;
      for (int v = 0; v < n_; ++v) if (dist[static_cast<std::size_t>(v)] != inf) potential[static_cast<std::size_t>(v)] = checked_add(potential[static_cast<std::size_t>(v)], dist[static_cast<std::size_t>(v)]);
      Value amount = requested - result.flow;
      for (int v = sink; v != source; v = prev_v[static_cast<std::size_t>(v)]) {
        if (v < 0 || prev_v[static_cast<std::size_t>(v)] < 0) throw std::logic_error("broken augmenting path");
        const Arc& a = g[static_cast<std::size_t>(prev_v[static_cast<std::size_t>(v)])][static_cast<std::size_t>(prev_e[static_cast<std::size_t>(v)])];
        amount = std::min(amount, a.cap);
      }
      Value path_cost = 0;
      for (int v = sink; v != source; v = prev_v[static_cast<std::size_t>(v)]) {
        int u = prev_v[static_cast<std::size_t>(v)], ei = prev_e[static_cast<std::size_t>(v)];
        Arc& a = g[static_cast<std::size_t>(u)][static_cast<std::size_t>(ei)];
        path_cost = checked_add(path_cost, a.cost);
        a.cap -= amount;
        g[static_cast<std::size_t>(v)][static_cast<std::size_t>(a.rev)].cap = checked_add(g[static_cast<std::size_t>(v)][static_cast<std::size_t>(a.rev)].cap, amount);
        if (a.original >= 0) result.edge_flows[static_cast<std::size_t>(a.original)] = checked_add(result.edge_flows[static_cast<std::size_t>(a.original)], amount);
        else result.edge_flows[static_cast<std::size_t>(g[static_cast<std::size_t>(v)][static_cast<std::size_t>(a.rev)].original)] -= amount;
      }
      result.flow = checked_add(result.flow, amount);
      result.cost = checked_add(result.cost, checked_mul(amount, path_cost));
    }
    result.potentials = std::move(potential);
    return result;
  }

  bool check_certificate(int source, int sink, const Result& r) const {
    check_vertex(source); check_vertex(sink);
    if (source == sink || r.edge_flows.size() != edges_.size() || r.flow < 0) return false;
    std::vector<Value> balance(static_cast<std::size_t>(n_), 0);
    Value cost = 0;
    for (std::size_t i = 0; i < edges_.size(); ++i) {
      const auto [u, v, cap, weight] = edges_[i];
      Value f = r.edge_flows[i];
      if (f < 0 || f > cap) return false;
      if (u != v) {
        balance[static_cast<std::size_t>(u)] = checked_add(balance[static_cast<std::size_t>(u)], -f);
        balance[static_cast<std::size_t>(v)] = checked_add(balance[static_cast<std::size_t>(v)], f);
      }
      cost = checked_add(cost, checked_mul(f, weight));
    }
    if (cost != r.cost || balance[static_cast<std::size_t>(source)] != -r.flow || balance[static_cast<std::size_t>(sink)] != r.flow) return false;
    for (int v = 0; v < n_; ++v) if (v != source && v != sink && balance[static_cast<std::size_t>(v)] != 0) return false;
    return true;
  }

  bool reduced_costs_nonnegative(const Result& r) const {
    if (r.potentials.size() != static_cast<std::size_t>(n_) || r.edge_flows.size() != edges_.size()) return false;
    // Check each residual direction with positive residual capacity.
    for (std::size_t i = 0; i < edges_.size(); ++i) {
      const auto [u, v, cap, cost] = edges_[i];
      Value f = r.edge_flows[i];
      if (f < 0 || f > cap) return false;
      if (cap > f && checked_add(cost, checked_add(r.potentials[static_cast<std::size_t>(u)], -r.potentials[static_cast<std::size_t>(v)])) < 0) return false;
      if (f > 0 && checked_add(-cost, checked_add(r.potentials[static_cast<std::size_t>(v)], -r.potentials[static_cast<std::size_t>(u)])) < 0) return false;
    }
    return true;
  }

 private:
  int n_;
  std::vector<std::tuple<int, int, Value, Value>> edges_;

  void check_vertex(int v) const { if (v < 0 || v >= n_) throw std::out_of_range("vertex index"); }
  static Value checked_add(Value a, Value b) {
    if ((b > 0 && a > std::numeric_limits<Value>::max() - b) || (b < 0 && a < std::numeric_limits<Value>::min() - b)) throw std::overflow_error("min-cost flow arithmetic overflow");
    return a + b;
  }
  static Value checked_mul(Value a, Value b) {
    if (a == 0 || b == 0) return 0;
    if ((a == -1 && b == std::numeric_limits<Value>::min()) || (b == -1 && a == std::numeric_limits<Value>::min())) throw std::overflow_error("min-cost flow arithmetic overflow");
    if (a > 0 ? (b > 0 ? a > std::numeric_limits<Value>::max() / b : b < std::numeric_limits<Value>::min() / a)
              : (b > 0 ? a < std::numeric_limits<Value>::min() / b : a < std::numeric_limits<Value>::max() / b)) throw std::overflow_error("min-cost flow arithmetic overflow");
    return a * b;
  }
};

}  // namespace algo
