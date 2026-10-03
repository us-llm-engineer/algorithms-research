#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algo {

class DpOptimization {
 public:
  // Minimum sum of squared segment sums over exactly `segments` nonempty,
  // contiguous segments. Divide-and-conquer optimization is valid for
  // nonnegative inputs; signed inputs use the exact quadratic recurrence.
  static std::int64_t partition_squared(const std::vector<std::int64_t>& values,
                                        std::size_t segments) {
    validate_partition(values, segments);
    if (!all_nonnegative(values)) return partition_squared_naive(values, segments);

    const std::size_t n = values.size();
    const auto prefix = prefix_sums(values);
    constexpr auto inf = std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> previous(n + 1, inf), current(n + 1, inf);
    previous[0] = 0;
    for (std::size_t group = 1; group <= segments; ++group) {
      std::fill(current.begin(), current.end(), inf);
      solve_partition_layer(prefix, previous, current, group, n, group - 1, n - 1);
      previous.swap(current);
    }
    return previous[n];
  }

  static std::int64_t partition_squared_naive(const std::vector<std::int64_t>& values,
                                              std::size_t segments) {
    validate_partition(values, segments);
    const std::size_t n = values.size();
    const auto prefix = prefix_sums(values);
    constexpr auto inf = std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> previous(n + 1, inf), current(n + 1, inf);
    previous[0] = 0;
    for (std::size_t group = 1; group <= segments; ++group) {
      std::fill(current.begin(), current.end(), inf);
      for (std::size_t end = group; end <= n; ++end) {
        for (std::size_t split = group - 1; split < end; ++split) {
          if (previous[split] == inf) continue;
          current[end] = std::min(current[end], add_cost(
              previous[split], square_difference(prefix[end], prefix[split])));
        }
      }
      previous.swap(current);
    }
    return previous[n];
  }

  static bool partition_decisions_monotone(const std::vector<std::int64_t>& values,
                                           std::size_t segments) {
    if (values.empty() || segments == 0 || segments > values.size()) return false;
    const std::size_t n = values.size();
    const auto prefix = prefix_sums(values);
    constexpr auto inf = std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> previous(n + 1, inf), current(n + 1, inf);
    previous[0] = 0;
    for (std::size_t group = 1; group <= segments; ++group) {
      std::fill(current.begin(), current.end(), inf);
      std::size_t prior_argmin = group - 1;
      for (std::size_t end = group; end <= n; ++end) {
        std::size_t best_split = group - 1;
        std::int64_t best = inf;
        for (std::size_t split = group - 1; split < end; ++split) {
          if (previous[split] == inf) continue;
          const auto candidate = add_cost(previous[split], square_difference(prefix[end], prefix[split]));
          if (candidate < best) {
            best = candidate;
            best_split = split;
          }
        }
        current[end] = best;
        if (best_split < prior_argmin) return false;
        prior_argmin = best_split;
      }
      previous.swap(current);
    }
    return true;
  }

  // Minimum cost of repeatedly merging adjacent piles, where a merge costs
  // the sum of the weights in the resulting interval.
  static std::int64_t optimal_merge(const std::vector<std::int64_t>& weights) {
    const std::size_t n = weights.size();
    if (n < 2) return 0;
    if (!all_nonnegative(weights)) return optimal_merge_naive(weights);

    const auto prefix = prefix_sums(weights);
    std::vector<std::vector<std::int64_t>> dp(n, std::vector<std::int64_t>(n, 0));
    std::vector<std::vector<std::size_t>> opt(n, std::vector<std::size_t>(n));
    for (std::size_t i = 0; i < n; ++i) opt[i][i] = i;
    for (std::size_t length = 2; length <= n; ++length) {
      for (std::size_t left = 0; left + length <= n; ++left) {
        const std::size_t right = left + length - 1;
        const std::size_t lo = std::max(left, opt[left][right - 1]);
        const std::size_t hi = std::min(right - 1, opt[left + 1][right]);
        auto best = std::numeric_limits<std::int64_t>::max();
        std::size_t best_split = lo;
        for (std::size_t split = lo; split <= hi; ++split) {
          const auto candidate = add_cost(add_cost(dp[left][split], dp[split + 1][right]),
                                          prefix[right + 1] - prefix[left]);
          if (candidate < best) {
            best = candidate;
            best_split = split;
          }
        }
        dp[left][right] = best;
        opt[left][right] = best_split;
      }
    }
    return dp[0][n - 1];
  }

  static std::int64_t optimal_merge_naive(const std::vector<std::int64_t>& weights) {
    const std::size_t n = weights.size();
    if (n < 2) return 0;
    const auto prefix = prefix_sums(weights);
    std::vector<std::vector<std::int64_t>> dp(n, std::vector<std::int64_t>(n, 0));
    for (std::size_t length = 2; length <= n; ++length) {
      for (std::size_t left = 0; left + length <= n; ++left) {
        const std::size_t right = left + length - 1;
        auto best = std::numeric_limits<std::int64_t>::max();
        for (std::size_t split = left; split < right; ++split) {
          best = std::min(best, add_cost(add_cost(dp[left][split], dp[split + 1][right]),
                                         prefix[right + 1] - prefix[left]));
        }
        dp[left][right] = best;
      }
    }
    return dp[0][n - 1];
  }

 private:
  static void validate_partition(const std::vector<std::int64_t>& values, std::size_t segments) {
    if (values.empty() || segments == 0 || segments > values.size())
      throw std::invalid_argument("partition requires 1..n nonempty segments");
  }

  static bool all_nonnegative(const std::vector<std::int64_t>& values) {
    return std::all_of(values.begin(), values.end(), [](std::int64_t x) { return x >= 0; });
  }

  static std::vector<std::int64_t> prefix_sums(const std::vector<std::int64_t>& values) {
    std::vector<std::int64_t> prefix(values.size() + 1, 0);
    for (std::size_t i = 0; i < values.size(); ++i)
      prefix[i + 1] = add_cost(prefix[i], values[i]);
    return prefix;
  }

  static std::int64_t add_cost(std::int64_t a, std::int64_t b) {
    if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
        (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b))
      throw std::overflow_error("dynamic programming cost exceeds int64_t");
    return a + b;
  }

  static std::int64_t square_difference(std::int64_t a, std::int64_t b) {
    const std::uint64_t difference = a >= b
        ? static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b)
        : static_cast<std::uint64_t>(b) - static_cast<std::uint64_t>(a);
    constexpr std::uint64_t max_root = 3'037'000'499ULL;
    if (difference > max_root)
      throw std::overflow_error("squared segment sum exceeds int64_t");
    return static_cast<std::int64_t>(difference * difference);
  }

  static void solve_partition_layer(const std::vector<std::int64_t>& prefix,
                                    const std::vector<std::int64_t>& previous,
                                    std::vector<std::int64_t>& current,
                                    std::size_t first_end, std::size_t last_end,
                                    std::size_t opt_left, std::size_t opt_right) {
    if (first_end > last_end) return;
    const std::size_t mid = first_end + (last_end - first_end) / 2;
    constexpr auto inf = std::numeric_limits<std::int64_t>::max();
    auto best = inf;
    std::size_t best_split = opt_left;
    const std::size_t hi = std::min(opt_right, mid - 1);
    for (std::size_t split = opt_left; split <= hi; ++split) {
      if (previous[split] == inf) continue;
      const auto candidate = add_cost(previous[split], square_difference(prefix[mid], prefix[split]));
      if (candidate < best) {
        best = candidate;
        best_split = split;
      }
    }
    current[mid] = best;
    if (mid > first_end)
      solve_partition_layer(prefix, previous, current, first_end, mid - 1, opt_left, best_split);
    if (mid < last_end)
      solve_partition_layer(prefix, previous, current, mid + 1, last_end, best_split, opt_right);
  }
};

}  // namespace algo
