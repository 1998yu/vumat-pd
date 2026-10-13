#pragma once

#include "PdTypes.hpp"

#include <vector>

namespace vumat {
namespace pd {

class PdCorrespondenceSkeleton {
public:
  explicit PdCorrespondenceSkeleton(double horizon);

  void initialize(const ParticleBlock& block, CorrespondenceState& state) const;
  std::vector<Tensor3> computeDeformationGradients(
      const ParticleBlock& block, const CorrespondenceState& state) const;
  std::vector<Vec3> computeForceDensity(
      const ParticleBlock& block,
      const CorrespondenceState& state,
      const std::vector<Tensor3>& deformationGradient,
      const std::vector<Tensor3>& cauchyStress) const;

private:
  double horizon_;
};

std::vector<std::vector<int>> buildNeighborList(
    const std::vector<Vec3>& referencePosition, double horizon);

std::vector<std::vector<int>> buildNeighborListBruteForce(
    const std::vector<Vec3>& referencePosition, double horizon);

} // namespace pd
} // namespace vumat
