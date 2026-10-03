// Fibonacci heap vs std::priority_queue (binary heap).
//   push-pop:     n random pushes then n pops.
//   decrease-key: n pushes, n random decrease-keys, n pops. The binary-heap baseline uses the
//                 standard lazy-deletion idiom (push a duplicate, skip stale entries on pop).
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <queue>
#include <vector>

#include "bench_harness.hpp"
#include "fibonacci_heap.hpp"
#include "rng.hpp"

using algo::bench::measure;
using algo::bench::do_not_optimize;

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/data-structures/fibonacci-heap/results/bench.csv";
  std::filesystem::create_directories(out.parent_path());
  std::ofstream file(out);
  if (!file) return 2;
  algo::bench::Csv csv(file);

  for (std::size_t n : {1u << 12, 1u << 14, 1u << 16, 1u << 18, 1u << 20}) {
    algo::Rng rng(n);
    std::vector<std::int64_t> keys(n), lowered(n);
    std::vector<std::size_t> pick(n);
    for (std::size_t i = 0; i < n; ++i) {
      keys[i] = 1'000'000 + rng.range(0, 1'000'000'000);
      pick[i] = rng.below(n);
      lowered[i] = rng.range(0, 999'999);
    }
    const int reps = n >= (1u << 18) ? 5 : 9;

    csv.row("fibonacci-heap", "push-pop", n, measure({}, [&] {
              algo::FibonacciHeap<std::int64_t> h;
              h.reserve(n);
              for (auto k : keys) h.push(k);
              std::int64_t s = 0;
              while (!h.empty()) s += h.pop();
              do_not_optimize(s);
            }, reps));
    csv.row("std::priority_queue", "push-pop", n, measure({}, [&] {
              std::priority_queue<std::int64_t, std::vector<std::int64_t>, std::greater<>> h;
              for (auto k : keys) h.push(k);
              std::int64_t s = 0;
              while (!h.empty()) { s += h.top(); h.pop(); }
              do_not_optimize(s);
            }, reps));

    csv.row("fibonacci-heap", "decrease-key", n, measure({}, [&] {
              algo::FibonacciHeap<std::int64_t> h;
              h.reserve(n);
              std::vector<algo::FibonacciHeap<std::int64_t>::Handle> hs(n);
              for (std::size_t i = 0; i < n; ++i) hs[i] = h.push(keys[i]);
              for (std::size_t i = 0; i < n; ++i)
                if (lowered[i] < h.key(hs[pick[i]])) h.decrease_key(hs[pick[i]], lowered[i]);
              std::int64_t s = 0;
              while (!h.empty()) s += h.pop();
              do_not_optimize(s);
            }, reps));
    csv.row("std::priority_queue (lazy)", "decrease-key", n, measure({}, [&] {
              using P = std::pair<std::int64_t, std::size_t>;
              std::priority_queue<P, std::vector<P>, std::greater<>> h;
              std::vector<std::int64_t> cur = keys;
              for (std::size_t i = 0; i < n; ++i) h.push({keys[i], i});
              for (std::size_t i = 0; i < n; ++i)
                if (lowered[i] < cur[pick[i]]) { cur[pick[i]] = lowered[i]; h.push({lowered[i], pick[i]}); }
              std::int64_t s = 0;
              while (!h.empty()) {
                auto [k, id] = h.top(); h.pop();
                if (k == cur[id]) { s += k; cur[id] = -1; }
              }
              do_not_optimize(s);
            }, reps));
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n';
  return 0;
}
