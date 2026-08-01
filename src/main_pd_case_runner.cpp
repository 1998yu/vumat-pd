#include "DemoCaseRunner.hpp"

#include <string>

namespace {
std::string executableDirectory(const char* argv0) {
  const std::string path = argv0 ? argv0 : "";
  const size_t separator = path.find_last_of("\\/");
  return separator == std::string::npos ? "." : path.substr(0, separator);
}
}

int main(int argc, char** argv) {
  const std::string casePath =
      argc > 1 ? argv[1] : "examples/affine_bar_elastic.json";
  return vumat::demo::runDemoCaseFromJson(casePath, executableDirectory(argv[0]));
}
