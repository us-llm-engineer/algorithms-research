#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algo {

template <class Key, class Compare = std::less<Key>>
class SkipList {
 public:
  explicit SkipList(std::uint64_t seed = 0) : seed_(seed), rng_(seed ? seed : 0x9e3779b97f4a7c15ULL) {
    nodes_.push_back(Node{});
    nodes_[0].next.assign(kMaxLevel, 0);
  }

  bool empty() const noexcept { return size_ == 0; }
  std::size_t size() const noexcept { return size_; }
  std::size_t node_capacity() const noexcept { return nodes_.capacity(); }
  std::size_t height() const noexcept { return size_ == 0 ? 0 : current_level_; }

  bool contains(const Key& key) const { return find_index(key) != 0; }

  bool insert(const Key& key) {
    std::vector<std::size_t> prev(kMaxLevel, 0);
    locate(key, prev);
    const auto next = nodes_[prev[0]].next[0];
    if (next != 0 && equal(nodes_[next].key, key)) return false;
    const auto level = random_level();
    const auto idx = allocate(key, level);
    for (std::size_t i = 0; i < level; ++i) {
      nodes_[idx].next[i] = nodes_[prev[i]].next[i];
      nodes_[prev[i]].next[i] = idx;
    }
    current_level_ = std::max(current_level_, level);
    ++size_;
    return true;
  }

  bool erase(const Key& key) {
    std::vector<std::size_t> prev(kMaxLevel, 0);
    locate(key, prev);
    const auto idx = nodes_[prev[0]].next[0];
    if (idx == 0 || !equal(nodes_[idx].key, key)) return false;
    for (std::size_t i = 0; i < nodes_[idx].next.size(); ++i) {
      if (nodes_[prev[i]].next[i] == idx) nodes_[prev[i]].next[i] = nodes_[idx].next[i];
    }
    free_.push_back(idx);
    --size_;
    while (current_level_ > 1 && nodes_[0].next[current_level_ - 1] == 0) --current_level_;
    if (size_ == 0) current_level_ = 1;
    return true;
  }

  std::vector<Key> to_vector() const {
    std::vector<Key> result;
    result.reserve(size_);
    for (auto p = nodes_[0].next[0]; p != 0; p = nodes_[p].next[0]) result.push_back(nodes_[p].key);
    return result;
  }

  void clear() {
    nodes_.resize(1);
    nodes_[0].next.assign(kMaxLevel, 0);
    free_.clear(); size_ = 0; current_level_ = 1;
  }

  void check_invariants() const {
    std::size_t count = 0, previous = 0;
    for (auto p = nodes_[0].next[0]; p != 0; p = nodes_[p].next[0]) {
      if (p >= nodes_.size() || !nodes_[p].alive) throw std::logic_error("skip list dangling node");
      if (previous != 0 && !comp_(nodes_[previous].key, nodes_[p].key)) throw std::logic_error("skip list order");
      previous = p; ++count;
    }
    if (count != size_) throw std::logic_error("skip list size");
    for (std::size_t level = 1; level < current_level_; ++level) {
      auto p = nodes_[0].next[level];
      while (p != 0) { if (nodes_[p].next.size() <= level) throw std::logic_error("skip list level"); p = nodes_[p].next[level]; }
    }
  }

 private:
  static constexpr std::size_t kMaxLevel = 24;
  struct Node { Key key{}; std::vector<std::size_t> next{}; bool alive = false; };
  std::uint64_t seed_;
  std::uint64_t rng_;
  Compare comp_{};
  std::vector<Node> nodes_;
  std::vector<std::size_t> free_;
  std::size_t size_ = 0, current_level_ = 1;

  static std::uint64_t mix(std::uint64_t x) { x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; return x ^ (x >> 31); }
  std::size_t random_level() { std::size_t l = 1; rng_ = mix(rng_); while (l < kMaxLevel && (rng_ & 3ULL) == 0) { ++l; rng_ = mix(rng_); } return l; }
  bool equal(const Key& a, const Key& b) const { return !comp_(a,b) && !comp_(b,a); }
  void locate(const Key& key, std::vector<std::size_t>& prev) const {
    auto p = std::size_t{0};
    for (std::size_t level = current_level_; level-- > 0;) { while (nodes_[p].next[level] != 0 && comp_(nodes_[nodes_[p].next[level]].key, key)) p = nodes_[p].next[level]; prev[level] = p; }
    for (std::size_t level = current_level_; level < kMaxLevel; ++level) prev[level] = 0;
  }
  std::size_t find_index(const Key& key) const { std::vector<std::size_t> p(kMaxLevel); locate(key,p); auto n=nodes_[p[0]].next[0]; return n != 0 && equal(nodes_[n].key,key) ? n : 0; }
  std::size_t allocate(const Key& key, std::size_t level) {
    std::size_t idx;
    if (free_.empty()) { idx = nodes_.size(); nodes_.push_back(Node{}); } else { idx = free_.back(); free_.pop_back(); }
    nodes_[idx].key = key; nodes_[idx].next.assign(level, 0); nodes_[idx].alive = true; return idx;
  }
};

}  // namespace algo
