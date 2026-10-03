#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "christofides.hpp"
#include "bench_harness.hpp"

int main(int argc, char** argv) {
  const std::filesystem::path out = argc > 1 ? argv[1] : "modules/approximation/christofides/results/bench.csv";
  std::filesystem::create_directories(out.parent_path()); std::ofstream file(out); if (!file) return 2; algo::bench::Csv csv(file);
  for (std::size_t n : {32U, 64U, 128U, 256U}) {
    std::vector<std::pair<double,double>> p(n); for(std::size_t i=0;i<n;++i){const double a=2*3.141592653589793*i/n;p[i]={std::cos(a),std::sin(a)};}
    std::vector<std::vector<double>> d(n,std::vector<double>(n)); for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j)d[i][j]=std::hypot(p[i].first-p[j].first,p[i].second-p[j].second);
    algo::ChristofidesTour result; const auto stats=algo::bench::measure({},[&]{result=algo::christofides(d);algo::bench::do_not_optimize(result.cost);},5,1); csv.row("christofides","euclidean-circle",n,stats,"cost="+std::to_string(result.cost));
  }
  std::cerr << "peak_rss_kb=" << algo::bench::peak_rss_kb() << '\n'; return 0;
}
