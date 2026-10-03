#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algo {

class HyperLogLog {
 public:
  explicit HyperLogLog(std::uint8_t precision) : precision_(precision), registers_(std::size_t{1} << precision, 0) {
    if (precision < 4 || precision > 18) throw std::invalid_argument("precision must be in [4,18]");
  }

  void add(std::uint64_t value) {
    const auto h = mix(value);
    const std::size_t bucket = static_cast<std::size_t>(h >> (64 - precision_));
    const std::uint64_t tail = h << precision_;
    const std::uint8_t rank = static_cast<std::uint8_t>(tail == 0 ? (65 - precision_) : (std::countl_zero(tail) + 1));
    if (rank > registers_[bucket]) registers_[bucket] = rank;
  }

  double estimate() const {
    const auto m = static_cast<double>(registers_.size());
    double sum = 0.0; std::size_t zeros = 0;
    for (auto r : registers_) { sum += std::ldexp(1.0, -static_cast<int>(r)); if (r == 0) ++zeros; }
    const double alpha = registers_.size() == 16 ? 0.673 : registers_.size() == 32 ? 0.697 : registers_.size() == 64 ? 0.709 : 0.7213 / (1.0 + 1.079 / m);
    const double raw = alpha * m * m / sum;
    if (raw <= 2.5 * m && zeros != 0) return m * std::log(m / static_cast<double>(zeros));
    if (raw > (1.0 / 30.0) * std::ldexp(1.0, 32)) return -std::ldexp(1.0, 32) * std::log1p(-raw / std::ldexp(1.0, 32));
    return raw;
  }

  bool empty() const noexcept { for (auto r : registers_) if (r != 0) return false; return true; }
  void clear() { std::fill(registers_.begin(), registers_.end(), 0); }
  void merge(const HyperLogLog& other) {
    if (precision_ != other.precision_) throw std::invalid_argument("incompatible HyperLogLog precision");
    for (std::size_t i = 0; i < registers_.size(); ++i) registers_[i] = std::max(registers_[i], other.registers_[i]);
  }
  std::uint8_t precision() const noexcept { return precision_; }

 private:
  std::uint8_t precision_;
  std::vector<std::uint8_t> registers_;
  static std::uint64_t mix(std::uint64_t x) { x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; return x ^ (x >> 31); }
};

}  // namespace algo
