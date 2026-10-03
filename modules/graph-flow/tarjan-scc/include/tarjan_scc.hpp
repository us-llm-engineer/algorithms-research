#pragma once

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algo {

class TarjanScc {
 public:
  struct Result {
    std::vector<int> component_of;
    std::vector<std::vector<int>> components;
    std::vector<std::pair<int, int>> condensation_edges;
  };

  explicit TarjanScc(int vertex_count)
      : vertex_count_(vertex_count), adjacency_(checked_size(vertex_count)) {}

  void add_edge(int from, int to) {
    check_vertex(from);
    check_vertex(to);
    adjacency_[static_cast<std::size_t>(from)].push_back(to);
  }

  Result run() const {
    const int unseen = -1;
    std::vector<int> discovery(static_cast<std::size_t>(vertex_count_), unseen);
    std::vector<int> low(static_cast<std::size_t>(vertex_count_), 0);
    std::vector<int> component_of(static_cast<std::size_t>(vertex_count_), unseen);
    std::vector<char> on_stack(static_cast<std::size_t>(vertex_count_), false);
    std::vector<int> tarjan_stack;
    tarjan_stack.reserve(static_cast<std::size_t>(vertex_count_));

    struct Frame {
      int vertex;
      std::size_t next_edge;
      int parent;
    };
    std::vector<Frame> dfs;
    dfs.reserve(static_cast<std::size_t>(vertex_count_));
    std::vector<std::vector<int>> components;
    int clock = 0;

    for (int root = 0; root < vertex_count_; ++root) {
      if (discovery[static_cast<std::size_t>(root)] != unseen) continue;
      discovery[static_cast<std::size_t>(root)] = low[static_cast<std::size_t>(root)] = clock++;
      tarjan_stack.push_back(root);
      on_stack[static_cast<std::size_t>(root)] = true;
      dfs.push_back({root, 0, -1});

      while (!dfs.empty()) {
        Frame& frame = dfs.back();
        const auto& edges = adjacency_[static_cast<std::size_t>(frame.vertex)];
        if (frame.next_edge < edges.size()) {
          const int next = edges[frame.next_edge++];
          const auto ni = static_cast<std::size_t>(next);
          if (discovery[ni] == unseen) {
            discovery[ni] = low[ni] = clock++;
            tarjan_stack.push_back(next);
            on_stack[ni] = true;
            dfs.push_back({next, 0, frame.vertex});
          } else if (on_stack[ni]) {
            low[static_cast<std::size_t>(frame.vertex)] =
                std::min(low[static_cast<std::size_t>(frame.vertex)], discovery[ni]);
          }
          continue;
        }

        const int vertex = frame.vertex;
        const int parent = frame.parent;
        dfs.pop_back();
        const auto vi = static_cast<std::size_t>(vertex);
        if (parent != -1) {
          const auto pi = static_cast<std::size_t>(parent);
          low[pi] = std::min(low[pi], low[vi]);
        }
        if (low[vi] == discovery[vi]) {
          const int id = static_cast<int>(components.size());
          components.emplace_back();
          while (true) {
            const int member = tarjan_stack.back();
            tarjan_stack.pop_back();
            on_stack[static_cast<std::size_t>(member)] = false;
            component_of[static_cast<std::size_t>(member)] = id;
            components.back().push_back(member);
            if (member == vertex) break;
          }
        }
      }
    }

    std::vector<std::pair<int, int>> condensation;
    for (int from = 0; from < vertex_count_; ++from) {
      const int source_component = component_of[static_cast<std::size_t>(from)];
      for (int to : adjacency_[static_cast<std::size_t>(from)]) {
        const int target_component = component_of[static_cast<std::size_t>(to)];
        if (source_component != target_component)
          condensation.emplace_back(source_component, target_component);
      }
    }
    std::sort(condensation.begin(), condensation.end());
    condensation.erase(std::unique(condensation.begin(), condensation.end()), condensation.end());
    return {std::move(component_of), std::move(components), std::move(condensation)};
  }

 private:
  static std::size_t checked_size(int n) {
    if (n < 0) throw std::invalid_argument("vertex count must be non-negative");
    return static_cast<std::size_t>(n);
  }

  void check_vertex(int vertex) const {
    if (vertex < 0 || vertex >= vertex_count_) throw std::out_of_range("vertex out of range");
  }

  int vertex_count_;
  std::vector<std::vector<int>> adjacency_;
};

}  // namespace algo
