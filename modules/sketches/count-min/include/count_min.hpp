#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algo {

template <class Key>
class CountMinSketch {
 public:
  CountMinSketch(std::size_t width, std::size_t depth, std::uint64_t seed)
      : width_(width), depth_(depth), seed_(seed), counters_(width * depth, 0) {
    if (width == 0 || depth == 0) throw std::invalid_argument("count-min dimensions must be positive");
  }

  void add(const Key& key, std::uint64_t amount = 1) {
    if (std::numeric_limits<std::uint64_t>::max() - total_ < amount) throw std::overflow_error("count-min total overflow");
    for (std::size_t row = 0; row < depth_; ++row) {
      auto& cell = counters_[row * width_ + index(key, row)];
      if (std::numeric_limits<std::uint64_t>::max() - cell < amount) throw std::overflow_error("count-min counter overflow");
      cell += amount;
    }
    total_ += amount;
  }

  std::uint64_t estimate(const Key& key) const {
    std::uint64_t result = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t row = 0; row < depth_; ++row) result = std::min(result, counters_[row * width_ + index(key, row)]);
    return result;
  }

  void merge(const CountMinSketch& other) {
    if (width_ != other.width_ || depth_ != other.depth_ || seed_ != other.seed_) throw std::invalid_argument("incompatible count-min sketches");
    if (std::numeric_limits<std::uint64_t>::max() - total_ < other.total_) throw std::overflow_error("count-min total overflow");
    for (std::size_t i = 0; i < counters_.size(); ++i) {
      if (std::numeric_limits<std::uint64_t>::max() - counters_[i] < other.counters_[i]) throw std::overflow_error("count-min counter overflow");
      counters_[i] += other.counters_[i];
    }
    total_ += other.total_;
  }

  void clear() { std::fill(counters_.begin(), counters_.end(), 0); total_ = 0; }
  std::uint64_t total_count() const noexcept { return total_; }
  std::size_t width() const noexcept { return width_; }
  std::size_t depth() const noexcept { return depth_; }

 private:
  std::size_t width_, depth_;
  std::uint64_t seed_;
  std::vector<std::uint64_t> counters_;
  std::uint64_t total_ = 0;
  static std::uint64_t mix(std::uint64_t x) { x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; return x ^ (x >> 31); }
  std::size_t index(const Key& key, std::size_t row) const { auto h = static_cast<std::uint64_t>(std::hash<Key>{}(key)); return static_cast<std::size_t>(mix(h ^ seed_ ^ (row * 0xd1b54a32d192ed03ULL)) % width_); }
};

}  // namespace algo
