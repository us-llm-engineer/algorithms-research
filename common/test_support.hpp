// Helpers shared by the Catch2 suites.
#pragma once
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "rng.hpp"

namespace algo::testing {

// Base seed of every randomized test. Override with ALGO_TEST_SEED=<n> to explore other inputs;
// a failing run prints the exact seed so it can be replayed.
inline std::uint64_t base_seed() {
  if (const char* env = std::getenv("ALGO_TEST_SEED")) return std::strtoull(env, nullptr, 0);
  return 0x5EEDBA5EULL;
}

// Seed for the i-th trial of a randomized test.
inline std::uint64_t trial_seed(std::uint64_t trial, std::uint64_t salt = 0) {
  std::uint64_t s = base_seed() ^ (salt * 0x9E3779B97F4A7C15ULL) ^ (trial * 0xD1B54A32D192ED03ULL);
  return mix64(s);
}

}  // namespace algo::testing

// Records the seed in the failure output of the enclosing scope.
#define ALGO_TRIAL(trial, salt)                                         \
  const std::uint64_t seed_ = ::algo::testing::trial_seed((trial), (salt)); \
  INFO("trial=" << (trial) << " seed=" << seed_ << " (replay: ALGO_TEST_SEED=" << ::algo::testing::base_seed() << ")")
