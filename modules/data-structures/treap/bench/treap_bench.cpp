// Treap vs sorted array (brute force), std::set (red-black tree) and std::unordered_set (hash table)
// on identical key streams.
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <unordered_set>

#include "treap.hpp"
#include "ordered_set_bench.hpp"

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/data-structures/treap/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  using namespace algo::bench;
  for (std::size_t n : {1u << 10, 1u << 12, 1u << 14, 1u << 16, 1u << 18}) {
    const auto w = make_ordered_workloads(n);
    const int reps = n >= (1u << 16) ? 5 : 9;
    run_ordered(csv, "treap", n, w, [] { return algo::Treap<std::uint64_t>(42); }, reps);
    run_ordered(csv, "std::set", n, w, [] { return StdSetAdapter<std::set<std::uint64_t>>{}; }, reps);
    run_ordered(csv, "std::unordered_set", n, w, [] { return StdSetAdapter<std::unordered_set<std::uint64_t>>{}; }, reps);
    if (n <= (1u << 16)) run_ordered(csv, "sorted array (brute force)", n, w, [] { return SortedVectorSet{}; }, reps);
  }
  std::cerr << "peak_rss_kb=" << peak_rss_kb() << '\n';
  return 0;
}
