#include "ExplicitTimeIntegrator.hpp"

#include <stdexcept>

namespace vumat {
namespace pd {

ExplicitTimeIntegrator::ExplicitTimeIntegrator(double density) : density_(density) {
  if (density_ <= 0.0)
    throw std::runtime_error("Explicit dynamics requires density > 0.");
}

void ExplicitTimeIntegrator::initialize(const ParticleBlock& block,
                                        ExplicitDynamicState& state) const {
  const size_t n = block.referencePosition.size();
  if (block.displacement.size() != n)
    throw std::runtime_error("Particle displacement size mismatch in explicit dynamics.");
  state.velocity.assign(n, {0.0, 0.0, 0.0});
  state.acceleration.assign(n, {0.0, 0.0, 0.0});
}

void ExplicitTimeIntegrator::predictDisplacement(
    ParticleBlock& block, const ExplicitDynamicState& state, double dt) const {
  if (dt <= 0.0 || state.velocity.size() != block.displacement.size() ||
      state.acceleration.size() != block.displacement.size())
    throw std::runtime_error("Invalid explicit dynamics predictor input.");
  for (size_t i = 0; i < block.displacement.size(); ++i)
    for (int d = 0; d < 3; ++d)
      block.displacement[i][static_cast<size_t>(d)] +=
          dt * state.velocity[i][static_cast<size_t>(d)] +
          0.5 * dt * dt * state.acceleration[i][static_cast<size_t>(d)];
}

void ExplicitTimeIntegrator::correctVelocity(
    ExplicitDynamicState& state, const std::vector<Vec3>& forceDensity, double dt) const {
  if (dt <= 0.0 || forceDensity.size() != state.velocity.size() ||
      state.acceleration.size() != state.velocity.size())
    throw std::runtime_error("Invalid explicit dynamics corrector input.");
  for (size_t i = 0; i < state.velocity.size(); ++i) {
    Vec3 nextAcceleration{};
    for (int d = 0; d < 3; ++d) {
      nextAcceleration[static_cast<size_t>(d)] =
          forceDensity[i][static_cast<size_t>(d)] / density_;
      state.velocity[i][static_cast<size_t>(d)] +=
          0.5 * dt * (state.acceleration[i][static_cast<size_t>(d)] +
                      nextAcceleration[static_cast<size_t>(d)]);
    }
    state.acceleration[i] = nextAcceleration;
  }
}

} // namespace pd
} // namespace vumat
