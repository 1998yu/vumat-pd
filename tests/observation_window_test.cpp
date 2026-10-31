#include "Geometry.hpp"
#include "ObservationWindow.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
  const double automatic = std::numeric_limits<double>::quiet_NaN();
  for (int axis : {0, 1}) {
    auto block = vumat::demo::makeParticlePatch(9, 7, 5, 1.0, 3.0);
    // Verify coordinate-origin independence, including the y-loading path.
    for (auto &x : block.referencePosition)
      x[axis] += 137.0;
    auto [a, b] = vumat::demo::observationBounds(block, axis, 1.0, automatic, automatic);
    const auto weights = vumat::demo::observationWeights(block, axis, a, b, 1e-8);
    double volume = 0.0, firstMoment = 0.0;
    for (size_t i = 0; i < block.volume.size(); ++i) {
      double w = weights[i];
      volume += w;
      firstMoment += w * block.referencePosition[i][axis];
    }
    const double expectedVolume = (axis == 0 ? 6.0 * 6.0 * 4.0 : 8.0 * 4.0 * 4.0);
    if (std::abs(volume - expectedVolume) > 1e-10 ||
        std::abs(firstMoment / volume - (a + b) / 2) > 1e-10)
      return 1;
    bool rejected = false;
    try {
      vumat::demo::observationBounds(block, axis, 1.0, b, a);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    if (!rejected)
      return 2;
    auto [lo, hi] = vumat::demo::observationBounds(block, axis, 0.0, automatic, automatic);
    const auto full = vumat::demo::observationWeights(block, axis, lo, hi, 1e-8);
    double fullVolume = 0.0;
    for (double w : full)
      fullVolume += w;
    if (std::abs(fullVolume - 192.0) > 1e-10)
      return 3;
    rejected = false;
    try {
      vumat::demo::observationWeights(block, axis, a + 0.1, b, 1e-8);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    if (!rejected)
      return 4;
  }
  std::cout << "PASS shifted x/y windows, exact volume and affine mean; reversed bounds rejected\n";
}
