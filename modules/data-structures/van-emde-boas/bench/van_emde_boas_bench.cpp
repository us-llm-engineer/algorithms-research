// Predecessor-search structures over a universe of 2^u integers holding 2^u / 16 random keys:
//   linear scan of a bitmap (brute force)   successor = next set bit, O(U / 64)
//   sorted array + binary search            O(log n) successor, O(n) insert
//   std::set                                red-black tree, O(log n)
//   van Emde Boas (dense) / sparse vEB      O(log log U)
// `n` is the universe size U = 2^u; 200000 queries per measurement. `extra` records bytes where available.
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "bench_harness.hpp"
#include "rng.hpp"
#include "van_emde_boas.hpp"

using algo::bench::measure;
constexpr std::size_t Q = 200000;

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/data-structures/van-emde-boas/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);
  for (unsigned u : {12u, 14u, 16u, 18u, 20u, 22u, 24u}) {
    const std::uint64_t U = std::uint64_t{1} << u;
    algo::Rng rng(u);
    std::vector<std::uint32_t> keys(U / 16), qs(Q);
    for (auto& k : keys) k = static_cast<std::uint32_t>(rng.below(U));
    for (auto& q : qs) q = static_cast<std::uint32_t>(rng.below(U));
    std::vector<std::uint32_t> sorted = keys;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    const int reps = 5;

    auto emit = [&](const std::string& name, auto&& build, auto&& succ, const std::string& extra) {
      csv.row(name, "build", U, measure({}, build, reps), extra);
      std::uint64_t acc = 0;
      csv.row(name, "successor", U, measure({}, [&] { for (auto q : qs) acc += succ(q); algo::bench::do_not_optimize(acc); }, reps), extra);
    };

    algo::VebTree dense(u);
    for (auto k : keys) dense.insert(k);
    emit("van Emde Boas (dense)", [&] { algo::VebTree t(u); for (auto k : keys) t.insert(k); algo::bench::do_not_optimize(t); },
         [&](auto q) { auto r = dense.successor(q); return r ? *r : 0; }, "bytes=" + std::to_string(dense.memory_bytes()));
    algo::SparseVebTree sparse(u);
    for (auto k : keys) sparse.insert(k);
    emit("van Emde Boas (sparse)", [&] { algo::SparseVebTree t(u); for (auto k : keys) t.insert(k); algo::bench::do_not_optimize(t); },
         [&](auto q) { auto r = sparse.successor(q); return r ? *r : 0; }, "bytes=" + std::to_string(sparse.memory_bytes()));
    std::set<std::uint32_t> rb(keys.begin(), keys.end());
    emit("std::set", [&] { std::set<std::uint32_t> t(keys.begin(), keys.end()); algo::bench::do_not_optimize(t); },
         [&](auto q) { auto it = rb.upper_bound(q); return it == rb.end() ? 0 : *it; }, "bytes=" + std::to_string(rb.size() * 40));
    emit("sorted array + binary search", [&] { auto t = keys; std::sort(t.begin(), t.end()); algo::bench::do_not_optimize(t); },
         [&](auto q) { auto it = std::upper_bound(sorted.begin(), sorted.end(), q); return it == sorted.end() ? 0 : *it; },
         "bytes=" + std::to_string(sorted.size() * 4));
    if (u <= 20) {
      std::vector<std::uint64_t> bitmap(U / 64, 0);
      for (auto k : keys) bitmap[k / 64] |= std::uint64_t{1} << (k % 64);
      emit("bitmap scan (brute force)", [&] { std::vector<std::uint64_t> b(U / 64, 0); for (auto k : keys) b[k / 64] |= std::uint64_t{1} << (k % 64); algo::bench::do_not_optimize(b); },
           [&](std::uint32_t q) -> std::uint64_t {
             std::uint64_t i = std::uint64_t{q} + 1;
             if (i >= U) return 0;
             std::uint64_t w = i / 64;
             std::uint64_t cur = bitmap[w] & (~0ULL << (i % 64));
             while (!cur && ++w < bitmap.size()) cur = bitmap[w];
             return cur ? w * 64 + std::countr_zero(cur) : 0;
           }, "bytes=" + std::to_string(U / 8));
    }
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
