#include <cmath>
#include <cstdint>
#include <stdexcept>

#include <catch2/catch_approx.hpp>

#include "hyperloglog.hpp"
#include "test_support.hpp"

TEST_CASE("S1.1 empty estimate is zero", "[S1][given]") { algo::HyperLogLog h(10); REQUIRE(h.estimate() == 0.0); REQUIRE(h.empty()); }
TEST_CASE("S1.2 duplicate inserts do not increase estimate materially", "[S1][correctness]") { algo::HyperLogLog h(12); for(int i=0;i<10000;++i)h.add(42); auto one=h.estimate(); for(int i=0;i<10000;++i)h.add(42); REQUIRE(h.estimate()==Catch::Approx(one)); REQUIRE(one < 2.0); }
TEST_CASE("S1.3 small exact cardinality is close", "[S1][correctness]") { algo::HyperLogLog h(12); for(std::uint64_t i=0;i<1000;++i)h.add(i); REQUIRE(h.estimate()==Catch::Approx(1000.0).margin(35.0)); }
TEST_CASE("S1.4 medium cardinality relative error is bounded", "[S1][quality]") { algo::HyperLogLog h(14); for(std::uint64_t i=0;i<100000;++i)h.add(i); REQUIRE(std::abs(h.estimate()-100000.0)/100000.0 < 0.03); }
TEST_CASE("S1.5 precision bounds are validated", "[S1][invalid]") { REQUIRE_THROWS_AS(algo::HyperLogLog(3), std::invalid_argument); REQUIRE_THROWS_AS(algo::HyperLogLog(20), std::invalid_argument); }
TEST_CASE("S1.6 clear returns to empty", "[S1][recovery]") { algo::HyperLogLog h(10); for(int i=0;i<1000;++i)h.add(i); REQUIRE(h.estimate()>500); h.clear(); REQUIRE(h.empty()); REQUIRE(h.estimate()==0.0); }
TEST_CASE("S1.7 deterministic hashing gives repeatable estimate", "[S1][determinism]") { algo::HyperLogLog a(12), b(12); for(int i=0;i<50000;++i){a.add(i);b.add(i);} REQUIRE(a.estimate()==Catch::Approx(b.estimate())); }
TEST_CASE("S1.8 high-bit and zero values are distinct", "[S1][boundary]") { algo::HyperLogLog h(10); h.add(0); h.add(UINT64_MAX); h.add(1ULL<<63); REQUIRE(h.estimate()>2.0); }

TEST_CASE("S2.1 merge equals union estimate", "[S2][merge]") { algo::HyperLogLog a(12), b(12), all(12); for(std::uint64_t i=0;i<50000;++i){if(i&1)a.add(i);else b.add(i);all.add(i);} a.merge(b); REQUIRE(a.estimate()==Catch::Approx(all.estimate())); }
TEST_CASE("S2.2 merge rejects different precision", "[S2][invalid]") { algo::HyperLogLog a(10), b(11); REQUIRE_THROWS_AS(a.merge(b), std::invalid_argument); }
TEST_CASE("S2.3 merge is idempotent", "[S2][merge]") { algo::HyperLogLog a(12), b(12); for(int i=0;i<10000;++i){a.add(i);b.add(i);} a.merge(b); auto first=a.estimate(); a.merge(b); REQUIRE(a.estimate()==Catch::Approx(first)); }
TEST_CASE("S2.4 relative error stays bounded across disjoint trials", "[S2][quality]") { for(int trial=0;trial<8;++trial){algo::HyperLogLog h(12); for(std::uint64_t i=0;i<20000;++i)h.add(i+static_cast<std::uint64_t>(trial)*1000000); REQUIRE(std::abs(h.estimate()-20000.0)/20000.0 < 0.08);} }
TEST_CASE("S2.5 precision improves expected error", "[S2][quality]") { algo::HyperLogLog low(8), high(14); for(std::uint64_t i=0;i<50000;++i){low.add(i);high.add(i);} REQUIRE(std::abs(high.estimate()-50000.0) < std::abs(low.estimate()-50000.0) + 5000.0); }
TEST_CASE("S2.6 repeated merge tree preserves total union", "[S2][merge]") { algo::HyperLogLog root(12); for(int shard=0;shard<8;++shard){algo::HyperLogLog part(12); for(int i=0;i<10000;++i)part.add(static_cast<std::uint64_t>(shard)*10000+i); root.merge(part);} REQUIRE(std::abs(root.estimate()-80000.0)/80000.0 < 0.06); }
TEST_CASE("S2.7 sparse and dense phases can be reset", "[S2][recovery]") { algo::HyperLogLog h(10); for(int i=0;i<100;++i)h.add(i); h.clear(); for(int i=0;i<10000;++i)h.add(i); REQUIRE(std::abs(h.estimate()-10000.0)/10000.0 < 0.12); }
TEST_CASE("S2.8 estimate remains finite at large cardinality", "[S2][scale]") { algo::HyperLogLog h(14); for(std::uint64_t i=0;i<1000000;++i)h.add(i); REQUIRE(std::isfinite(h.estimate())); REQUIRE(h.estimate()>700000.0); }
