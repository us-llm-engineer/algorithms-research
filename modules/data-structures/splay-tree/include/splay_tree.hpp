#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algo {

template <class Key, class Compare = std::less<Key>>
class SplayTree {
 public:
  struct Stats {
    std::uint64_t rotations = 0;
    std::uint64_t accesses = 0;
  };

  explicit SplayTree(Compare comp = Compare())
      : compare_(comp), root_(kNone), size_(0), stats_{0, 0} {}

  bool insert(const Key& key) {
    ++stats_.accesses;
    if (root_ == kNone) {
      root_ = allocate(key);
      ++size_;
      return true;
    }

    std::size_t last = root_;
    std::size_t parent = kNone;
    bool is_left = false;

    // Search for the key
    while (last != kNone) {
      parent = last;
      if (compare_(key, nodes_[last].key)) {
        last = nodes_[last].left;
        is_left = true;
      } else if (compare_(nodes_[last].key, key)) {
        last = nodes_[last].right;
        is_left = false;
      } else {
        // Key already exists; splay it to the root
        splay(parent);
        return false;
      }
    }

    // Not found; attach new node as a leaf
    std::size_t new_node = allocate(key);
    ++size_;
    nodes_[new_node].parent = parent;
    if (is_left) {
      nodes_[parent].left = new_node;
    } else {
      nodes_[parent].right = new_node;
    }

    // Splay the new node to the root
    splay(new_node);
    return true;
  }

  bool erase(const Key& key) {
    ++stats_.accesses;
    if (root_ == kNone) return false;

    std::size_t node = root_;
    std::size_t last_visited = kNone;

    // Search for the key
    while (node != kNone) {
      last_visited = node;
      if (compare_(key, nodes_[node].key)) {
        node = nodes_[node].left;
      } else if (compare_(nodes_[node].key, key)) {
        node = nodes_[node].right;
      } else {
        // Found it
        break;
      }
    }

    // Splay the found node (or last visited)
    splay(last_visited);

    // Check if we found the key
    if (root_ == kNone || !equal(nodes_[root_].key, key)) {
      return false;
    }

    // Remove the root
    std::size_t left = nodes_[root_].left;
    std::size_t right = nodes_[root_].right;
    deallocate(root_);
    --size_;

    if (left == kNone) {
      root_ = right;
      if (root_ != kNone) {
        nodes_[root_].parent = kNone;
      }
    } else {
      // Find max in the left subtree and splay it
      nodes_[left].parent = kNone;
      std::size_t max_node = left;
      while (nodes_[max_node].right != kNone) {
        max_node = nodes_[max_node].right;
      }

      // Splay the max node to be the root of the left subtree
      splay_subtree(max_node, left);

      // Attach the right subtree
      nodes_[max_node].right = right;
      if (right != kNone) {
        nodes_[right].parent = max_node;
      }

      root_ = max_node;
      nodes_[max_node].parent = kNone;
    }

    return true;
  }

  bool contains(const Key& key) {
    ++stats_.accesses;
    if (root_ == kNone) {
      return false;
    }

    std::size_t node = root_;
    std::size_t last = kNone;

    while (node != kNone) {
      last = node;
      if (compare_(key, nodes_[node].key)) {
        node = nodes_[node].left;
      } else if (compare_(nodes_[node].key, key)) {
        node = nodes_[node].right;
      } else {
        // Found it
        splay(node);
        return true;
      }
    }

    // Not found; splay the last visited node
    splay(last);
    return false;
  }

  std::optional<Key> lower_bound(const Key& key) {
    ++stats_.accesses;
    if (root_ == kNone) {
      return std::nullopt;
    }

    std::size_t result = kNone;
    std::size_t last = kNone;
    std::size_t node = root_;

    while (node != kNone) {
      last = node;
      if (compare_(key, nodes_[node].key)) {
        result = node;
        node = nodes_[node].left;
      } else if (compare_(nodes_[node].key, key)) {
        node = nodes_[node].right;
      } else {
        result = node;
        break;
      }
    }

    // Splay the result (or last visited)
    if (result != kNone) {
      splay(result);
      return nodes_[result].key;
    } else if (last != kNone) {
      splay(last);
    }

    return std::nullopt;
  }

  std::optional<Key> upper_bound(const Key& key) {
    ++stats_.accesses;
    if (root_ == kNone) {
      return std::nullopt;
    }

    std::size_t result = kNone;
    std::size_t last = kNone;
    std::size_t node = root_;

    while (node != kNone) {
      last = node;
      if (compare_(key, nodes_[node].key)) {
        result = node;
        node = nodes_[node].left;
      } else {
        node = nodes_[node].right;
      }
    }

    // Splay the result (or last visited)
    if (result != kNone) {
      splay(result);
      return nodes_[result].key;
    } else if (last != kNone) {
      splay(last);
    }

    return std::nullopt;
  }

  std::optional<Key> predecessor(const Key& key) {
    ++stats_.accesses;
    if (root_ == kNone) {
      return std::nullopt;
    }

    std::size_t result = kNone;
    std::size_t last = kNone;
    std::size_t node = root_;

    while (node != kNone) {
      last = node;
      if (compare_(nodes_[node].key, key)) {
        result = node;
        node = nodes_[node].right;
      } else {
        node = nodes_[node].left;
      }
    }

    // Splay the result (or last visited)
    if (result != kNone) {
      splay(result);
      return nodes_[result].key;
    } else if (last != kNone) {
      splay(last);
    }

    return std::nullopt;
  }

  std::optional<Key> min() {
    ++stats_.accesses;
    if (root_ == kNone) {
      return std::nullopt;
    }

    std::size_t node = root_;
    while (nodes_[node].left != kNone) {
      node = nodes_[node].left;
    }

    splay(node);
    return nodes_[node].key;
  }

  std::optional<Key> max() {
    ++stats_.accesses;
    if (root_ == kNone) {
      return std::nullopt;
    }

    std::size_t node = root_;
    while (nodes_[node].right != kNone) {
      node = nodes_[node].right;
    }

    splay(node);
    return nodes_[node].key;
  }

  std::size_t size() const noexcept {
    return size_;
  }

  bool empty() const noexcept {
    return size_ == 0;
  }

  std::size_t height() const {
    if (root_ == kNone) return 0;

    std::size_t max_h = 0;
    std::vector<std::pair<std::size_t, std::size_t>> stack; // (node, depth)
    stack.push_back({root_, 1});

    while (!stack.empty()) {
      auto [node, depth] = stack.back();
      stack.pop_back();

      if (depth > max_h) max_h = depth;

      if (nodes_[node].left != kNone) {
        stack.push_back({nodes_[node].left, depth + 1});
      }
      if (nodes_[node].right != kNone) {
        stack.push_back({nodes_[node].right, depth + 1});
      }
    }

    return max_h;
  }

  std::optional<Key> root_key() const {
    if (root_ == kNone) {
      return std::nullopt;
    }
    return nodes_[root_].key;
  }

  std::vector<Key> to_vector() const {
    std::vector<Key> result;
    if (root_ == kNone) return result;

    // Iterative in-order traversal
    std::vector<std::pair<std::size_t, int>> stack; // (node, state)
    stack.push_back({root_, 0});

    while (!stack.empty()) {
      auto [node, state] = stack.back();
      stack.pop_back();

      if (state == 0) {
        // Going down left
        stack.push_back({node, 1});
        if (nodes_[node].left != kNone) {
          stack.push_back({nodes_[node].left, 0});
        }
      } else if (state == 1) {
        // After left, visit node
        result.push_back(nodes_[node].key);
        stack.push_back({node, 2});
        if (nodes_[node].right != kNone) {
          stack.push_back({nodes_[node].right, 0});
        }
      }
    }

    return result;
  }

  const Stats& stats() const noexcept {
    return stats_;
  }

  void reset_stats() noexcept {
    stats_.rotations = 0;
    stats_.accesses = 0;
  }

  void check_invariants() const {
    if (root_ == kNone) {
      if (size_ != 0) {
        throw std::logic_error("Empty tree must have size 0");
      }
      return;
    }

    // Check that root has no parent
    if (nodes_[root_].parent != kNone) {
      throw std::logic_error("Root must have no parent");
    }

    // Count nodes and verify BST property
    std::vector<bool> visited(nodes_.size(), false);
    std::vector<std::pair<std::size_t, bool>> stack; // (node, is_left_child)
    stack.push_back({root_, false});
    std::size_t count = 0;

    std::vector<std::pair<std::size_t, int>> traversal_stack; // (node, state)
    traversal_stack.push_back({root_, 0});
    Key prev_key = Key();
    bool first = true;

    while (!traversal_stack.empty()) {
      auto [node, state] = traversal_stack.back();
      traversal_stack.pop_back();

      if (state == 0) {
        // Going down left
        if (visited[node]) {
          throw std::logic_error("Cycle detected in tree");
        }
        visited[node] = true;
        ++count;

        traversal_stack.push_back({node, 1});
        if (nodes_[node].left != kNone) {
          traversal_stack.push_back({nodes_[node].left, 0});
          // Check that left child's parent pointer is correct
          if (nodes_[nodes_[node].left].parent != node) {
            throw std::logic_error("Left child's parent pointer is incorrect");
          }
        }
      } else if (state == 1) {
        // After left, visit node
        if (!first && !compare_(prev_key, nodes_[node].key) && !equal(prev_key, nodes_[node].key)) {
          throw std::logic_error("BST order violated");
        }
        prev_key = nodes_[node].key;
        first = false;

        traversal_stack.push_back({node, 2});
        if (nodes_[node].right != kNone) {
          traversal_stack.push_back({nodes_[node].right, 0});
          // Check that right child's parent pointer is correct
          if (nodes_[nodes_[node].right].parent != node) {
            throw std::logic_error("Right child's parent pointer is incorrect");
          }
        }
      }
    }

    if (count != size_) {
      throw std::logic_error("Node count mismatch");
    }
  }

 private:
  struct Node {
    Key key;
    std::size_t left = kNone;
    std::size_t right = kNone;
    std::size_t parent = kNone;
  };

  static constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();

  Compare compare_;
  std::vector<Node> nodes_;
  std::size_t root_;
  std::size_t size_;
  std::vector<std::size_t> free_list_;
  Stats stats_;

  std::size_t allocate(const Key& key) {
    std::size_t idx;
    if (!free_list_.empty()) {
      idx = free_list_.back();
      free_list_.pop_back();
      nodes_[idx] = Node{key, kNone, kNone, kNone};
    } else {
      idx = nodes_.size();
      nodes_.push_back(Node{key, kNone, kNone, kNone});
    }
    return idx;
  }

  void deallocate(std::size_t idx) {
    free_list_.push_back(idx);
  }

  bool equal(const Key& a, const Key& b) const {
    return !compare_(a, b) && !compare_(b, a);
  }

  // Splay a node to the root using the Sleator-Tarjan algorithm
  void splay(std::size_t node) {
    while (nodes_[node].parent != kNone) {
      std::size_t parent = nodes_[node].parent;
      std::size_t grandparent = nodes_[parent].parent;

      if (grandparent == kNone) {
        // Zig: node is a child of the root
        zig(node, parent);
        ++stats_.rotations;
      } else {
        // Check if it's zig-zig or zig-zag
        bool is_parent_left_child = nodes_[grandparent].left == parent;
        bool is_node_left_child = nodes_[parent].left == node;

        if (is_parent_left_child == is_node_left_child) {
          // Zig-zig
          zig(parent, grandparent);
          zig(node, parent);
          stats_.rotations += 2;
        } else {
          // Zig-zag
          zig(node, parent);
          zig(node, grandparent);
          stats_.rotations += 2;
        }

      }
    }

    root_ = node;
  }

  // Splay within a subtree (for erase operation)
  void splay_subtree(std::size_t node, std::size_t subtree_root) {
    while (node != subtree_root && nodes_[node].parent != kNone) {
      std::size_t parent = nodes_[node].parent;
      std::size_t grandparent = nodes_[parent].parent;

      if (grandparent == kNone || parent == subtree_root) {
        // Zig
        zig(node, parent);
        ++stats_.rotations;
      } else {
        bool is_parent_left_child = nodes_[grandparent].left == parent;
        bool is_node_left_child = nodes_[parent].left == node;

        if (is_parent_left_child == is_node_left_child) {
          // Zig-zig
          zig(parent, grandparent);
          zig(node, parent);
          stats_.rotations += 2;
        } else {
          // Zig-zag
          zig(node, parent);
          zig(node, grandparent);
          stats_.rotations += 2;
        }

      }
    }
  }

  // Single rotation: rotate node up over its parent
  void zig(std::size_t node, std::size_t parent) {
    const std::size_t grandparent = nodes_[parent].parent;
    bool is_left_child = nodes_[parent].left == node;

    if (is_left_child) {
      // Right rotation
      std::size_t right_child = nodes_[node].right;
      nodes_[parent].left = right_child;
      if (right_child != kNone) {
        nodes_[right_child].parent = parent;
      }
      nodes_[node].right = parent;
    } else {
      // Left rotation
      std::size_t left_child = nodes_[node].left;
      nodes_[parent].right = left_child;
      if (left_child != kNone) {
        nodes_[left_child].parent = parent;
      }
      nodes_[node].left = parent;
    }

    nodes_[node].parent = nodes_[parent].parent;
    nodes_[parent].parent = node;
    if (grandparent != kNone) {
      if (nodes_[grandparent].left == parent) {
        nodes_[grandparent].left = node;
      } else {
        nodes_[grandparent].right = node;
      }
    }
  }
};

}  // namespace algo
