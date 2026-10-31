// Module: apps. See docs/architecture.md for ownership and call flow.
#include "DemoCaseRunner.hpp"

#include <string>
#include <iostream>

namespace {
std::string executableDirectory(const char *argv0) {
  const std::string path = argv0 ? argv0 : "";
  const size_t separator = path.find_last_of("\\/");
  return separator == std::string::npos ? "." : path.substr(0, separator);
}
} // namespace

int main(int argc, char **argv) {
  if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
    std::cout << "Usage: vumat_pd_run [case.json]\n"
              << "Run from the checkout root; default: examples/affine_bar_elastic.json\n"
              << "Regional PD units: mm, ms, N, MPa. dt is user supplied.\n"
              << "No zero-energy stabilization or automatic stable-step estimator.\n"
              << "See docs/parameters.md and docs/time-step.md.\n";
    return 0;
  }
  const std::string casePath = argc > 1 ? argv[1] : "examples/affine_bar_elastic.json";
  return vumat::demo::runDemoCaseFromJson(casePath, executableDirectory(argv[0]));
}
