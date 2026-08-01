#include "Geometry.hpp"
#include "PdCorrespondenceSkeleton.hpp"

#include <chrono>
#include <iostream>

int main() {
  constexpr double spacing=1.25;
  constexpr double horizon=3.0*spacing;
  auto block=vumat::demo::makeParticlePatch(17,9,3,spacing,horizon);
  const auto start=std::chrono::steady_clock::now();
  const auto reference=vumat::pd::buildNeighborListBruteForce(
      block.referencePosition,horizon);
  const auto middle=std::chrono::steady_clock::now();
  const auto linked=vumat::pd::buildNeighborList(block.referencePosition,horizon);
  const auto finish=std::chrono::steady_clock::now();
  if(reference!=linked){
    std::cerr << "Cell-linked-list neighbors differ from brute-force neighbors.\n";
    return 2;
  }
  const auto bruteMs=std::chrono::duration<double,std::milli>(middle-start).count();
  const auto linkedMs=std::chrono::duration<double,std::milli>(finish-middle).count();
  std::cout << "Neighbor lists identical for " << block.referencePosition.size()
            << " particles; brute_ms=" << bruteMs << " linked_ms=" << linkedMs << '\n';
  return 0;
}
