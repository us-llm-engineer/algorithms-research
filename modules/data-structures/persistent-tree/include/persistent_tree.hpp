#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

#include "rng.hpp"

namespace algo {

template <class Key, class Value, class Compare = std::less<Key>>
class PersistentMap {
 public:
  using Version = std::uint32_t;

  PersistentMap() : compare_(Compare()) {
    // Version 0 is the empty map
    versions_.push_back(null_index);
  }

  Version empty_version() const noexcept { return 0; }

  Version insert(Version v, const Key& key, const Value& value) {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    const uint32_t new_root = insert_impl(versions_[v], key, value);
    versions_.push_back(new_root);
    last_nodes_created_ = nodes_created_in_last_op_;
    return static_cast<Version>(versions_.size() - 1);
  }

  Version erase(Version v, const Key& key) {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    const uint32_t new_root = erase_impl(versions_[v], key);
    versions_.push_back(new_root);
    last_nodes_created_ = nodes_created_in_last_op_;
    return static_cast<Version>(versions_.size() - 1);
  }

  std::optional<Value> find(Version v, const Key& key) const {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    return find_impl(versions_[v], key);
  }

  bool contains(Version v, const Key& key) const {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    return find_impl(versions_[v], key).has_value();
  }

  std::size_t size(Version v) const {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    return node_size(versions_[v]);
  }

  std::optional<std::pair<Key, Value>> kth(Version v, std::size_t k) const {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    return kth_impl(versions_[v], k);
  }

  std::size_t rank(Version v, const Key& key) const {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    return rank_impl(versions_[v], key);
  }

  std::vector<std::pair<Key, Value>> to_vector(Version v) const {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    std::vector<std::pair<Key, Value>> result;
    to_vector_impl(versions_[v], result);
    return result;
  }

  std::size_t depth(Version v) const {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    return depth_impl(versions_[v]);
  }

  std::size_t versions() const noexcept { return versions_.size(); }

  std::size_t node_count() const noexcept { return node_arena_.size(); }

  std::size_t nodes_created_last_update() const noexcept { return last_nodes_created_; }

  std::uint64_t shape_hash(Version v) const {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    std::uint64_t h = 0;
    shape_hash_impl(versions_[v], h);
    return h;
  }

  void check_invariants(Version v) const {
    if (v >= versions_.size()) throw std::out_of_range("Unknown version");
    check_invariants_impl(versions_[v], std::nullopt, std::nullopt);
  }

 private:
  struct Node {
    Key key;
    Value value;
    std::uint64_t prio;
    std::uint32_t left;
    std::uint32_t right;
    std::uint32_t size;

    Node(const Key& k, const Value& val, std::uint64_t p, std::uint32_t l, std::uint32_t r, std::uint32_t sz)
        : key(k), value(val), prio(p), left(l), right(r), size(sz) {}
  };

  static constexpr std::uint32_t null_index = ~0u;

  std::vector<Node> node_arena_;
  std::vector<std::uint32_t> versions_;
  Compare compare_;
  std::size_t last_nodes_created_ = 0;
  std::size_t nodes_created_in_last_op_ = 0;

  std::uint32_t new_node(const Key& key, const Value& value, std::uint64_t prio, std::uint32_t left, std::uint32_t right) {
    node_arena_.emplace_back(key, value, prio, left, right, 1 + node_size(left) + node_size(right));
    nodes_created_in_last_op_++;
    return static_cast<std::uint32_t>(node_arena_.size() - 1);
  }

  std::size_t node_size(std::uint32_t node_idx) const {
    return node_idx == null_index ? 0 : node_arena_[node_idx].size;
  }

  std::uint64_t node_prio(const Key& key) const {
    return mix64(std::hash<Key>{}(key));
  }

  bool higher_priority(const Key& a_key, std::uint64_t a_prio,
                       const Key& b_key, std::uint64_t b_prio) const {
    if (a_prio != b_prio) return a_prio > b_prio;
    // Resolve hash collisions consistently so a key set has one shape regardless
    // of insertion order, including when a custom comparator is used.
    return compare_(b_key, a_key);
  }

  std::uint32_t insert_impl(std::uint32_t node_idx, const Key& key, const Value& value) {
    nodes_created_in_last_op_ = 0;
    return insert_recursive(node_idx, key, value);
  }

  std::uint32_t insert_recursive(std::uint32_t node_idx, const Key& key, const Value& value) {
    if (node_idx == null_index) {
      return new_node(key, value, node_prio(key), null_index, null_index);
    }

    // Recursive calls may grow node_arena_ and invalidate references into it.
    const Node node = node_arena_[node_idx];

    if (compare_(key, node.key)) {
      // key < node.key, go left
      const uint32_t new_left = insert_recursive(node.left, key, value);
      const uint32_t new_right = node.right;

      // Check if we need to rotate
      if (new_left != null_index && higher_priority(node_arena_[new_left].key, node_arena_[new_left].prio,
                                                     node.key, node.prio)) {
        // Rotate right: new_left becomes root, node becomes right child
        return new_node(node_arena_[new_left].key, node_arena_[new_left].value, node_arena_[new_left].prio,
                        node_arena_[new_left].left, new_node(node.key, node.value, node.prio, node_arena_[new_left].right, new_right));
      } else {
        return new_node(node.key, node.value, node.prio, new_left, new_right);
      }
    } else if (compare_(node.key, key)) {
      // key > node.key, go right
      const uint32_t new_left = node.left;
      const uint32_t new_right = insert_recursive(node.right, key, value);

      // Check if we need to rotate
      if (new_right != null_index && higher_priority(node_arena_[new_right].key, node_arena_[new_right].prio,
                                                      node.key, node.prio)) {
        // Rotate left: new_right becomes root, node becomes left child
        return new_node(node_arena_[new_right].key, node_arena_[new_right].value, node_arena_[new_right].prio,
                        new_node(node.key, node.value, node.prio, new_left, node_arena_[new_right].left),
                        node_arena_[new_right].right);
      } else {
        return new_node(node.key, node.value, node.prio, new_left, new_right);
      }
    } else {
      // key == node.key, overwrite
      return new_node(node.key, value, node.prio, node.left, node.right);
    }
  }

  std::uint32_t erase_impl(std::uint32_t node_idx, const Key& key) {
    nodes_created_in_last_op_ = 0;
    return erase_recursive(node_idx, key);
  }

  std::uint32_t erase_recursive(std::uint32_t node_idx, const Key& key) {
    if (node_idx == null_index) {
      // Key not found, but still create a new node to return a new version
      return null_index;
    }

    const Node node = node_arena_[node_idx];

    if (compare_(key, node.key)) {
      // key < node.key, go left
      const uint32_t new_left = erase_recursive(node.left, key);
      return new_node(node.key, node.value, node.prio, new_left, node.right);
    } else if (compare_(node.key, key)) {
      // key > node.key, go right
      const uint32_t new_right = erase_recursive(node.right, key);
      return new_node(node.key, node.value, node.prio, node.left, new_right);
    } else {
      // key == node.key, delete this node
      return merge_impl(node.left, node.right);
    }
  }

  std::uint32_t merge_impl(std::uint32_t left_idx, std::uint32_t right_idx) {
    if (left_idx == null_index) return right_idx;
    if (right_idx == null_index) return left_idx;

    // merge_impl allocates while descending, so keep stable copies.
    const Node left_node = node_arena_[left_idx];
    const Node right_node = node_arena_[right_idx];

    if (higher_priority(left_node.key, left_node.prio, right_node.key, right_node.prio)) {
      // Left node has higher priority, keep it as root
      const uint32_t new_right = merge_impl(left_node.right, right_idx);
      return new_node(left_node.key, left_node.value, left_node.prio, left_node.left, new_right);
    } else {
      // Right node has higher priority, keep it as root
      const uint32_t new_left = merge_impl(left_idx, right_node.left);
      return new_node(right_node.key, right_node.value, right_node.prio, new_left, right_node.right);
    }
  }

  std::optional<Value> find_impl(std::uint32_t node_idx, const Key& key) const {
    if (node_idx == null_index) return std::nullopt;

    const Node& node = node_arena_[node_idx];

    if (compare_(key, node.key)) {
      return find_impl(node.left, key);
    } else if (compare_(node.key, key)) {
      return find_impl(node.right, key);
    } else {
      return node.value;
    }
  }

  std::optional<std::pair<Key, Value>> kth_impl(std::uint32_t node_idx, std::size_t k) const {
    if (node_idx == null_index) return std::nullopt;

    const Node& node = node_arena_[node_idx];
    const std::size_t left_size = node_size(node.left);

    if (k < left_size) {
      return kth_impl(node.left, k);
    } else if (k == left_size) {
      return std::make_pair(node.key, node.value);
    } else {
      return kth_impl(node.right, k - left_size - 1);
    }
  }

  std::size_t rank_impl(std::uint32_t node_idx, const Key& key) const {
    if (node_idx == null_index) return 0;

    const Node& node = node_arena_[node_idx];

    if (compare_(key, node.key)) {
      return rank_impl(node.left, key);
    } else if (compare_(node.key, key)) {
      return 1 + node_size(node.left) + rank_impl(node.right, key);
    } else {
      return node_size(node.left);
    }
  }

  void to_vector_impl(std::uint32_t node_idx, std::vector<std::pair<Key, Value>>& result) const {
    if (node_idx == null_index) return;

    const Node& node = node_arena_[node_idx];
    to_vector_impl(node.left, result);
    result.emplace_back(node.key, node.value);
    to_vector_impl(node.right, result);
  }

  std::size_t depth_impl(std::uint32_t node_idx) const {
    if (node_idx == null_index) return 0;

    const Node& node = node_arena_[node_idx];
    return 1 + std::max(depth_impl(node.left), depth_impl(node.right));
  }

  void shape_hash_impl(std::uint32_t node_idx, std::uint64_t& h) const {
    if (node_idx == null_index) {
      h = mix64(h ^ 0xDEADBEEFULL);
      return;
    }

    const Node& node = node_arena_[node_idx];
    h = mix64(h ^ std::hash<Key>{}(node.key));
    shape_hash_impl(node.left, h);
    shape_hash_impl(node.right, h);
  }

  void check_invariants_impl(std::uint32_t node_idx, std::optional<Key> min_key, std::optional<Key> max_key) const {
    if (node_idx == null_index) return;

    const Node& node = node_arena_[node_idx];

    // BST order
    if (min_key && !compare_(*min_key, node.key)) {
      throw std::logic_error("BST order violated: key not greater than min");
    }
    if (max_key && !compare_(node.key, *max_key)) {
      throw std::logic_error("BST order violated: key not less than max");
    }

    // Max-heap on priority
    if (node.left != null_index) {
      if (higher_priority(node_arena_[node.left].key, node_arena_[node.left].prio, node.key, node.prio)) {
        throw std::logic_error("Heap order violated: left child has higher priority");
      }
    }
    if (node.right != null_index) {
      if (higher_priority(node_arena_[node.right].key, node_arena_[node.right].prio, node.key, node.prio)) {
        throw std::logic_error("Heap order violated: right child has higher priority");
      }
    }

    // Size field
    const std::size_t expected_size = 1 + node_size(node.left) + node_size(node.right);
    if (node.size != expected_size) {
      throw std::logic_error("Size field incorrect");
    }

    check_invariants_impl(node.left, min_key, std::make_optional(node.key));
    check_invariants_impl(node.right, std::make_optional(node.key), max_key);
  }
};

}  // namespace algo
