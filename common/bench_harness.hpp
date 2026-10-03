// Minimal, dependency-free benchmark harness: warm-up, repeated timing with
// std::chrono::steady_clock, median/min/max reporting and CSV output.
#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace algo::bench {

// Prevents the optimizer from discarding a computed value or reordering around it.
template <class T>
inline void do_not_optimize(const T& value) {
  asm volatile("" : : "g"(&value) : "memory");
}
inline void clobber() { asm volatile("" : : : "memory"); }

struct Stats {
  double median_ms = 0, min_ms = 0, max_ms = 0;
  int reps = 0;
};

// Times `body()` `reps` times after `warmup` untimed runs. `setup()` runs before every
// timed repetition and is not timed (use it to rebuild the input).
inline Stats measure(const std::function<void()>& setup, const std::function<void()>& body, int reps = 7,
                     int warmup = 1) {
  using clock = std::chrono::steady_clock;
  std::vector<double> ms;
  for (int i = 0; i < warmup + reps; ++i) {
    if (setup) setup();
    clobber();
    const auto t0 = clock::now();
    body();
    clobber();
    const auto t1 = clock::now();
    if (i >= warmup) ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
  }
  std::sort(ms.begin(), ms.end());
  Stats s;
  s.reps = reps;
  s.min_ms = ms.front();
  s.max_ms = ms.back();
  s.median_ms = ms[ms.size() / 2];
  return s;
}

// Peak resident set size of this process in KiB (VmHWM), or 0 when /proc is unavailable.
inline long peak_rss_kb() {
  std::ifstream f("/proc/self/status");
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("VmHWM:", 0) == 0) return std::stol(line.substr(6));
  }
  return 0;
}

// Appends rows to a CSV stream: algorithm,workload,n,median_ms,min_ms,max_ms,reps,extra
class Csv {
 public:
  explicit Csv(std::ostream& out) : out_(out) {
    out_ << "algorithm,workload,n,median_ms,min_ms,max_ms,reps,extra\n";
  }
  void row(const std::string& algorithm, const std::string& workload, std::uint64_t n, const Stats& s,
           const std::string& extra = "") {
    char buf[256];
    std::snprintf(buf, sizeof buf, "%.6f,%.6f,%.6f,%d", s.median_ms, s.min_ms, s.max_ms, s.reps);
    out_ << algorithm << ',' << workload << ',' << n << ',' << buf << ',' << extra << '\n';
    out_.flush();
  }

 private:
  std::ostream& out_;
};

}  // namespace algo::bench
