#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algo {

class MaxFlow {
 public:
  using Cap = std::int64_t;
  enum class Algorithm { FifoPushRelabel, HighestLabelPushRelabel, EdmondsKarp, Dinic };
  struct Options { bool gap_heuristic = true; bool global_relabel = true; };
  struct Stats {
    std::uint64_t pushes = 0;
    std::uint64_t saturating_pushes = 0;
    std::uint64_t relabels = 0;
    std::uint64_t augmentations = 0;
    std::uint64_t phases = 0;
  };

  explicit MaxFlow(int vertices) : n_(vertices) {
    if (vertices < 0) throw std::invalid_argument("vertex count must be nonnegative");
  }

  void add_edge(int u, int v, Cap capacity) {
    check_vertex(u);
    check_vertex(v);
    if (capacity < 0) throw std::invalid_argument("capacity must be nonnegative");
    edges_.push_back({u, v, capacity});
    flows_.push_back(0);
  }

  int num_vertices() const noexcept { return n_; }
  int num_edges() const noexcept { return static_cast<int>(edges_.size()); }
  Cap flow_on(int edge) const {
    if (edge < 0 || static_cast<std::size_t>(edge) >= flows_.size()) throw std::out_of_range("edge index");
    return flows_[static_cast<std::size_t>(edge)];
  }
  void set_options(Options options) noexcept { options_ = options; }
  const Stats& stats() const noexcept { return stats_; }
  const std::vector<bool>& min_cut_source_side() const noexcept { return cut_side_; }

  Cap max_flow(int source, int sink, Algorithm algorithm) {
    check_vertex(source);
    check_vertex(sink);
    if (source == sink) throw std::invalid_argument("source and sink must differ");
    Cap source_capacity = 0;
    for (const Edge& e : edges_) if (e.u == source) source_capacity = checked_add(source_capacity, e.capacity);
    (void)source_capacity;
    stats_ = {};
    flows_.assign(edges_.size(), 0);
    Residual g(static_cast<std::size_t>(n_));
    refs_.clear();
    refs_.reserve(edges_.size());
    for (std::size_t i = 0; i < edges_.size(); ++i) {
      const auto& e = edges_[i];
      refs_.push_back(g.add(e.u, e.v, e.capacity, static_cast<int>(i)));
    }
    Cap value = 0;
    switch (algorithm) {
      case Algorithm::FifoPushRelabel: value = push_relabel(g, source, sink, false); break;
      case Algorithm::HighestLabelPushRelabel: value = push_relabel(g, source, sink, true); break;
      case Algorithm::EdmondsKarp: value = edmonds_karp(g, source, sink); break;
      case Algorithm::Dinic: value = dinic(g, source, sink); break;
      default: throw std::invalid_argument("unknown max-flow algorithm");
    }
    for (std::size_t i = 0; i < refs_.size(); ++i) {
      const auto [u, a] = refs_[i];
      flows_[i] = edges_[i].capacity - g.adj[static_cast<std::size_t>(u)][static_cast<std::size_t>(a)].cap;
    }
    cut_side_.assign(static_cast<std::size_t>(n_), false);
    std::queue<int> q;
    cut_side_[static_cast<std::size_t>(source)] = true;
    q.push(source);
    while (!q.empty()) {
      const int u = q.front(); q.pop();
      for (const Arc& a : g.adj[static_cast<std::size_t>(u)]) if (a.cap > 0 && !cut_side_[static_cast<std::size_t>(a.to)]) {
        cut_side_[static_cast<std::size_t>(a.to)] = true;
        q.push(a.to);
      }
    }
    return value;
  }

  void check_certificate(int source, int sink, Cap value) const {
    check_vertex(source); check_vertex(sink);
    std::vector<Cap> net(static_cast<std::size_t>(n_), 0);
    for (std::size_t i = 0; i < edges_.size(); ++i) {
      const Edge& e = edges_[i]; const Cap f = flows_[i];
      if (f < 0 || f > e.capacity) throw std::logic_error("capacity certificate failed");
      if (e.u == e.v) continue;
      net[static_cast<std::size_t>(e.u)] = checked_add(net[static_cast<std::size_t>(e.u)], -f);
      net[static_cast<std::size_t>(e.v)] = checked_add(net[static_cast<std::size_t>(e.v)], f);
    }
    for (int v = 0; v < n_; ++v) if (v != source && v != sink && net[static_cast<std::size_t>(v)] != 0) throw std::logic_error("flow conservation failed");
    if (net[static_cast<std::size_t>(source)] != -value || net[static_cast<std::size_t>(sink)] != value) throw std::logic_error("flow value certificate failed");
    if (cut_side_.size() != static_cast<std::size_t>(n_) || !cut_side_[static_cast<std::size_t>(source)] || cut_side_[static_cast<std::size_t>(sink)]) throw std::logic_error("cut certificate failed");
    Cap cut = 0;
    for (const Edge& e : edges_) if (cut_side_[static_cast<std::size_t>(e.u)] && !cut_side_[static_cast<std::size_t>(e.v)]) cut = checked_add(cut, e.capacity);
    if (cut != value) throw std::logic_error("max-flow/min-cut certificate failed");
  }

  void check_labeling() const {
    if (last_heights_.size() != static_cast<std::size_t>(n_)) throw std::logic_error("no push-relabel labeling available");
    if (last_source_ < 0 || last_sink_ < 0 || last_heights_[static_cast<std::size_t>(last_source_)] != n_ || last_heights_[static_cast<std::size_t>(last_sink_)] != 0) throw std::logic_error("terminal labels invalid");
    for (int u = 0; u < n_; ++u) {
      int h = last_heights_[static_cast<std::size_t>(u)];
      if (h < 0 || h >= 2 * n_) throw std::logic_error("height out of range");
    }
    for (const auto& e : last_residual_) if (e.cap > 0 && last_heights_[static_cast<std::size_t>(e.u)] > last_heights_[static_cast<std::size_t>(e.v)] + 1) throw std::logic_error("invalid residual labeling");
  }

 private:
  struct Edge { int u, v; Cap capacity; };
  struct Arc { int to, rev; Cap cap; int original; };
  struct Residual {
    explicit Residual(std::size_t n) : adj(n) {}
    std::vector<std::vector<Arc>> adj;
    std::pair<int, int> add(int u, int v, Cap cap, int original) {
      auto& au = adj[static_cast<std::size_t>(u)]; auto& av = adj[static_cast<std::size_t>(v)];
      int fi = static_cast<int>(au.size()), ri = static_cast<int>(av.size());
      if (u == v) { au.push_back({v, fi + 1, cap, original}); au.push_back({u, fi, 0, -1}); }
      else { au.push_back({v, ri, cap, original}); av.push_back({u, fi, 0, -1}); }
      return {u, fi};
    }
  };
  static Cap checked_add(Cap a, Cap b) {
    if ((b > 0 && a > std::numeric_limits<Cap>::max() - b) || (b < 0 && a < std::numeric_limits<Cap>::min() - b)) throw std::overflow_error("capacity or flow overflow");
    return a + b;
  }
  void check_vertex(int v) const { if (v < 0 || v >= n_) throw std::out_of_range("vertex index"); }
  static void augment(Residual& g, int u, int i, Cap amount) {
    Arc& a = g.adj[static_cast<std::size_t>(u)][static_cast<std::size_t>(i)];
    const int v = a.to, r = a.rev;
    a.cap -= amount;
    g.adj[static_cast<std::size_t>(v)][static_cast<std::size_t>(r)].cap += amount;
  }

  Cap push_relabel(Residual& g, int s, int t, bool highest) {
    const int n = n_, maxh = std::max(1, 2 * n);
    std::vector<int> h(static_cast<std::size_t>(n), 0), cur(static_cast<std::size_t>(n), 0), count(static_cast<std::size_t>(maxh + 1), 0);
    std::vector<Cap> excess(static_cast<std::size_t>(n), 0);
    std::vector<bool> active(static_cast<std::size_t>(n), false);
    std::queue<int> fifo;
    std::vector<std::vector<int>> buckets(static_cast<std::size_t>(maxh + 1));
    int highest_h = 0;
    auto enqueue = [&](int v) {
      if (v == s || v == t || excess[static_cast<std::size_t>(v)] <= 0 || active[static_cast<std::size_t>(v)]) return;
      active[static_cast<std::size_t>(v)] = true;
      if (highest) { buckets[static_cast<std::size_t>(h[static_cast<std::size_t>(v)])].push_back(v); highest_h = std::max(highest_h, h[static_cast<std::size_t>(v)]); }
      else fifo.push(v);
    };
    h[static_cast<std::size_t>(s)] = n;
    std::fill(count.begin(), count.end(), 0);
    count[0] = n - 1; count[static_cast<std::size_t>(n)] = 1;
    for (std::size_t i = 0; i < g.adj[static_cast<std::size_t>(s)].size(); ++i) {
      Arc& a = g.adj[static_cast<std::size_t>(s)][i];
      if (a.to == s || a.cap == 0) continue;
      Cap amount = a.cap; a.cap = 0;
      g.adj[static_cast<std::size_t>(a.to)][static_cast<std::size_t>(a.rev)].cap += amount;
      excess[static_cast<std::size_t>(s)] -= amount;
      excess[static_cast<std::size_t>(a.to)] = checked_add(excess[static_cast<std::size_t>(a.to)], amount);
      enqueue(a.to);
    }
    auto global_relabel = [&]() {
      std::fill(h.begin(), h.end(), n + 1);
      std::queue<int> q;
      h[static_cast<std::size_t>(t)] = 0; q.push(t);
      while (!q.empty()) {
        int v = q.front(); q.pop();
        for (const Arc& rev : g.adj[static_cast<std::size_t>(v)]) {
          int u = rev.to;
          const Arc& forward = g.adj[static_cast<std::size_t>(u)][static_cast<std::size_t>(rev.rev)];
          if (forward.cap > 0 && h[static_cast<std::size_t>(u)] == n + 1 && u != s) { h[static_cast<std::size_t>(u)] = h[static_cast<std::size_t>(v)] + 1; q.push(u); }
        }
      }
      h[static_cast<std::size_t>(s)] = n;
      std::fill(count.begin(), count.end(), 0);
      for (int u = 0; u < n; ++u) ++count[static_cast<std::size_t>(h[static_cast<std::size_t>(u)])];
      std::fill(cur.begin(), cur.end(), 0);
      while (!fifo.empty()) fifo.pop();
      for (auto& b : buckets) b.clear();
      std::fill(active.begin(), active.end(), false);
      highest_h = 0;
      for (int u = 0; u < n; ++u) enqueue(u);
    };
    if (options_.global_relabel) global_relabel();
    auto pop_active = [&]() -> int {
      if (!highest) { while (!fifo.empty()) { int v = fifo.front(); fifo.pop(); if (active[static_cast<std::size_t>(v)]) return v; } return -1; }
      while (highest_h >= 0) {
        auto& b = buckets[static_cast<std::size_t>(highest_h)];
        while (!b.empty()) { int v = b.back(); b.pop_back(); if (active[static_cast<std::size_t>(v)]) return v; }
        --highest_h;
      }
      return -1;
    };
    while (true) {
      int u = pop_active(); if (u < 0) break;
      active[static_cast<std::size_t>(u)] = false;
      while (excess[static_cast<std::size_t>(u)] > 0) {
        auto& adj = g.adj[static_cast<std::size_t>(u)];
        if (cur[static_cast<std::size_t>(u)] == static_cast<int>(adj.size())) {
          const int old = h[static_cast<std::size_t>(u)];
          int nh = maxh;
          for (const Arc& a : adj) if (a.cap > 0) nh = std::min(nh, h[static_cast<std::size_t>(a.to)] + 1);
          if (nh >= maxh) { // stranded excess can only be returned to the source
            nh = n + 1;
            for (const Arc& a : adj) if (a.cap > 0) nh = std::min(nh, h[static_cast<std::size_t>(a.to)] + 1);
          }
          if (nh > maxh) break;
          --count[static_cast<std::size_t>(old)];
          h[static_cast<std::size_t>(u)] = nh; ++stats_.relabels; ++count[static_cast<std::size_t>(nh)];
          cur[static_cast<std::size_t>(u)] = 0;
          if (options_.gap_heuristic && old < n && count[static_cast<std::size_t>(old)] == 0) {
            for (int v = 0; v < n; ++v) if (v != s && v != t && v != u && h[static_cast<std::size_t>(v)] > old && h[static_cast<std::size_t>(v)] < n) {
              --count[static_cast<std::size_t>(h[static_cast<std::size_t>(v)])]; h[static_cast<std::size_t>(v)] = n + 1; ++count[static_cast<std::size_t>(n + 1)]; cur[static_cast<std::size_t>(v)] = 0;
            }
          }
          continue;
        }
        Arc& a = adj[static_cast<std::size_t>(cur[static_cast<std::size_t>(u)])];
        if (a.cap > 0 && h[static_cast<std::size_t>(u)] == h[static_cast<std::size_t>(a.to)] + 1) {
          Cap delta = std::min(excess[static_cast<std::size_t>(u)], a.cap);
          const bool sat = delta == a.cap;
          augment(g, u, cur[static_cast<std::size_t>(u)], delta);
          excess[static_cast<std::size_t>(u)] -= delta;
          excess[static_cast<std::size_t>(a.to)] = checked_add(excess[static_cast<std::size_t>(a.to)], delta);
          ++stats_.pushes; if (sat) ++stats_.saturating_pushes;
          enqueue(a.to);
        } else ++cur[static_cast<std::size_t>(u)];
      }
      enqueue(u);
    }
    last_heights_ = h; last_source_ = s; last_sink_ = t;
    last_residual_.clear();
    for (int u = 0; u < n; ++u) for (const Arc& a : g.adj[static_cast<std::size_t>(u)]) last_residual_.push_back({u, a.to, a.cap});
    return excess[static_cast<std::size_t>(t)];
  }

  Cap edmonds_karp(Residual& g, int s, int t) {
    Cap total = 0;
    for (;;) {
      std::vector<int> pu(static_cast<std::size_t>(n_), -1), pi(static_cast<std::size_t>(n_), -1);
      std::queue<int> q; q.push(s); pu[static_cast<std::size_t>(s)] = s;
      while (!q.empty() && pu[static_cast<std::size_t>(t)] < 0) {
        int u = q.front(); q.pop();
        for (int i = 0; i < static_cast<int>(g.adj[static_cast<std::size_t>(u)].size()); ++i) { const Arc& a = g.adj[static_cast<std::size_t>(u)][static_cast<std::size_t>(i)]; if (a.cap > 0 && pu[static_cast<std::size_t>(a.to)] < 0) { pu[static_cast<std::size_t>(a.to)] = u; pi[static_cast<std::size_t>(a.to)] = i; q.push(a.to); } }
      }
      if (pu[static_cast<std::size_t>(t)] < 0) return total;
      Cap d = std::numeric_limits<Cap>::max();
      for (int v = t; v != s; v = pu[static_cast<std::size_t>(v)]) d = std::min(d, g.adj[static_cast<std::size_t>(pu[static_cast<std::size_t>(v)])][static_cast<std::size_t>(pi[static_cast<std::size_t>(v)])].cap);
      for (int v = t; v != s; v = pu[static_cast<std::size_t>(v)]) augment(g, pu[static_cast<std::size_t>(v)], pi[static_cast<std::size_t>(v)], d);
      total = checked_add(total, d); ++stats_.augmentations;
    }
  }
  Cap dinic(Residual& g, int s, int t) {
    Cap total = 0;
    for (;;) {
      std::vector<int> level(static_cast<std::size_t>(n_), -1); std::queue<int> q; level[static_cast<std::size_t>(s)] = 0; q.push(s);
      while (!q.empty()) { int u = q.front(); q.pop(); for (const Arc& a : g.adj[static_cast<std::size_t>(u)]) if (a.cap > 0 && level[static_cast<std::size_t>(a.to)] < 0) { level[static_cast<std::size_t>(a.to)] = level[static_cast<std::size_t>(u)] + 1; q.push(a.to); } }
      if (level[static_cast<std::size_t>(t)] < 0) return total;
      ++stats_.phases;
      std::vector<int> it(static_cast<std::size_t>(n_), 0);
      auto dfs = [&](auto&& self, int u, Cap f) -> Cap {
        if (u == t) return f;
        auto& adj = g.adj[static_cast<std::size_t>(u)];
        for (int& i = it[static_cast<std::size_t>(u)]; i < static_cast<int>(adj.size()); ++i) {
          Arc& a = adj[static_cast<std::size_t>(i)];
          if (a.cap > 0 && level[static_cast<std::size_t>(a.to)] == level[static_cast<std::size_t>(u)] + 1) {
            Cap d = self(self, a.to, std::min(f, a.cap));
            if (d > 0) { augment(g, u, i, d); return d; }
          }
        }
        return 0;
      };
      for (;;) { Cap d = dfs(dfs, s, std::numeric_limits<Cap>::max()); if (d == 0) break; total = checked_add(total, d); ++stats_.augmentations; }
    }
  }

  int n_;
  std::vector<Edge> edges_;
  std::vector<Cap> flows_;
  std::vector<std::pair<int, int>> refs_;
  std::vector<bool> cut_side_;
  std::vector<int> last_heights_;
  struct LabelArc { int u, v; Cap cap; };
  std::vector<LabelArc> last_residual_;
  int last_source_ = -1, last_sink_ = -1;
  Options options_;
  Stats stats_;
};

}  // namespace algo
