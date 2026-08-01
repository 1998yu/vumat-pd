#include "RigidWallContact.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vumat {
namespace pd {
namespace {

double dot(const Vec3& a, const Vec3& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double norm(const Vec3& value) { return std::sqrt(dot(value, value)); }

} // namespace

RigidWallContact::RigidWallContact(const demo::RigidWallContactConfig& config)
    : enabled_(config.enabled), point_(config.point), normal_(config.normal),
      penaltyStiffness_(config.penaltyStiffness),
      frictionCoefficient_(config.frictionCoefficient) {
  const double magnitude = norm(normal_);
  if (enabled_ && (magnitude <= 0.0 || penaltyStiffness_ <= 0.0 ||
                   frictionCoefficient_ < 0.0))
    throw std::runtime_error("Invalid rigid-wall contact configuration.");
  if (magnitude > 0.0)
    for (double& value : normal_) value /= magnitude;
}

RigidWallContactResult RigidWallContact::evaluate(
    const ParticleBlock& block, const ExplicitDynamicState& state) const {
  if (block.referencePosition.size() != block.displacement.size() ||
      block.referencePosition.size() != block.volume.size() ||
      block.referencePosition.size() != state.velocity.size())
    throw std::runtime_error("Rigid-wall contact input sizes do not match.");

  RigidWallContactResult result;
  result.forceDensity.assign(block.referencePosition.size(), {0.0, 0.0, 0.0});
  if (!enabled_) return result;

  for (std::size_t i = 0; i < block.referencePosition.size(); ++i) {
    Vec3 current{};
    for (int d = 0; d < 3; ++d)
      current[static_cast<std::size_t>(d)] =
          block.referencePosition[i][static_cast<std::size_t>(d)] +
          block.displacement[i][static_cast<std::size_t>(d)] -
          point_[static_cast<std::size_t>(d)];
    const double gap = dot(current, normal_);
    if (gap >= 0.0) continue;

    const double penetration = -gap;
    const double normalDensity = penaltyStiffness_ * penetration;
    Vec3 force{};
    for (int d = 0; d < 3; ++d)
      force[static_cast<std::size_t>(d)] =
          normalDensity * normal_[static_cast<std::size_t>(d)];

    if (frictionCoefficient_ > 0.0) {
      const double normalVelocity = dot(state.velocity[i], normal_);
      Vec3 tangentialVelocity{};
      for (int d = 0; d < 3; ++d)
        tangentialVelocity[static_cast<std::size_t>(d)] =
            state.velocity[i][static_cast<std::size_t>(d)] -
            normalVelocity * normal_[static_cast<std::size_t>(d)];
      const double tangentialSpeed = norm(tangentialVelocity);
      if (tangentialSpeed > 1.0e-14)
        for (int d = 0; d < 3; ++d)
          force[static_cast<std::size_t>(d)] -=
              frictionCoefficient_ * normalDensity *
              tangentialVelocity[static_cast<std::size_t>(d)] / tangentialSpeed;
    }

    result.forceDensity[i] = force;
    for (int d = 0; d < 3; ++d) {
      const double particleForce = force[static_cast<std::size_t>(d)] * block.volume[i];
      result.forceOnBody[static_cast<std::size_t>(d)] += particleForce;
      result.wallReaction[static_cast<std::size_t>(d)] -= particleForce;
    }
    result.normalReaction += normalDensity * block.volume[i];
    result.potentialEnergy +=
        0.5 * penaltyStiffness_ * penetration * penetration * block.volume[i];
    result.frictionPower += dot(force, state.velocity[i]) * block.volume[i] -
        normalDensity * dot(normal_, state.velocity[i]) * block.volume[i];
    result.maxPenetration = std::max(result.maxPenetration, penetration);
    ++result.activeParticles;
  }
  return result;
}

} // namespace pd
} // namespace vumat
