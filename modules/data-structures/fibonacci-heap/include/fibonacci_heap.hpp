#pragma once
#include <cassert>
#include <algorithm>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <vector>

namespace algo {

template <class Key, class Compare = std::less<Key>> class FibonacciHeap {
 public:
  using Handle = std::uint32_t;
  static constexpr Handle kNull = 0xFFFFFFFFu;

  struct Stats {
    std::uint64_t links = 0;
    std::uint64_t cuts = 0;
    std::uint64_t root_visits = 0;
    std::uint64_t children_promoted = 0;
  };

  explicit FibonacciHeap(Compare comp = Compare()) : comp_(comp) {}

  void reserve(std::size_t n) {
    if (n > nodes_.size()) {
      nodes_.reserve(n);
    }
  }

  Handle push(const Key& key) {
    Handle h = allocate_node(key);
    if (min_root_ == kNull) {
      min_root_ = h;
      nodes_[h].left = h;
      nodes_[h].right = h;
      roots_count_ = 1;
    } else {
      // Insert as a new root into the circular list
      insert_into_sibling_list(h, min_root_);
      roots_count_++;
      if (comp_(key, nodes_[min_root_].key)) {
        min_root_ = h;
      }
    }
    size_++;
    return h;
  }

  bool empty() const noexcept { return size_ == 0; }

  std::size_t size() const noexcept { return size_; }

  const Key& top() const {
    if (min_root_ == kNull) {
      throw std::out_of_range("FibonacciHeap is empty");
    }
    return nodes_[min_root_].key;
  }

  Handle top_handle() const {
    if (min_root_ == kNull) {
      throw std::out_of_range("FibonacciHeap is empty");
    }
    return min_root_;
  }

  Key pop() {
    if (min_root_ == kNull) {
      throw std::out_of_range("FibonacciHeap is empty");
    }
    Key result = nodes_[min_root_].key;
    erase_internal(min_root_);
    return result;
  }

  void decrease_key(Handle h, const Key& new_key) {
    if (!contains(h)) {
      throw std::invalid_argument("Invalid or dead handle");
    }
    if (comp_(nodes_[h].key, new_key)) {
      throw std::invalid_argument("New key is greater than current key");
    }
    nodes_[h].key = new_key;

    Handle parent = nodes_[h].parent;
    if (parent != kNull && comp_(new_key, nodes_[parent].key)) {
      cut_node(h);
      cascading_cut(parent);
    }

    if (comp_(new_key, nodes_[min_root_].key)) {
      min_root_ = h;
    }
  }

  void erase(Handle h) {
    if (!contains(h)) {
      throw std::invalid_argument("Invalid or dead handle");
    }
    erase_internal(h);
  }

  void meld(FibonacciHeap& other) {
    if (&other == this) {
      throw std::invalid_argument("Cannot meld heap with itself");
    }

    if (other.empty()) {
      return;
    }

    if (empty()) {
      *this = std::move(other);
      // The implicitly generated move assignment transfers vectors, but scalar
      // bookkeeping is copied. Leave the source as a valid reusable heap.
      other.min_root_ = kNull;
      other.free_list_ = kNull;
      other.size_ = 0;
      other.roots_count_ = 0;
      other.marked_count_ = 0;
      other.degree_table_.clear();
      return;
    }

    // Move all nodes from other to this, with index adjustment
    std::size_t offset = nodes_.size();
    nodes_.insert(nodes_.end(), other.nodes_.begin(), other.nodes_.end());

    // Adjust all indices in the moved nodes
    for (std::size_t i = offset; i < nodes_.size(); ++i) {
      auto& node = nodes_[i];
      if (node.parent != kNull) node.parent += offset;
      if (node.child != kNull) node.child += offset;
      if (node.left != kNull) node.left += offset;
      if (node.right != kNull) node.right += offset;
    }

    // Adjust other's free list if needed
    if (other.free_list_ != kNull) {
      other.free_list_ += offset;
    }

    // Adjust other's min_root
    Handle other_min_adjusted = other.min_root_;
    if (other_min_adjusted != kNull) {
      other_min_adjusted += offset;
    }

    // Concatenate the root lists
    Handle left_end = nodes_[min_root_].left;
    Handle other_left_end = nodes_[other_min_adjusted].left;

    nodes_[left_end].right = other_min_adjusted;
    nodes_[other_min_adjusted].left = left_end;

    nodes_[other_left_end].right = min_root_;
    nodes_[min_root_].left = other_left_end;

    roots_count_ += other.roots_count_;
    marked_count_ += other.marked_count_;

    if (comp_(nodes_[other_min_adjusted].key, nodes_[min_root_].key)) {
      min_root_ = other_min_adjusted;
    }

    size_ += other.size_;

    // Transfer free list from other to this
    if (other.free_list_ != kNull) {
      // Find the end of other's free list and attach to this heap's free list
      Handle curr = other.free_list_;
      Handle last = curr;
      while (nodes_[last].left != kNull && nodes_[last].left != other.free_list_) {
        last = nodes_[last].left;
      }
      // Attach to this's free list
      if (free_list_ != kNull) {
        nodes_[last].left = free_list_;
        free_list_ = other.free_list_;
      } else {
        free_list_ = other.free_list_;
      }
    }

    // Clear the other heap
    other.nodes_.clear();
    other.size_ = 0;
    other.min_root_ = kNull;
    other.roots_count_ = 0;
    other.marked_count_ = 0;
    other.free_list_ = kNull;
  }

  bool contains(Handle h) const noexcept {
    if (h == kNull || static_cast<std::size_t>(h) >= nodes_.size()) {
      return false;
    }
    return nodes_[h].alive;
  }

  const Key& key(Handle h) const {
    if (!contains(h)) {
      throw std::invalid_argument("Invalid or dead handle");
    }
    return nodes_[h].key;
  }

  Handle parent(Handle h) const {
    if (!contains(h)) {
      throw std::invalid_argument("Invalid or dead handle");
    }
    return nodes_[h].parent;
  }

  std::vector<Handle> children(Handle h) const {
    if (!contains(h)) {
      throw std::invalid_argument("Invalid or dead handle");
    }
    std::vector<Handle> result;
    if (nodes_[h].child == kNull) {
      return result;
    }
    Handle curr = nodes_[h].child;
    do {
      result.push_back(curr);
      curr = nodes_[curr].right;
    } while (curr != nodes_[h].child);
    return result;
  }

  std::size_t degree(Handle h) const {
    if (!contains(h)) {
      throw std::invalid_argument("Invalid or dead handle");
    }
    return nodes_[h].degree;
  }

  bool marked(Handle h) const {
    if (!contains(h)) {
      throw std::invalid_argument("Invalid or dead handle");
    }
    return nodes_[h].marked;
  }

  std::size_t trees() const noexcept { return roots_count_; }

  std::size_t marked_count() const noexcept { return marked_count_; }

  std::size_t max_degree() const noexcept {
    std::size_t maxd = 0;
    for (const auto& node : nodes_) {
      if (node.alive) {
        maxd = std::max(maxd, node.degree);
      }
    }
    return maxd;
  }

  const Stats& stats() const noexcept { return stats_; }

  void check_invariants() const {
    // 1. All roots unmarked
    if (min_root_ != kNull) {
      Handle curr = min_root_;
      do {
        assert(nodes_[curr].parent == kNull);
        assert(!nodes_[curr].marked);
        curr = nodes_[curr].right;
      } while (curr != min_root_);
    }

    // 2. Heap order property
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
      if (!nodes_[i].alive) continue;
      if (nodes_[i].parent != kNull) {
        assert(!comp_(nodes_[i].key, nodes_[nodes_[i].parent].key));
      }
    }

    // 3. Sibling lists are circular and consistent
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
      if (!nodes_[i].alive) continue;
      assert(nodes_[nodes_[i].left].right == i);
      assert(nodes_[nodes_[i].right].left == i);
    }

    // 4. Degrees match child counts
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
      if (!nodes_[i].alive) continue;
      std::size_t count = 0;
      if (nodes_[i].child != kNull) {
        Handle curr = nodes_[i].child;
        do {
          count++;
          curr = nodes_[curr].right;
        } while (curr != nodes_[i].child);
      }
      assert(nodes_[i].degree == count);
    }

    // 5. Size matches count of live nodes
    std::size_t live_count = 0;
    for (const auto& node : nodes_) {
      if (node.alive) live_count++;
    }
    assert(live_count == size_);

    // 6. Min pointer validity
    assert(min_root_ == kNull || nodes_[min_root_].alive);
    if (min_root_ != kNull) {
      Handle curr = min_root_;
      do {
        assert(!comp_(nodes_[curr].key, nodes_[min_root_].key));
        curr = nodes_[curr].right;
      } while (curr != min_root_);
    }

    // 7. Fibonacci size lemma: subtree of degree d has size >= F_{d+2}
    // where F_1=1, F_2=1, F_3=2, F_4=3, F_5=5, ...
    // degree 0 -> size >= F_2 = 1, degree 1 -> size >= F_3 = 2, degree 2 -> size >= F_4 = 3
    std::vector<std::size_t> fib(65);
    fib[1] = 1;
    fib[2] = 1;
    for (std::size_t i = 3; i < 65; ++i) fib[i] = fib[i - 1] + fib[i - 2];

    for (std::size_t i = 0; i < nodes_.size(); ++i) {
      if (!nodes_[i].alive) continue;
      [[maybe_unused]] std::size_t subtree_size = get_subtree_size(i);
      std::size_t d = nodes_[i].degree;
      if (d + 2 < 65) {
        assert(subtree_size >= fib[d + 2]);
      }
    }

    // 8. Marked count
    std::size_t marked_cnt = 0;
    for (const auto& node : nodes_) {
      if (node.alive && node.marked) marked_cnt++;
    }
    assert(marked_cnt == marked_count_);

    // 9. Trees count
    std::size_t trees_cnt = 0;
    if (min_root_ != kNull) {
      Handle curr = min_root_;
      do {
        trees_cnt++;
        curr = nodes_[curr].right;
      } while (curr != min_root_);
    }
    assert(trees_cnt == roots_count_);

    // 10. Degree bound: max degree <= floor(log_phi(n))
    if (size_ > 1) {
      const double kPhi = 1.6180339887498949;
      [[maybe_unused]] std::size_t max_allowed_degree =
          size_ <= 1 ? 0 : static_cast<std::size_t>(std::log(static_cast<double>(size_)) / std::log(kPhi) + 1e-9) + 1;
      assert(max_degree() <= max_allowed_degree);
    }
  }

 private:
  struct Node {
    Key key;
    Handle parent = kNull;
    Handle child = kNull;
    Handle left = kNull;
    Handle right = kNull;
    std::size_t degree = 0;
    bool marked = false;
    bool alive = false;
  };

  std::vector<Node> nodes_;
  Handle min_root_ = kNull;
  Handle free_list_ = kNull;
  std::size_t size_ = 0;
  std::size_t roots_count_ = 0;
  std::size_t marked_count_ = 0;
  Stats stats_;
  Compare comp_;
  std::vector<Handle> degree_table_;

  Handle allocate_node(const Key& key) {
    Handle h;
    if (free_list_ != kNull) {
      h = free_list_;
      free_list_ = nodes_[h].left;
      nodes_[h] = Node{};
      nodes_[h].key = key;
      nodes_[h].alive = true;
    } else {
      h = static_cast<Handle>(nodes_.size());
      nodes_.push_back(Node{key, kNull, kNull, kNull, kNull, 0, false, true});
    }
    return h;
  }

  void free_node(Handle h) {
    nodes_[h].alive = false;
    nodes_[h].left = free_list_;
    free_list_ = h;
  }

  void insert_into_sibling_list(Handle node, Handle sibling) {
    Handle sibling_right = nodes_[sibling].right;
    nodes_[node].left = sibling;
    nodes_[node].right = sibling_right;
    nodes_[sibling].right = node;
    nodes_[sibling_right].left = node;
  }

  void remove_from_sibling_list(Handle node) {
    nodes_[nodes_[node].left].right = nodes_[node].right;
    nodes_[nodes_[node].right].left = nodes_[node].left;
  }

  void cut_node(Handle node) {
    Handle parent = nodes_[node].parent;
    assert(parent != kNull);

    remove_from_sibling_list(node);

    if (nodes_[parent].child == node) {
      if (nodes_[node].left == node) {
        nodes_[parent].child = kNull;
      } else {
        nodes_[parent].child = nodes_[node].left;
      }
    }

    nodes_[parent].degree--;
    nodes_[node].parent = kNull;
    if (nodes_[node].marked) {
      marked_count_--;
      nodes_[node].marked = false;
    }

    // Insert into root list
    if (min_root_ == kNull) {
      min_root_ = node;
      nodes_[node].left = node;
      nodes_[node].right = node;
      roots_count_ = 1;
    } else {
      insert_into_sibling_list(node, min_root_);
      roots_count_++;
    }

    stats_.cuts++;
  }

  void cascading_cut(Handle node) {
    Handle parent = nodes_[node].parent;
    if (parent == kNull) return;

    if (!nodes_[node].marked) {
      nodes_[node].marked = true;
      marked_count_++;
    } else {
      cut_node(node);
      cascading_cut(parent);
    }
  }

  void erase_internal(Handle node) {
    // If node has a parent, recursively cut
    if (nodes_[node].parent != kNull) {
      Handle parent = nodes_[node].parent;
      cut_node(node);
      cascading_cut(parent);
    }

    // Now node is a root. Extract it.
    Handle extract_node = node;

    // First, remove it from the root list
    if (roots_count_ == 1) {
      min_root_ = kNull;
      roots_count_ = 0;
    } else {
      Handle next_root = nodes_[extract_node].right;
      remove_from_sibling_list(extract_node);
      min_root_ = next_root;
      roots_count_--;
    }

    size_--;
    if (nodes_[extract_node].marked) {
      marked_count_--;
      nodes_[extract_node].marked = false;
    }

    // Promote all children to root list
    std::size_t num_children = nodes_[extract_node].degree;
    if (nodes_[extract_node].child != kNull) {
      Handle child_start = nodes_[extract_node].child;
      Handle curr = child_start;
      do {
        Handle next = nodes_[curr].right;
        nodes_[curr].parent = kNull;
        if (nodes_[curr].marked) {
          marked_count_--;
          nodes_[curr].marked = false;
        }
        curr = next;
      } while (curr != child_start);

      // Merge child list with root list
      if (min_root_ != kNull) {
        Handle child_end = nodes_[child_start].left;
        Handle root_left = nodes_[min_root_].left;
        nodes_[root_left].right = child_start;
        nodes_[child_start].left = root_left;
        nodes_[min_root_].left = child_end;
        nodes_[child_end].right = min_root_;
      } else {
        // No other roots, so promote children as the new root list
        min_root_ = child_start;
      }

      stats_.children_promoted += num_children;
      roots_count_ += num_children;
      nodes_[extract_node].child = kNull;
      nodes_[extract_node].degree = 0;
    }

    free_node(extract_node);

    if (size_ == 0) {
      min_root_ = kNull;
      roots_count_ = 0;
      return;
    }

    // Consolidate
    consolidate();
  }

  void consolidate() {
    if (min_root_ == kNull) {
      return;
    }

    std::vector<Handle> roots;

    // Collect all roots
    Handle curr = min_root_;
    do {
      roots.push_back(curr);
      curr = nodes_[curr].right;
    } while (curr != min_root_);

    // Linking a root rewrites its sibling pointers to join a child list. First
    // detach every saved root so that those rewrites cannot leave stale links
    // in the old root ring.
    for (Handle root : roots) {
      nodes_[root].left = root;
      nodes_[root].right = root;
    }

    // Reset root list
    min_root_ = kNull;
    roots_count_ = 0;

    // Clear degree table
    degree_table_.assign(degree_table_.size(), kNull);

    for (Handle root : roots) {
      Handle x = root;
      std::size_t d = nodes_[x].degree;

      while (d < degree_table_.size() && degree_table_[d] != kNull) {
        Handle y = degree_table_[d];
        degree_table_[d] = kNull;

        if (comp_(nodes_[y].key, nodes_[x].key)) {
          std::swap(x, y);
        }

        // Link y under x
        if (nodes_[x].child == kNull) {
          nodes_[x].child = y;
          nodes_[y].left = y;
          nodes_[y].right = y;
        } else {
          insert_into_sibling_list(y, nodes_[x].child);
        }
        nodes_[y].parent = x;
        if (nodes_[y].marked) {
          marked_count_--;
          nodes_[y].marked = false;
        }
        nodes_[x].degree++;
        stats_.links++;

        d++;
      }

      if (d >= degree_table_.size()) {
        degree_table_.resize(d + 1, kNull);
      }
      degree_table_[d] = x;
      stats_.root_visits++;
    }

    // Rebuild root list from degree table
    for (std::size_t i = 0; i < degree_table_.size(); ++i) {
      Handle h = degree_table_[i];
      if (h != kNull) {
        if (min_root_ == kNull) {
          min_root_ = h;
          nodes_[h].left = h;
          nodes_[h].right = h;
          roots_count_ = 1;
        } else {
          insert_into_sibling_list(h, min_root_);
          roots_count_++;
          if (comp_(nodes_[h].key, nodes_[min_root_].key)) {
            min_root_ = h;
          }
        }
      }
    }
  }

  std::size_t get_subtree_size(Handle node) const {
    std::size_t sz = 1;
    if (nodes_[node].child != kNull) {
      Handle curr = nodes_[node].child;
      do {
        sz += get_subtree_size(curr);
        curr = nodes_[curr].right;
      } while (curr != nodes_[node].child);
    }
    return sz;
  }
};

}  // namespace algo
