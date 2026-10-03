#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "count_min.hpp"
#include "test_support.hpp"

using Sketch = algo::CountMinSketch<std::uint64_t>;

TEST_CASE("S1.1 empty sketch estimates zero", "[S1][given]") { Sketch s(128, 5, 1); REQUIRE(s.estimate(7) == 0); REQUIRE(s.total_count() == 0); }
TEST_CASE("S1.2 point updates are never underestimates", "[S1][correctness]") { Sketch s(256, 5, 2); for (int i=0;i<100;++i) s.add(1); for (int i=0;i<37;++i) s.add(2); REQUIRE(s.estimate(1)>=100); REQUIRE(s.estimate(2)>=37); REQUIRE(s.total_count()==137); }
TEST_CASE("S1.3 unseen keys remain zero with sparse counters", "[S1][boundary]") { Sketch s(4096, 6, 3); for (std::uint64_t k=0;k<100;++k) s.add(k); REQUIRE(s.estimate(1000000)==0); }
TEST_CASE("S1.4 weighted updates accumulate exactly without collisions", "[S1][correctness]") { Sketch s(1024, 7, 4); s.add(11, 100); s.add(11, 23); s.add(12, 4); REQUIRE(s.estimate(11)>=123); REQUIRE(s.estimate(12)>=4); REQUIRE(s.total_count()==127); }
TEST_CASE("S1.5 randomized estimates obey frequency bounds", "[S1][stress]") { Sketch s(2048, 7, 5); std::vector<std::uint64_t> exact(500); algo::Rng r(5); for(int i=0;i<20000;++i){auto k=r.below(500); ++exact[k]; s.add(k);} for(int k=0;k<500;++k) REQUIRE(s.estimate(k)>=exact[k]); }
TEST_CASE("S1.6 constructor rejects invalid dimensions", "[S1][invalid]") { REQUIRE_THROWS_AS(Sketch(0, 3, 1), std::invalid_argument); REQUIRE_THROWS_AS(Sketch(3, 0, 1), std::invalid_argument); }
TEST_CASE("S1.7 clear resets counters and total", "[S1][recovery]") { Sketch s(64,4,7); s.add(1,9); s.add(2,3); s.clear(); REQUIRE(s.total_count()==0); REQUIRE(s.estimate(1)==0); REQUIRE(s.estimate(2)==0); }
TEST_CASE("S1.8 string-like workload through hashed integer ids", "[S1][scale]") { Sketch s(8192,5,8); for(std::uint64_t i=0;i<100000;++i) s.add(i%1000); REQUIRE(s.total_count()==100000); REQUIRE(s.estimate(17)>=100); }

TEST_CASE("S2.1 merge equals pointwise addition", "[S2][merge]") { Sketch a(512,5,9), b(512,5,9), all(512,5,9); for(int i=0;i<10000;++i){auto k=static_cast<std::uint64_t>(i%97); if(i&1)a.add(k); else b.add(k); all.add(k);} a.merge(b); for(int k=0;k<97;++k) REQUIRE(a.estimate(k)==all.estimate(k)); REQUIRE(a.total_count()==all.total_count()); }
TEST_CASE("S2.2 merge rejects incompatible dimensions", "[S2][invalid]") { Sketch a(32,4,1), b(64,4,1), c(32,5,1); REQUIRE_THROWS_AS(a.merge(b), std::invalid_argument); REQUIRE_THROWS_AS(a.merge(c), std::invalid_argument); }
TEST_CASE("S2.3 merge rejects incompatible seeds", "[S2][invalid]") { Sketch a(32,4,1), b(32,4,2); REQUIRE_THROWS_AS(a.merge(b), std::invalid_argument); }
TEST_CASE("S2.4 heavy hitter estimate remains within its frequency scale", "[S2][quality]") { Sketch s(4096,7,10); for(int i=0;i<100000;++i)s.add(1); for(int i=0;i<100;++i)s.add(2); REQUIRE(s.estimate(1)>=100000); REQUIRE(s.estimate(1)-100000 <= 1000); }
TEST_CASE("S2.5 deterministic seed gives identical counters", "[S2][determinism]") { Sketch a(128,5,11), b(128,5,11); for(int i=0;i<1000;++i){a.add(i%31);b.add(i%31);} for(int i=0;i<31;++i) REQUIRE(a.estimate(i)==b.estimate(i)); }
TEST_CASE("S2.6 large weighted counts do not overflow", "[S2][scale]") { Sketch s(128,5,12); s.add(1, 4000000000ULL); s.add(1, 4000000000ULL); REQUIRE(s.estimate(1)>=8000000000ULL); REQUIRE(s.total_count()==8000000000ULL); }
TEST_CASE("S2.7 collision error is bounded by total mass over width", "[S2][invariant]") { Sketch s(256,5,13); for(int i=0;i<10000;++i)s.add(static_cast<std::uint64_t>(i)); for(int k=0;k<10000;++k) REQUIRE(s.estimate(k)-1 <= s.total_count()/256 + 100); }
TEST_CASE("S2.8 repeated merge and clear cycles recover exact totals", "[S2][recovery]") { Sketch a(128,5,14), b(128,5,14); for(int r=0;r<20;++r){a.clear();b.clear();a.add(r, static_cast<std::uint64_t>(r+1));b.add(r+1, static_cast<std::uint64_t>(2*r)); a.merge(b); REQUIRE(a.total_count()==static_cast<std::uint64_t>(3*r+1));} }
