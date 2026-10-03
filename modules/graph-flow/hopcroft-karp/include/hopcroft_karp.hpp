#pragma once

#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algo {

class HopcroftKarp {
 public:
  struct VertexCover {
    std::vector<bool> left;
    std::vector<bool> right;
  };

  HopcroftKarp(int left_size, int right_size) {
    if (left_size < 0 || right_size < 0) {
      throw std::invalid_argument("partition sizes must be non-negative");
    }
    graph_.resize(static_cast<std::size_t>(left_size));
    left_match_.assign(static_cast<std::size_t>(left_size), -1);
    right_match_.assign(static_cast<std::size_t>(right_size), -1);
    distance_.resize(static_cast<std::size_t>(left_size));
  }

  void add_edge(int left, int right) {
    if (left < 0 || static_cast<std::size_t>(left) >= graph_.size() || right < 0 ||
        static_cast<std::size_t>(right) >= right_match_.size()) {
      throw std::out_of_range("bipartite edge endpoint out of range");
    }
    graph_[static_cast<std::size_t>(left)].push_back(right);
  }

  int maximum_matching() {
    int added = 0;
    while (build_layers()) {
      for (std::size_t left = 0; left < graph_.size(); ++left) {
        if (left_match_[left] == -1 && augment(static_cast<int>(left))) {
          ++added;
        }
      }
    }
    matching_size_ += added;
    return matching_size_;
  }

  const std::vector<int>& left_match() const noexcept { return left_match_; }
  const std::vector<int>& right_match() const noexcept { return right_match_; }

  VertexCover min_vertex_cover() {
    maximum_matching();
    std::vector<bool> reachable_left(graph_.size(), false);
    std::vector<bool> reachable_right(right_match_.size(), false);
    std::queue<int> pending;
    for (std::size_t u = 0; u < graph_.size(); ++u) {
      if (left_match_[u] == -1) {
        reachable_left[u] = true;
        pending.push(static_cast<int>(u));
      }
    }

    while (!pending.empty()) {
      const int u = pending.front();
      pending.pop();
      for (int v : graph_[static_cast<std::size_t>(u)]) {
        if (left_match_[static_cast<std::size_t>(u)] == v ||
            reachable_right[static_cast<std::size_t>(v)]) {
          continue;
        }
        reachable_right[static_cast<std::size_t>(v)] = true;
        const int mate = right_match_[static_cast<std::size_t>(v)];
        if (mate != -1 && !reachable_left[static_cast<std::size_t>(mate)]) {
          reachable_left[static_cast<std::size_t>(mate)] = true;
          pending.push(mate);
        }
      }
    }

    VertexCover cover{std::vector<bool>(graph_.size()),
                      std::vector<bool>(right_match_.size())};
    for (std::size_t u = 0; u < graph_.size(); ++u) {
      cover.left[u] = !reachable_left[u];
    }
    cover.right = std::move(reachable_right);
    return cover;
  }

 private:
  std::vector<std::vector<int>> graph_;
  std::vector<int> left_match_;
  std::vector<int> right_match_;
  std::vector<int> distance_;
  int matching_size_ = 0;

  bool build_layers() {
    constexpr int kUnreachable = -1;
    std::queue<int> pending;
    for (std::size_t u = 0; u < graph_.size(); ++u) {
      if (left_match_[u] == -1) {
        distance_[u] = 0;
        pending.push(static_cast<int>(u));
      } else {
        distance_[u] = kUnreachable;
      }
    }

    bool reaches_free_right = false;
    while (!pending.empty()) {
      const int u = pending.front();
      pending.pop();
      for (int v : graph_[static_cast<std::size_t>(u)]) {
        const int mate = right_match_[static_cast<std::size_t>(v)];
        if (mate == -1) {
          reaches_free_right = true;
        } else if (distance_[static_cast<std::size_t>(mate)] == kUnreachable) {
          distance_[static_cast<std::size_t>(mate)] =
              distance_[static_cast<std::size_t>(u)] + 1;
          pending.push(mate);
        }
      }
    }
    return reaches_free_right;
  }

  bool augment(int u) {
    for (int v : graph_[static_cast<std::size_t>(u)]) {
      const int mate = right_match_[static_cast<std::size_t>(v)];
      if (mate == -1 ||
          (distance_[static_cast<std::size_t>(mate)] ==
               distance_[static_cast<std::size_t>(u)] + 1 &&
           augment(mate))) {
        left_match_[static_cast<std::size_t>(u)] = v;
        right_match_[static_cast<std::size_t>(v)] = u;
        return true;
      }
    }
    distance_[static_cast<std::size_t>(u)] = -1;
    return false;
  }
};

}  // namespace algo
