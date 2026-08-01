#pragma once

#include "DemoConfig.hpp"
#include "ExplicitTimeIntegrator.hpp"
#include "PdTypes.hpp"

#include <cstddef>
#include <vector>

namespace vumat {
namespace pd {

struct RigidWallContactResult {
  std::vector<Vec3> forceDensity;
  Vec3 forceOnBody{0.0, 0.0, 0.0};
  Vec3 wallReaction{0.0, 0.0, 0.0};
  double normalReaction = 0.0;
  double potentialEnergy = 0.0;
  double frictionPower = 0.0;
  double maxPenetration = 0.0;
  std::size_t activeParticles = 0;
};

class RigidWallContact {
public:
  explicit RigidWallContact(const demo::RigidWallContactConfig& config);

  RigidWallContactResult evaluate(const ParticleBlock& block,
                                  const ExplicitDynamicState& state) const;
  bool enabled() const { return enabled_; }

private:
  bool enabled_ = false;
  Vec3 point_{0.0, 0.0, 0.0};
  Vec3 normal_{1.0, 0.0, 0.0};
  double penaltyStiffness_ = 0.0;
  double frictionCoefficient_ = 0.0;
};

} // namespace pd
} // namespace vumat
