#include "ParticleSets.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace vumat {
namespace demo {

std::vector<size_t> selectBoundaryRegion(const vumat::pd::ParticleBlock& block,
                                         const std::string& side,
                                         double width) {
  if (width <= 0.0 || block.referencePosition.empty())
    throw std::runtime_error("Boundary-region width and particle set must be nonzero.");
  int component = 0;
  bool selectMinimum = true;
  if (side == "left") { component = 0; selectMinimum = true; }
  else if (side == "right") { component = 0; selectMinimum = false; }
  else if (side == "bottom") { component = 1; selectMinimum = true; }
  else if (side == "top") { component = 1; selectMinimum = false; }
  else throw std::runtime_error("Unsupported boundary region: " + side);

  double extreme = selectMinimum ? std::numeric_limits<double>::infinity()
                                 : -std::numeric_limits<double>::infinity();
  for (const auto& x : block.referencePosition)
    extreme = selectMinimum ? std::min(extreme, x[static_cast<size_t>(component)])
                            : std::max(extreme, x[static_cast<size_t>(component)]);
  std::vector<size_t> selected;
  const double tolerance = 1.0e-10 * std::max(1.0, std::abs(extreme));
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const double distance = selectMinimum
        ? block.referencePosition[i][static_cast<size_t>(component)] - extreme
        : extreme - block.referencePosition[i][static_cast<size_t>(component)];
    if (distance <= width + tolerance)
      selected.push_back(i);
  }
  if (selected.empty())
    throw std::runtime_error("Boundary-region selection returned no particles.");
  return selected;
}

size_t selectClosestParticle(const vumat::pd::ParticleBlock& block,
                             const std::vector<size_t>& candidates,
                             const vumat::pd::Vec3& target) {
  if (candidates.empty())
    throw std::runtime_error("Closest-particle selection requires candidates.");
  size_t best = candidates.front();
  double bestDistance = std::numeric_limits<double>::infinity();
  for (size_t index : candidates) {
    double distance = 0.0;
    for (int d = 0; d < 3; ++d) {
      const double delta = block.referencePosition[index][static_cast<size_t>(d)] -
                           target[static_cast<size_t>(d)];
      distance += delta * delta;
    }
    if (distance < bestDistance) {
      bestDistance = distance;
      best = index;
    }
  }
  return best;
}

} // namespace demo
} // namespace vumat
