#pragma once

#include "PdTypes.hpp"

#include <vector>

namespace vumat {
namespace pd {

struct ExplicitDynamicState {
  std::vector<Vec3> velocity;
  std::vector<Vec3> acceleration;
};

class ExplicitTimeIntegrator {
public:
  explicit ExplicitTimeIntegrator(double density);

  void initialize(const ParticleBlock& block, ExplicitDynamicState& state) const;
  void predictDisplacement(ParticleBlock& block,
                           const ExplicitDynamicState& state,
                           double dt) const;
  void correctVelocity(ExplicitDynamicState& state,
                       const std::vector<Vec3>& forceDensity,
                       double dt) const;

  double density() const { return density_; }

private:
  double density_;
};

} // namespace pd
} // namespace vumat
