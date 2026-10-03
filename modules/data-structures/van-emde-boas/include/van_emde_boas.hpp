#pragma once

#include <bit>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace algo {

namespace detail {

inline void split_bits(unsigned k, unsigned& high_bits, unsigned& low_bits) {
  low_bits = k / 2;
  high_bits = k - low_bits;
}

inline std::uint32_t high(std::uint32_t x, unsigned low_bits) {
  return x >> low_bits;
}

inline std::uint32_t low(std::uint32_t x, unsigned low_bits) {
  // Do the mask arithmetic wide enough that the boundary width never shifts
  // a 32-bit value by its full width.
  const std::uint64_t mask = low_bits == 0 ? 0 : ((1ULL << low_bits) - 1);
  return static_cast<std::uint32_t>(x & mask);
}

inline std::uint32_t combine(std::uint32_t h, std::uint32_t l, unsigned low_bits) {
  return (h << low_bits) | l;
}

}  // namespace detail

class VebTree {
 public:
  explicit VebTree(unsigned bits);
  ~VebTree() = default;

  VebTree(const VebTree& other);
  VebTree& operator=(const VebTree& other);

  VebTree(VebTree&& other) noexcept = default;
  VebTree& operator=(VebTree&& other) noexcept = default;

  std::uint64_t universe() const;
  std::size_t size() const noexcept;
  bool empty() const noexcept;

  bool insert(std::uint32_t x);
  bool erase(std::uint32_t x);
  bool contains(std::uint32_t x) const;

  std::optional<std::uint32_t> min() const;
  std::optional<std::uint32_t> max() const;
  std::optional<std::uint32_t> successor(std::uint32_t x) const;
  std::optional<std::uint32_t> predecessor(std::uint32_t x) const;

  std::uint32_t last_depth() const;
  std::uint64_t last_calls() const;

  std::size_t memory_bytes() const;
  void check_invariants() const;

 private:
  struct Node {
    unsigned bits = 0;
    std::uint32_t min_val = 0;
    std::uint32_t max_val = 0;
    bool has_min = false;
    bool has_max = false;

    std::uint64_t bitmap = 0;  // For bits <= 6

    // For bits > 6
    std::unique_ptr<Node> summary;
    std::vector<std::unique_ptr<Node>> clusters;

    explicit Node(unsigned b = 0) : bits(b) {
      if (b > 6) {
        unsigned high_bits, low_bits;
        detail::split_bits(b, high_bits, low_bits);
        summary = std::make_unique<Node>(high_bits);
        clusters.resize(1u << high_bits);
        for (auto& cluster : clusters) {
          cluster = std::make_unique<Node>(low_bits);
        }
      }
    }

    Node(const Node& other);
    Node& operator=(const Node& other);
  };

  std::unique_ptr<Node> root;
  unsigned universe_bits = 0;
  mutable unsigned last_depth_ = 0;
  mutable std::uint64_t last_calls_ = 0;
  mutable unsigned current_depth_ = 0;

  bool insert_internal(Node* node, std::uint32_t x);
  bool erase_internal(Node* node, std::uint32_t x);
  bool contains_internal(const Node* node, std::uint32_t x) const;
  std::optional<std::uint32_t> min_internal(const Node* node) const;
  std::optional<std::uint32_t> max_internal(const Node* node) const;
  std::optional<std::uint32_t> successor_internal(const Node* node, std::uint32_t x) const;
  std::optional<std::uint32_t> predecessor_internal(const Node* node, std::uint32_t x) const;

  std::size_t size_internal(const Node* node) const;
  std::size_t memory_bytes_internal(const Node* node) const;
  void check_invariants_internal(const Node* node) const;
};

class SparseVebTree {
 public:
  explicit SparseVebTree(unsigned bits);
  ~SparseVebTree() = default;

  SparseVebTree(const SparseVebTree& other);
  SparseVebTree& operator=(const SparseVebTree& other);

  SparseVebTree(SparseVebTree&& other) noexcept = default;
  SparseVebTree& operator=(SparseVebTree&& other) noexcept = default;

  std::uint64_t universe() const;
  std::size_t size() const noexcept;
  bool empty() const noexcept;

  bool insert(std::uint32_t x);
  bool erase(std::uint32_t x);
  bool contains(std::uint32_t x) const;

  std::optional<std::uint32_t> min() const;
  std::optional<std::uint32_t> max() const;
  std::optional<std::uint32_t> successor(std::uint32_t x) const;
  std::optional<std::uint32_t> predecessor(std::uint32_t x) const;

  std::uint32_t last_depth() const;
  std::uint64_t last_calls() const;

  std::size_t memory_bytes() const;
  void check_invariants() const;

 private:
  struct Node {
    unsigned bits = 0;
    std::uint32_t min_val = 0;
    std::uint32_t max_val = 0;
    bool has_min = false;
    bool has_max = false;

    std::uint64_t bitmap = 0;  // For bits <= 6

    // For bits > 6
    std::unique_ptr<Node> summary;
    std::unordered_map<std::uint32_t, std::unique_ptr<Node>> sparse_clusters;

    explicit Node(unsigned b = 0) : bits(b) {}

    Node(const Node& other);
    Node& operator=(const Node& other);
  };

  std::unique_ptr<Node> root;
  unsigned universe_bits = 0;
  mutable unsigned last_depth_ = 0;
  mutable std::uint64_t last_calls_ = 0;
  mutable unsigned current_depth_ = 0;

  bool insert_internal(Node* node, std::uint32_t x);
  bool erase_internal(Node* node, std::uint32_t x);
  bool contains_internal(const Node* node, std::uint32_t x) const;
  std::optional<std::uint32_t> min_internal(const Node* node) const;
  std::optional<std::uint32_t> max_internal(const Node* node) const;
  std::optional<std::uint32_t> successor_internal(const Node* node, std::uint32_t x) const;
  std::optional<std::uint32_t> predecessor_internal(const Node* node, std::uint32_t x) const;

  std::size_t size_internal(const Node* node) const;
  std::size_t memory_bytes_internal(const Node* node) const;
  void check_invariants_internal(const Node* node) const;

  Node* get_or_create_cluster(Node* node, std::uint32_t idx);
  bool erase_cluster_if_empty(Node* node, std::uint32_t idx);
};

// ============================================================================
// VebTree implementation
// ============================================================================

inline VebTree::Node::Node(const VebTree::Node& other)
    : bits(other.bits),
      min_val(other.min_val),
      max_val(other.max_val),
      has_min(other.has_min),
      has_max(other.has_max),
      bitmap(other.bitmap) {
  if (other.summary) summary = std::make_unique<Node>(*other.summary);
  clusters.resize(other.clusters.size());
  for (std::size_t i = 0; i < other.clusters.size(); ++i) {
    if (other.clusters[i]) clusters[i] = std::make_unique<Node>(*other.clusters[i]);
  }
}

inline VebTree::Node& VebTree::Node::operator=(const VebTree::Node& other) {
  if (this != &other) {
    bits = other.bits;
    min_val = other.min_val;
    max_val = other.max_val;
    has_min = other.has_min;
    has_max = other.has_max;
    bitmap = other.bitmap;
    if (other.summary)
      summary = std::make_unique<Node>(*other.summary);
    else
      summary = nullptr;
    clusters.clear();
    clusters.resize(other.clusters.size());
    for (std::size_t i = 0; i < other.clusters.size(); ++i) {
      if (other.clusters[i]) clusters[i] = std::make_unique<Node>(*other.clusters[i]);
    }
  }
  return *this;
}

inline VebTree::VebTree(unsigned bits) : universe_bits(bits) {
  if (bits == 0 || bits > 24) throw std::invalid_argument("VebTree bits must be in [1, 24]");
  root = std::make_unique<Node>(bits);
}

inline VebTree::VebTree(const VebTree& other)
    : root(other.root ? std::make_unique<Node>(*other.root) : nullptr),
      universe_bits(other.universe_bits) {}

inline VebTree& VebTree::operator=(const VebTree& other) {
  if (this != &other) {
    root = other.root ? std::make_unique<Node>(*other.root) : nullptr;
    universe_bits = other.universe_bits;
    last_depth_ = 0;
    last_calls_ = 0;
  }
  return *this;
}

inline std::uint64_t VebTree::universe() const { return 1ULL << universe_bits; }

inline bool VebTree::empty() const noexcept { return !root || !root->has_min; }

inline std::size_t VebTree::size() const noexcept {
  if (empty()) return 0;
  return size_internal(root.get());
}

inline std::optional<std::uint32_t> VebTree::min() const {
  return min_internal(root.get());
}

inline std::optional<std::uint32_t> VebTree::max() const {
  return max_internal(root.get());
}

inline std::uint32_t VebTree::last_depth() const { return last_depth_; }
inline std::uint64_t VebTree::last_calls() const { return last_calls_; }

inline std::optional<std::uint32_t> VebTree::min_internal(const Node* node) const {
  if (!node || !node->has_min) return std::nullopt;
  return node->min_val;
}

inline std::optional<std::uint32_t> VebTree::max_internal(const Node* node) const {
  if (!node || !node->has_max) return std::nullopt;
  return node->max_val;
}

inline bool VebTree::contains(std::uint32_t x) const {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  if (empty()) return false;
  return contains_internal(root.get(), x);
}

inline bool VebTree::contains_internal(const Node* node, std::uint32_t x) const {
  if (!node || !node->has_min) return false;
  if (x == node->min_val) return true;
  if (!node->has_max || x > node->max_val) return false;
  if (x == node->max_val) return true;
  if (node->bits <= 6) {
    return (node->bitmap >> x) & 1;
  }
  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::uint32_t h = detail::high(x, low_bits);
  std::uint32_t l = detail::low(x, low_bits);
  return contains_internal(node->clusters[h].get(), l);
}

inline bool VebTree::insert(std::uint32_t x) {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  last_depth_ = 0;
  last_calls_ = 0;
  current_depth_ = 0;
  if (!root->has_min) {
    root->min_val = x;
    root->max_val = x;
    root->has_min = true;
    root->has_max = true;
    return true;
  }
  if (x == root->min_val || x == root->max_val) return false;
  if (x < root->min_val) std::swap(x, root->min_val);
  return insert_internal(root.get(), x);
}

inline bool VebTree::insert_internal(Node* node, std::uint32_t x) {
  ++current_depth_;
  ++last_calls_;
  last_depth_ = std::max(last_depth_, current_depth_);

  if (!node->has_min) {
    node->min_val = x;
    node->max_val = x;
    node->has_min = true;
    node->has_max = true;
    --current_depth_;
    return true;
  }

  // A displaced old minimum can equal the cached maximum in a singleton
  // node. Only the current minimum is necessarily represented outside the
  // recursive structure; duplicate maxima are detected in their cluster.
  if (x == node->min_val) {
    --current_depth_;
    return false;
  }
  // The minimum is represented outside the recursive clusters. Keep that
  // invariant at every level, not only at the public root.
  if (x < node->min_val) std::swap(x, node->min_val);

  if (node->bits <= 6) {
    if ((node->bitmap >> x) & 1) {
      --current_depth_;
      return false;
    }
    node->bitmap |= 1ULL << x;
    node->max_val = std::max(node->max_val, x);
    if (!node->has_max) node->has_max = true;
    --current_depth_;
    return true;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::uint32_t h = detail::high(x, low_bits);
  std::uint32_t l = detail::low(x, low_bits);

  // If cluster is empty, insert high into summary
  if (!node->clusters[h]->has_min) {
    ++current_depth_;
    ++last_calls_;
    last_depth_ = std::max(last_depth_, current_depth_);
    bool summary_inserted = insert_internal(node->summary.get(), h);
    --current_depth_;
    if (!summary_inserted) {
      --current_depth_;
      return false;  // Duplicate in summary (shouldn't happen for empty cluster)
    }
    node->clusters[h]->min_val = l;
    node->clusters[h]->max_val = l;
    node->clusters[h]->has_min = true;
    node->clusters[h]->has_max = true;
  } else {
    if (!insert_internal(node->clusters[h].get(), l)) {
      --current_depth_;
      return false;  // Duplicate found in cluster
    }
  }

  node->max_val = std::max(node->max_val, x);
  if (!node->has_max) node->has_max = true;
  --current_depth_;
  return true;
}

inline bool VebTree::erase(std::uint32_t x) {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  last_depth_ = 0;
  last_calls_ = 0;
  current_depth_ = 0;
  return erase_internal(root.get(), x);
}

inline bool VebTree::erase_internal(Node* node, std::uint32_t x) {
  ++current_depth_;
  ++last_calls_;
  last_depth_ = std::max(last_depth_, current_depth_);

  if (!node->has_min || x < node->min_val || x > node->max_val) {
    --current_depth_;
    return false;
  }
  if (node->min_val == node->max_val) {
    if (x != node->min_val) { --current_depth_; return false; }
    node->has_min = node->has_max = false;
    node->bitmap = 0;
    --current_depth_;
    return true;
  }

  if (node->bits <= 6) {
    if (x == node->min_val) {
      const std::uint64_t members = node->bitmap;
      if (!members) {
        node->has_min = node->has_max = false;
        node->bitmap = 0;
      } else {
        node->min_val = std::countr_zero(members);
        node->bitmap &= ~(1ULL << node->min_val);
        node->max_val = 63 - std::countl_zero(members);
      }
      --current_depth_;
      return true;
    }
    if (!((node->bitmap >> x) & 1)) { --current_depth_; return false; }
    node->bitmap &= ~(1ULL << x);
    if (!node->bitmap) {
      // The minimum is stored outside the bitmap. Removing the only other
      // member leaves a singleton, not an empty node.
      node->max_val = node->min_val;
    } else {
      node->max_val = 63 - std::countl_zero(node->bitmap);
    }
    --current_depth_;
    return true;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  const bool removing_min = x == node->min_val;
  if (removing_min) {
    const auto first_h = node->summary->min_val;
    const auto first_l = node->clusters[first_h]->min_val;
    node->min_val = detail::combine(first_h, first_l, low_bits);
    x = node->min_val;
  }
  const auto h = detail::high(x, low_bits);
  const auto l = detail::low(x, low_bits);
  const bool found = erase_internal(node->clusters[h].get(), l);
  if (found && !node->clusters[h]->has_min) erase_internal(node->summary.get(), h);
  if (found) {
    if (!node->summary->has_max) node->max_val = node->min_val;
    else {
      const auto last_h = node->summary->max_val;
      node->max_val = detail::combine(last_h, node->clusters[last_h]->max_val, low_bits);
    }
  }
  --current_depth_;
  return found;
}

inline std::optional<std::uint32_t> VebTree::successor(std::uint32_t x) const {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  last_depth_ = 0;
  last_calls_ = 0;
  current_depth_ = 0;
  return successor_internal(root.get(), x);
}

inline std::optional<std::uint32_t> VebTree::successor_internal(const Node* node, std::uint32_t x) const {
  ++current_depth_;
  ++last_calls_;
  last_depth_ = std::max(last_depth_, current_depth_);

  if (!node || !node->has_min) {
    --current_depth_;
    return std::nullopt;
  }

  if (node->bits <= 6) {
    if (x < node->min_val) { --current_depth_; return node->min_val; }
    for (std::uint32_t i = x + 1; i < (1u << node->bits); ++i) {
      if ((node->bitmap >> i) & 1) {
        --current_depth_;
        return i;
      }
    }
    --current_depth_;
    return std::nullopt;
  }

  if (x < node->min_val) {
    --current_depth_;
    return node->min_val;
  }

  if (x >= node->max_val) {
    --current_depth_;
    return std::nullopt;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::uint32_t h = detail::high(x, low_bits);
  std::uint32_t l = detail::low(x, low_bits);

  if (node->clusters[h]->has_max && l < node->clusters[h]->max_val) {
    auto res = successor_internal(node->clusters[h].get(), l);
    if (res) {
      --current_depth_;
      return detail::combine(h, *res, low_bits);
    }
  }

  auto next_h_opt = successor_internal(node->summary.get(), h);
  if (!next_h_opt) {
    --current_depth_;
    return std::nullopt;
  }
  std::uint32_t next_h = *next_h_opt;
  std::uint32_t next_l = node->clusters[next_h]->min_val;
  --current_depth_;
  return detail::combine(next_h, next_l, low_bits);
}

inline std::optional<std::uint32_t> VebTree::predecessor(std::uint32_t x) const {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  last_depth_ = 0;
  last_calls_ = 0;
  current_depth_ = 0;
  return predecessor_internal(root.get(), x);
}

inline std::optional<std::uint32_t> VebTree::predecessor_internal(const Node* node, std::uint32_t x) const {
  ++current_depth_;
  ++last_calls_;
  last_depth_ = std::max(last_depth_, current_depth_);

  if (!node || !node->has_min) {
    --current_depth_;
    return std::nullopt;
  }

  if (node->bits <= 6) {
    if (x > node->max_val) { --current_depth_; return node->max_val; }
    if (x <= node->min_val) { --current_depth_; return std::nullopt; }
    for (std::int32_t i = static_cast<std::int32_t>(x) - 1; i >= 0; --i) {
      if ((node->bitmap >> i) & 1) {
        --current_depth_;
        return static_cast<std::uint32_t>(i);
      }
    }
    --current_depth_;
    return node->min_val;
  }

  if (x > node->max_val) {
    --current_depth_;
    return node->max_val;
  }

  if (x <= node->min_val) {
    --current_depth_;
    return std::nullopt;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::uint32_t h = detail::high(x, low_bits);
  std::uint32_t l = detail::low(x, low_bits);

  if (node->clusters[h]->has_min && l > node->clusters[h]->min_val) {
    auto res = predecessor_internal(node->clusters[h].get(), l);
    if (res) {
      --current_depth_;
      return detail::combine(h, *res, low_bits);
    }
  }

  auto prev_h_opt = predecessor_internal(node->summary.get(), h);
  if (!prev_h_opt) {
    --current_depth_;
    return node->min_val;
  }
  std::uint32_t prev_h = *prev_h_opt;
  std::uint32_t prev_l = node->clusters[prev_h]->max_val;
  --current_depth_;
  return detail::combine(prev_h, prev_l, low_bits);
}

inline std::size_t VebTree::size_internal(const Node* node) const {
  if (!node || !node->has_min) return 0;
  if (node->min_val == node->max_val) return 1;

  if (node->bits <= 6) {
    return 1 + std::popcount(node->bitmap);
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::size_t count = 1;  // min
  for (std::uint32_t h = 0; h < (1u << high_bits); ++h) {
    count += size_internal(node->clusters[h].get());
  }
  return count;
}

inline std::size_t VebTree::memory_bytes() const {
  return memory_bytes_internal(root.get());
}

inline std::size_t VebTree::memory_bytes_internal(const Node* node) const {
  if (!node) return 0;
  std::size_t bytes = sizeof(Node);
  if (node->bits <= 6) return bytes;

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  bytes += node->clusters.capacity() * sizeof(std::unique_ptr<Node>);
  if (node->summary) bytes += memory_bytes_internal(node->summary.get());
  for (const auto& c : node->clusters) bytes += memory_bytes_internal(c.get());
  return bytes;
}

inline void VebTree::check_invariants() const {
  if (root) check_invariants_internal(root.get());
}

inline void VebTree::check_invariants_internal(const Node* node) const {
  if (!node) return;

  if (node->has_min && node->has_max) {
    if (node->min_val > node->max_val) throw std::logic_error("min > max");
  } else if (node->has_min != node->has_max) {
    throw std::logic_error("has_min != has_max");
  }

  if (!node->has_min && !node->has_max) return;

  if (node->bits <= 6) {
    const std::uint64_t valid_mask = node->bits == 6 ? ~0ULL : (1ULL << (1u << node->bits)) - 1;
    if (node->bitmap & ~valid_mask) throw std::logic_error("bitmap exceeds leaf universe");
    if (node->bitmap & (1ULL << node->min_val)) throw std::logic_error("minimum duplicated in bitmap");
    if (node->has_max && node->bitmap) {
      std::uint32_t msb = 63 - std::countl_zero(node->bitmap);
      if (node->max_val != msb) throw std::logic_error("max inconsistent with bitmap");
    }
    if (node->has_max && !node->bitmap && node->min_val != node->max_val) throw std::logic_error("empty bitmap bounds differ");
    return;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);

  if (node->summary) check_invariants_internal(node->summary.get());
  for (const auto& c : node->clusters) check_invariants_internal(c.get());

  for (std::uint32_t h = 0; h < (1u << high_bits); ++h) {
    if (node->clusters[h] && node->clusters[h]->has_min) {
      if (!node->summary || !contains_internal(node->summary.get(), h)) {
        throw std::logic_error("cluster non-empty but summary missing");
      }
    }
  }

  if (node->min_val != node->max_val) {
    std::uint32_t h_max = detail::high(node->max_val, low_bits);
    std::uint32_t l_max = detail::low(node->max_val, low_bits);
    if (!node->clusters[h_max] || !contains_internal(node->clusters[h_max].get(), l_max)) {
      throw std::logic_error("max not in its cluster");
    }
  }
}

// ============================================================================
// SparseVebTree implementation
// ============================================================================

inline SparseVebTree::Node::Node(const SparseVebTree::Node& other)
    : bits(other.bits),
      min_val(other.min_val),
      max_val(other.max_val),
      has_min(other.has_min),
      has_max(other.has_max),
      bitmap(other.bitmap) {
  if (other.summary) summary = std::make_unique<Node>(*other.summary);
  for (const auto& [idx, node] : other.sparse_clusters) {
    sparse_clusters[idx] = std::make_unique<Node>(*node);
  }
}

inline SparseVebTree::Node& SparseVebTree::Node::operator=(const SparseVebTree::Node& other) {
  if (this != &other) {
    bits = other.bits;
    min_val = other.min_val;
    max_val = other.max_val;
    has_min = other.has_min;
    has_max = other.has_max;
    bitmap = other.bitmap;
    if (other.summary)
      summary = std::make_unique<Node>(*other.summary);
    else
      summary = nullptr;
    sparse_clusters.clear();
    for (const auto& [idx, node] : other.sparse_clusters) {
      sparse_clusters[idx] = std::make_unique<Node>(*node);
    }
  }
  return *this;
}

inline SparseVebTree::SparseVebTree(unsigned bits) : universe_bits(bits) {
  if (bits == 0 || bits > 32) throw std::invalid_argument("SparseVebTree bits must be in [1, 32]");
  root = std::make_unique<Node>(bits);
}

inline SparseVebTree::SparseVebTree(const SparseVebTree& other)
    : root(other.root ? std::make_unique<Node>(*other.root) : nullptr),
      universe_bits(other.universe_bits) {}

inline SparseVebTree& SparseVebTree::operator=(const SparseVebTree& other) {
  if (this != &other) {
    root = other.root ? std::make_unique<Node>(*other.root) : nullptr;
    universe_bits = other.universe_bits;
    last_depth_ = 0;
    last_calls_ = 0;
  }
  return *this;
}

inline std::uint64_t SparseVebTree::universe() const { return 1ULL << universe_bits; }

inline bool SparseVebTree::empty() const noexcept { return !root || !root->has_min; }

inline std::size_t SparseVebTree::size() const noexcept {
  if (empty()) return 0;
  return size_internal(root.get());
}

inline std::optional<std::uint32_t> SparseVebTree::min() const {
  return min_internal(root.get());
}

inline std::optional<std::uint32_t> SparseVebTree::max() const {
  return max_internal(root.get());
}

inline std::uint32_t SparseVebTree::last_depth() const { return last_depth_; }
inline std::uint64_t SparseVebTree::last_calls() const { return last_calls_; }

inline std::optional<std::uint32_t> SparseVebTree::min_internal(const Node* node) const {
  if (!node || !node->has_min) return std::nullopt;
  return node->min_val;
}

inline std::optional<std::uint32_t> SparseVebTree::max_internal(const Node* node) const {
  if (!node || !node->has_max) return std::nullopt;
  return node->max_val;
}

inline bool SparseVebTree::contains(std::uint32_t x) const {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  if (empty()) return false;
  return contains_internal(root.get(), x);
}

inline bool SparseVebTree::contains_internal(const Node* node, std::uint32_t x) const {
  if (!node || !node->has_min) return false;
  if (x == node->min_val) return true;
  if (!node->has_max || x > node->max_val) return false;
  if (x == node->max_val) return true;
  if (node->bits <= 6) {
    return (node->bitmap >> x) & 1;
  }
  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::uint32_t h = detail::high(x, low_bits);
  std::uint32_t l = detail::low(x, low_bits);
  auto it = node->sparse_clusters.find(h);
  if (it == node->sparse_clusters.end()) return false;
  return contains_internal(it->second.get(), l);
}

inline SparseVebTree::Node* SparseVebTree::get_or_create_cluster(Node* node, std::uint32_t idx) {
  auto it = node->sparse_clusters.find(idx);
  if (it != node->sparse_clusters.end()) return it->second.get();
  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  node->sparse_clusters[idx] = std::make_unique<Node>(low_bits);
  return node->sparse_clusters[idx].get();
}

inline bool SparseVebTree::erase_cluster_if_empty(Node* node, std::uint32_t idx) {
  auto it = node->sparse_clusters.find(idx);
  if (it != node->sparse_clusters.end() && !it->second->has_min) {
    node->sparse_clusters.erase(it);
    return true;
  }
  return false;
}

inline bool SparseVebTree::insert(std::uint32_t x) {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  last_depth_ = 0;
  last_calls_ = 0;
  current_depth_ = 0;
  if (!root->has_min) {
    root->min_val = x;
    root->max_val = x;
    root->has_min = true;
    root->has_max = true;
    return true;
  }
  if (x == root->min_val || x == root->max_val) return false;
  if (x < root->min_val) std::swap(x, root->min_val);
  return insert_internal(root.get(), x);
}

inline bool SparseVebTree::insert_internal(Node* node, std::uint32_t x) {
  ++current_depth_;
  ++last_calls_;
  last_depth_ = std::max(last_depth_, current_depth_);

  if (!node->has_min) {
    node->min_val = x;
    node->max_val = x;
    node->has_min = true;
    node->has_max = true;
    --current_depth_;
    return true;
  }

  if (x == node->min_val) {
    --current_depth_;
    return false;
  }
  if (x < node->min_val) std::swap(x, node->min_val);

  if (node->bits <= 6) {
    if ((node->bitmap >> x) & 1) {
      --current_depth_;
      return false;
    }
    node->bitmap |= 1ULL << x;
    node->max_val = std::max(node->max_val, x);
    if (!node->has_max) node->has_max = true;
    --current_depth_;
    return true;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::uint32_t h = detail::high(x, low_bits);
  std::uint32_t l = detail::low(x, low_bits);

  Node* cluster = get_or_create_cluster(node, h);
  if (!cluster->has_min) {
    if (!node->summary) node->summary = std::make_unique<Node>(high_bits);
    bool summary_inserted = insert_internal(node->summary.get(), h);
    if (!summary_inserted) {
      --current_depth_;
      return false;
    }
    cluster->min_val = l;
    cluster->max_val = l;
    cluster->has_min = true;
    cluster->has_max = true;
  } else {
    if (!insert_internal(cluster, l)) {
      --current_depth_;
      return false;
    }
  }

  node->max_val = std::max(node->max_val, x);
  if (!node->has_max) node->has_max = true;
  --current_depth_;
  return true;
}

inline bool SparseVebTree::erase(std::uint32_t x) {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  last_depth_ = 0;
  last_calls_ = 0;
  current_depth_ = 0;
  return erase_internal(root.get(), x);
}

inline bool SparseVebTree::erase_internal(Node* node, std::uint32_t x) {
  ++current_depth_;
  ++last_calls_;
  last_depth_ = std::max(last_depth_, current_depth_);

  if (!node->has_min) {
    --current_depth_;
    return false;
  }

  if (x == node->min_val) {
    if (node->min_val == node->max_val) {
      node->has_min = node->has_max = false;
      node->bitmap = 0;
      --current_depth_;
      return true;
    }
    if (node->bits <= 6) {
      const auto members = node->bitmap;
      // A non-singleton leaf normally stores every key other than min in the
      // bitmap.  Keep deletion safe if an earlier recursive update left only
      // the cached maximum behind: that maximum is still the sole remaining
      // key, so promote it instead of calling countr_zero(0).
      if (members == 0) {
        node->min_val = node->max_val;
        --current_depth_;
        return true;
      }
      node->min_val = std::countr_zero(members);
      node->bitmap &= ~(1ULL << node->min_val);
      node->max_val = 63 - std::countl_zero(members);
      --current_depth_;
      return true;
    }
    unsigned hb, lb;
    detail::split_bits(node->bits, hb, lb);
    const auto h = node->summary->min_val;
    auto it = node->sparse_clusters.find(h);
    const auto l = it->second->min_val;
    node->min_val = detail::combine(h, l, lb);
    const bool ok = erase_internal(it->second.get(), l);
    if (!it->second->has_min) {
      erase_internal(node->summary.get(), h);
      erase_cluster_if_empty(node, h);
    }
    if (!node->summary || !node->summary->has_max) node->max_val = node->min_val;
    else {
      const auto last_h = node->summary->max_val;
      node->max_val = detail::combine(last_h, node->sparse_clusters.at(last_h)->max_val, lb);
    }
    --current_depth_;
    return ok;
  }

  if (x == node->min_val || x == node->max_val) {
    if (x != node->min_val) {
      // x == max_val, erase from structure
      if (node->bits <= 6) {
        if (!((node->bitmap >> x) & 1)) {
          --current_depth_;
          return false;
        }
        node->bitmap &= ~(1ULL << x);
        if (!node->bitmap) {
          // The minimum is represented separately from bitmap members.
          node->max_val = node->min_val;
        } else {
          std::uint32_t msb = 63 - std::countl_zero(node->bitmap);
          node->max_val = msb;
        }
        --current_depth_;
        return true;
      }
      unsigned high_bits, low_bits;
      detail::split_bits(node->bits, high_bits, low_bits);
      std::uint32_t h = detail::high(x, low_bits);
      std::uint32_t l = detail::low(x, low_bits);
      auto it = node->sparse_clusters.find(h);
      if (it == node->sparse_clusters.end()) {
        --current_depth_;
        return false;
      }
      bool found = erase_internal(it->second.get(), l);
      if (found && !it->second->has_min) {
        if (node->summary) {
          erase_internal(node->summary.get(), h);
        }
        erase_cluster_if_empty(node, h);
      }
      // Update max
      if (found) {
        if (!node->summary || !node->summary->has_max) {
          node->max_val = node->min_val;
        } else {
          std::uint32_t last_h = node->summary->max_val;
          auto last_it = node->sparse_clusters.find(last_h);
          if (last_it == node->sparse_clusters.end()) {
            node->max_val = node->min_val;
          } else {
            std::uint32_t last_l = last_it->second->max_val;
            node->max_val = detail::combine(last_h, last_l, low_bits);
          }
        }
      }
      --current_depth_;
      return found;
    }
  }

  // x != min and x != max
  if (node->bits <= 6) {
    if (!((node->bitmap >> x) & 1)) {
      --current_depth_;
      return false;
    }
    node->bitmap &= ~(1ULL << x);
    node->max_val = node->bitmap ? 63 - std::countl_zero(node->bitmap) : node->min_val;
    --current_depth_;
    return true;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::uint32_t h = detail::high(x, low_bits);
  std::uint32_t l = detail::low(x, low_bits);

  auto it = node->sparse_clusters.find(h);
  if (it == node->sparse_clusters.end()) {
    --current_depth_;
    return false;
  }

  bool found = erase_internal(it->second.get(), l);
  if (found && !it->second->has_min) {
    if (node->summary) {
      erase_internal(node->summary.get(), h);
    }
    erase_cluster_if_empty(node, h);
  }
  --current_depth_;
  return found;
}

inline std::optional<std::uint32_t> SparseVebTree::successor(std::uint32_t x) const {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  last_depth_ = 0;
  last_calls_ = 0;
  current_depth_ = 0;
  return successor_internal(root.get(), x);
}

inline std::optional<std::uint32_t> SparseVebTree::successor_internal(const Node* node, std::uint32_t x) const {
  ++current_depth_;
  ++last_calls_;
  last_depth_ = std::max(last_depth_, current_depth_);

  if (!node || !node->has_min) {
    --current_depth_;
    return std::nullopt;
  }

  if (node->bits <= 6) {
    if (x < node->min_val) { --current_depth_; return node->min_val; }
    for (std::uint32_t i = x + 1; i < (1u << node->bits); ++i) {
      if ((node->bitmap >> i) & 1) {
        --current_depth_;
        return i;
      }
    }
    --current_depth_;
    return std::nullopt;
  }

  if (x < node->min_val) {
    --current_depth_;
    return node->min_val;
  }

  if (x >= node->max_val) {
    --current_depth_;
    return std::nullopt;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::uint32_t h = detail::high(x, low_bits);
  std::uint32_t l = detail::low(x, low_bits);

  auto it = node->sparse_clusters.find(h);
  if (it != node->sparse_clusters.end() && it->second->has_max && l < it->second->max_val) {
    auto res = successor_internal(it->second.get(), l);
    if (res) {
      --current_depth_;
      return detail::combine(h, *res, low_bits);
    }
  }

  if (!node->summary) {
    --current_depth_;
    return std::nullopt;
  }

  auto next_h_opt = successor_internal(node->summary.get(), h);
  if (!next_h_opt) {
    --current_depth_;
    return std::nullopt;
  }
  std::uint32_t next_h = *next_h_opt;
  auto next_it = node->sparse_clusters.find(next_h);
  if (next_it == node->sparse_clusters.end()) {
    --current_depth_;
    return std::nullopt;
  }
  std::uint32_t next_l = next_it->second->min_val;
  --current_depth_;
  return detail::combine(next_h, next_l, low_bits);
}

inline std::optional<std::uint32_t> SparseVebTree::predecessor(std::uint32_t x) const {
  if (x >= universe()) throw std::out_of_range("x >= universe()");
  last_depth_ = 0;
  last_calls_ = 0;
  current_depth_ = 0;
  return predecessor_internal(root.get(), x);
}

inline std::optional<std::uint32_t> SparseVebTree::predecessor_internal(const Node* node, std::uint32_t x) const {
  ++current_depth_;
  ++last_calls_;
  last_depth_ = std::max(last_depth_, current_depth_);

  if (!node || !node->has_min) {
    --current_depth_;
    return std::nullopt;
  }

  if (node->bits <= 6) {
    if (x > node->max_val) { --current_depth_; return node->max_val; }
    if (x <= node->min_val) { --current_depth_; return std::nullopt; }
    for (std::int32_t i = static_cast<std::int32_t>(x) - 1; i >= 0; --i) {
      if ((node->bitmap >> i) & 1) {
        --current_depth_;
        return static_cast<std::uint32_t>(i);
      }
    }
    --current_depth_;
    return node->min_val;
  }

  if (x > node->max_val) {
    --current_depth_;
    return node->max_val;
  }

  if (x <= node->min_val) {
    --current_depth_;
    return std::nullopt;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);
  std::uint32_t h = detail::high(x, low_bits);
  std::uint32_t l = detail::low(x, low_bits);

  auto it = node->sparse_clusters.find(h);
  if (it != node->sparse_clusters.end() && it->second->has_min && l > it->second->min_val) {
    auto res = predecessor_internal(it->second.get(), l);
    if (res) {
      --current_depth_;
      return detail::combine(h, *res, low_bits);
    }
  }

  if (!node->summary) {
    --current_depth_;
    return std::nullopt;
  }

  auto prev_h_opt = predecessor_internal(node->summary.get(), h);
  if (!prev_h_opt) {
    --current_depth_;
    return node->min_val;
  }
  std::uint32_t prev_h = *prev_h_opt;
  auto prev_it = node->sparse_clusters.find(prev_h);
  if (prev_it == node->sparse_clusters.end()) {
    --current_depth_;
    return std::nullopt;
  }
  std::uint32_t prev_l = prev_it->second->max_val;
  --current_depth_;
  return detail::combine(prev_h, prev_l, low_bits);
}

inline std::size_t SparseVebTree::size_internal(const Node* node) const {
  if (!node || !node->has_min) return 0;
  if (node->min_val == node->max_val) return 1;

  if (node->bits <= 6) {
    return 1 + std::popcount(node->bitmap);
  }

  std::size_t count = 1;  // min
  for (const auto& [_, cluster] : node->sparse_clusters) {
    count += size_internal(cluster.get());
  }
  return count;
}

inline std::size_t SparseVebTree::memory_bytes() const {
  return memory_bytes_internal(root.get());
}

inline std::size_t SparseVebTree::memory_bytes_internal(const Node* node) const {
  if (!node) return 0;
  std::size_t bytes = sizeof(Node);
  if (node->bits <= 6) return bytes;

  // Add unordered_map overhead
  bytes += node->sparse_clusters.size() * sizeof(std::pair<std::uint32_t, std::unique_ptr<Node>>);
  bytes += node->sparse_clusters.bucket_count() * sizeof(void*);

  if (node->summary) bytes += memory_bytes_internal(node->summary.get());
  for (const auto& [_, cluster] : node->sparse_clusters) bytes += memory_bytes_internal(cluster.get());
  return bytes;
}

inline void SparseVebTree::check_invariants() const {
  if (root) check_invariants_internal(root.get());
}

inline void SparseVebTree::check_invariants_internal(const Node* node) const {
  if (!node) return;

  if (node->has_min && node->has_max) {
    if (node->min_val > node->max_val) throw std::logic_error("min > max");
  } else if (node->has_min != node->has_max) {
    throw std::logic_error("has_min != has_max");
  }

  if (!node->has_min && !node->has_max) return;

  if (node->bits <= 6) {
    const std::uint64_t valid_mask = node->bits == 6 ? ~0ULL : (1ULL << (1u << node->bits)) - 1;
    if (node->bitmap & ~valid_mask) throw std::logic_error("bitmap exceeds leaf universe");
    if (node->bitmap & (1ULL << node->min_val)) throw std::logic_error("minimum duplicated in bitmap");
    if (node->has_max && node->bitmap) {
      std::uint32_t msb = 63 - std::countl_zero(node->bitmap);
      if (node->max_val != msb) throw std::logic_error("max inconsistent with bitmap");
    }
    if (node->has_max && !node->bitmap && node->min_val != node->max_val) throw std::logic_error("empty bitmap bounds differ");
    return;
  }

  unsigned high_bits, low_bits;
  detail::split_bits(node->bits, high_bits, low_bits);

  if (node->summary) check_invariants_internal(node->summary.get());
  for (const auto& [_, cluster] : node->sparse_clusters) check_invariants_internal(cluster.get());

  for (const auto& [h, cluster] : node->sparse_clusters) {
    if (cluster && cluster->has_min) {
      if (!node->summary || !contains_internal(node->summary.get(), h)) {
        throw std::logic_error("cluster non-empty but summary missing");
      }
    }
  }

  if (node->min_val != node->max_val) {
    std::uint32_t h_max = detail::high(node->max_val, low_bits);
    std::uint32_t l_max = detail::low(node->max_val, low_bits);
    auto it = node->sparse_clusters.find(h_max);
    if (it == node->sparse_clusters.end() || !contains_internal(it->second.get(), l_max)) {
      throw std::logic_error("max not in its cluster");
    }
  }
}

}  // namespace algo
