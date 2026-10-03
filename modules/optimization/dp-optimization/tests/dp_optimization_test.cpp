#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "dp_optimization.hpp"
using algo::DpOptimization;
TEST_CASE("S1.1 divide-and-conquer partition DP matches worked example", "[S1]"){REQUIRE(DpOptimization::partition_squared({1,2,3,4},2)==52);}
TEST_CASE("S1.2 one segment equals direct squared sum", "[S1]"){REQUIRE(DpOptimization::partition_squared({2,3,5},1)==100);}
TEST_CASE("S1.3 one item per segment has the sum of singleton squares", "[S1]"){REQUIRE(DpOptimization::partition_squared({4,1,9},3)==98);}
TEST_CASE("S1.4 invalid segment counts reject", "[S1]"){REQUIRE_THROWS_AS(DpOptimization::partition_squared({},1),std::invalid_argument);REQUIRE_THROWS_AS(DpOptimization::partition_squared({1},0),std::invalid_argument);}
TEST_CASE("S1.5 monotone weights match quadratic reference", "[S1]"){REQUIRE(DpOptimization::partition_squared({1,1,1,1,1},2)==13);}
TEST_CASE("S1.6 negative values retain exact arithmetic", "[S1]"){REQUIRE(DpOptimization::partition_squared({-2,3,-1},2)==2);}
TEST_CASE("S1.7 Knuth interval merge matches known optimal cost", "[S1]"){REQUIRE(DpOptimization::optimal_merge({1,2,3,4})==19);}
TEST_CASE("S1.8 singleton merge cost is zero", "[S1]"){REQUIRE(DpOptimization::optimal_merge({7})==0);}
TEST_CASE("S2.1 partition result is nonincreasing as segments grow", "[S2]"){auto a=DpOptimization::partition_squared({1,2,3,4,5},1);auto b=DpOptimization::partition_squared({1,2,3,4,5},2);REQUIRE(b<=a);}
TEST_CASE("S2.2 interval merge is invariant under equal-weight permutation", "[S2]"){REQUIRE(DpOptimization::optimal_merge({2,2,2,2})==16);}
TEST_CASE("S2.3 optimized partition equals quadratic oracle", "[S2]"){for(int n=1;n<30;++n){std::vector<std::int64_t>a(n,1);REQUIRE(DpOptimization::partition_squared(a,(n+1)/2)==DpOptimization::partition_squared_naive(a,(n+1)/2));}}
TEST_CASE("S2.4 optimized merge equals cubic oracle", "[S2]"){for(int n=1;n<12;++n){std::vector<std::int64_t>a(n,1);REQUIRE(DpOptimization::optimal_merge(a)==DpOptimization::optimal_merge_naive(a));}}
TEST_CASE("S2.5 large inputs remain 64-bit exact", "[S2]"){std::vector<std::int64_t>a(1000,1'000'000);REQUIRE(DpOptimization::partition_squared(a,100)>0);}
TEST_CASE("S2.6 optimization decisions preserve monotonicity certificates", "[S2]"){std::vector<std::int64_t>a(100,1);REQUIRE(DpOptimization::partition_decisions_monotone(a,10));}
TEST_CASE("S2.7 empty merge input is zero", "[S2]"){REQUIRE(DpOptimization::optimal_merge({})==0);}
TEST_CASE("S2.8 optimized algorithms handle repeated calls deterministically", "[S2]"){std::vector<std::int64_t>a{1,3,2,4};REQUIRE(DpOptimization::optimal_merge(a)==DpOptimization::optimal_merge(a));}
