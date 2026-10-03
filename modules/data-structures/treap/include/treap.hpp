#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <stack>
#include <utility>
#include <vector>

#include "rng.hpp"

namespace algo {

// A deterministic-seed randomized search tree. Keys are unique according to
// Compare; priorities form a max heap and subtree sizes support rank/select.
template <class Key, class Compare = std::less<Key>>
class Treap {
 public:
  struct Stats {
    std::uint64_t node_visits = 0;
  };

  explicit Treap(std::uint64_t seed = 1, Compare cmp = Compare())
      : rng_(seed), cmp_(std::move(cmp)) {}

  bool insert(const Key& key) {
    bool inserted = false;
    root_ = insert_rec(root_, key, inserted);
    return inserted;
  }

  bool erase(const Key& key) {
    bool erased = false;
    root_ = erase_rec(root_, key, erased);
    return erased;
  }

  bool contains(const Key& key) const {
    auto idx = root_;
    while (idx != nil) {
      const Node& node = arena_[idx];
      ++stats_.node_visits;
      if (cmp_(key, node.key)) idx = node.left;
      else if (cmp_(node.key, key)) idx = node.right;
      else return true;
    }
    return false;
  }

  std::optional<Key> lower_bound(const Key& key) const {
    auto idx = root_;
    auto candidate = nil;
    while (idx != nil) {
      const Node& node = arena_[idx];
      ++stats_.node_visits;
      if (cmp_(node.key, key)) {
        idx = node.right;
      } else {
        candidate = idx;
        idx = node.left;
      }
    }
    if (candidate == nil) return std::nullopt;
    return arena_[candidate].key;
  }

  std::optional<Key> kth(std::size_t k) const {
    if (k >= size()) return std::nullopt;
    auto idx = root_;
    while (idx != nil) {
      const Node& node = arena_[idx];
      ++stats_.node_visits;
      const std::size_t left_size = node_size(node.left);
      if (k < left_size) idx = node.left;
      else if (k == left_size) return node.key;
      else {
        k -= left_size + 1;
        idx = node.right;
      }
    }
    return std::nullopt;
  }

  // Number of stored keys strictly before key in Compare order.
  std::size_t rank(const Key& key) const {
    std::size_t result = 0;
    auto idx = root_;
    while (idx != nil) {
      const Node& node = arena_[idx];
      ++stats_.node_visits;
      if (cmp_(key, node.key)) {
        idx = node.left;
      } else if (cmp_(node.key, key)) {
        result += node_size(node.left) + 1;
        idx = node.right;
      } else {
        return result + node_size(node.left);
      }
    }
    return result;
  }

  std::size_t size() const noexcept { return node_size(root_); }
  bool empty() const noexcept { return root_ == nil; }

  std::size_t height() const {
    if (root_ == nil) return 0;
    std::size_t max_height = 0;
    std::stack<std::pair<std::uint32_t, std::size_t>> pending;
    pending.push({root_, 1});
    while (!pending.empty()) {
      const auto [idx, depth] = pending.top();
      pending.pop();
      max_height = std::max(max_height, depth);
      const Node& node = arena_[idx];
      if (node.left != nil) pending.push({node.left, depth + 1});
      if (node.right != nil) pending.push({node.right, depth + 1});
    }
    return max_height;
  }

  std::optional<std::size_t> depth_of(const Key& key) const {
    auto idx = root_;
    std::size_t depth = 0;
    while (idx != nil) {
      const Node& node = arena_[idx];
      if (cmp_(key, node.key)) idx = node.left;
      else if (cmp_(node.key, key)) idx = node.right;
      else return depth;
      ++depth;
    }
    return std::nullopt;
  }

  std::vector<Key> to_vector() const {
    std::vector<Key> result;
    result.reserve(size());
    std::stack<std::uint32_t> pending;
    auto idx = root_;
    while (!pending.empty() || idx != nil) {
      while (idx != nil) {
        pending.push(idx);
        idx = arena_[idx].left;
      }
      idx = pending.top();
      pending.pop();
      result.push_back(arena_[idx].key);
      idx = arena_[idx].right;
    }
    return result;
  }

  // Number of allocated slots, including reusable erased slots.
  std::size_t node_capacity() const noexcept { return arena_.size(); }
  const Stats& stats() const noexcept { return stats_; }
  void reset_stats() const noexcept { stats_ = Stats{}; }

  void check_invariants() const {
    if (root_ != nil) check_invariants_rec(root_, nullptr, nullptr);
  }

 private:
  static constexpr std::uint32_t nil = std::numeric_limits<std::uint32_t>::max();

  struct Node {
    Key key;
    std::uint64_t priority;
    std::uint32_t left = nil;
    std::uint32_t right = nil;
    std::uint32_t subtree_size = 1;
  };

  Rng rng_;
  Compare cmp_;
  std::vector<Node> arena_;
  std::vector<std::uint32_t> free_list_;
  std::uint32_t root_ = nil;
  mutable Stats stats_;

  std::size_t node_size(std::uint32_t idx) const noexcept {
    return idx == nil ? 0 : arena_[idx].subtree_size;
  }

  void update(std::uint32_t idx) {
    Node& node = arena_[idx];
    const std::size_t count = 1 + node_size(node.left) + node_size(node.right);
    if (count > std::numeric_limits<std::uint32_t>::max())
      throw std::length_error("treap exceeds supported size");
    node.subtree_size = static_cast<std::uint32_t>(count);
  }

  std::uint32_t allocate(const Key& key) {
    const auto priority = rng_();
    if (free_list_.empty()) {
      if (arena_.size() >= nil) throw std::length_error("treap node index capacity exceeded");
      arena_.push_back(Node{key, priority});
      return static_cast<std::uint32_t>(arena_.size() - 1);
    }
    const auto idx = free_list_.back();
    free_list_.pop_back();
    arena_[idx] = Node{key, priority};
    return idx;
  }

  std::uint32_t rotate_right(std::uint32_t root) {
    const auto pivot = arena_[root].left;
    arena_[root].left = arena_[pivot].right;
    arena_[pivot].right = root;
    update(root);
    update(pivot);
    return pivot;
  }

  std::uint32_t rotate_left(std::uint32_t root) {
    const auto pivot = arena_[root].right;
    arena_[root].right = arena_[pivot].left;
    arena_[pivot].left = root;
    update(root);
    update(pivot);
    return pivot;
  }

  std::uint32_t insert_rec(std::uint32_t idx, const Key& key, bool& inserted) {
    if (idx == nil) {
      inserted = true;
      return allocate(key);
    }
    if (cmp_(key, arena_[idx].key)) {
      const auto child = insert_rec(arena_[idx].left, key, inserted);
      if (!inserted) return idx;
      arena_[idx].left = child;
      if (arena_[child].priority > arena_[idx].priority) idx = rotate_right(idx);
    } else if (cmp_(arena_[idx].key, key)) {
      const auto child = insert_rec(arena_[idx].right, key, inserted);
      if (!inserted) return idx;
      arena_[idx].right = child;
      if (arena_[child].priority > arena_[idx].priority) idx = rotate_left(idx);
    } else {
      return idx;
    }
    update(idx);
    return idx;
  }

  std::uint32_t merge(std::uint32_t left, std::uint32_t right) {
    if (left == nil) return right;
    if (right == nil) return left;
    if (arena_[left].priority > arena_[right].priority) {
      const auto child = merge(arena_[left].right, right);
      arena_[left].right = child;
      update(left);
      return left;
    }
    const auto child = merge(left, arena_[right].left);
    arena_[right].left = child;
    update(right);
    return right;
  }

  std::uint32_t erase_rec(std::uint32_t idx, const Key& key, bool& erased) {
    if (idx == nil) return nil;
    if (cmp_(key, arena_[idx].key)) {
      const auto child = erase_rec(arena_[idx].left, key, erased);
      if (erased) arena_[idx].left = child;
    } else if (cmp_(arena_[idx].key, key)) {
      const auto child = erase_rec(arena_[idx].right, key, erased);
      if (erased) arena_[idx].right = child;
    } else {
      const auto left = arena_[idx].left;
      const auto right = arena_[idx].right;
      const auto replacement = merge(left, right);
      free_list_.push_back(idx);
      erased = true;
      return replacement;
    }
    if (erased) update(idx);
    return idx;
  }

  void check_invariants_rec(std::uint32_t idx, const Key* lower, const Key* upper) const {
    const Node& node = arena_[idx];
    if (lower != nullptr && !cmp_(*lower, node.key))
      throw std::logic_error("BST order violated: lower bound");
    if (upper != nullptr && !cmp_(node.key, *upper))
      throw std::logic_error("BST order violated: upper bound");
    if ((node.left != nil && arena_[node.left].priority > node.priority) ||
        (node.right != nil && arena_[node.right].priority > node.priority))
      throw std::logic_error("treap heap order violated");
    const std::size_t expected = 1 + node_size(node.left) + node_size(node.right);
    if (expected != node.subtree_size) throw std::logic_error("treap subtree size mismatch");
    if (node.left != nil) check_invariants_rec(node.left, lower, &node.key);
    if (node.right != nil) check_invariants_rec(node.right, &node.key, upper);
  }
};

}  // namespace algo
