#include "BoundaryConditions.hpp"
#include "ParticleSets.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace vumat {
namespace demo {
namespace {

class AffineDisplacementBoundaryCondition : public BoundaryCondition {
public:
  explicit AffineDisplacementBoundaryCondition(BoundaryConditionConfig config)
      : config_(std::move(config)) {
    if (config_.type != "affine_displacement")
      throw std::runtime_error("Unsupported boundary condition type: " + config_.type);
    if (config_.maxStrain == 0.0)
      throw std::runtime_error("Affine displacement boundary condition requires maxStrain.");
  }

  void apply(vumat::pd::ParticleBlock& block, int step, int steps) const override {
    const double load = loadValue(step, steps);
    if (config_.mode == "simple_shear_xy") {
      for (size_t i = 0; i < block.referencePosition.size(); ++i)
        block.displacement[i] = {load * block.referencePosition[i][1], 0.0, 0.0};
      return;
    }

    for (size_t i = 0; i < block.referencePosition.size(); ++i)
      block.displacement[i] = {load * block.referencePosition[i][0], 0.0, 0.0};
  }

  double loadValue(int step, int steps) const override {
    if (steps <= 0)
      throw std::runtime_error("Boundary condition requires steps > 0.");
    return config_.maxStrain * static_cast<double>(step) / static_cast<double>(steps);
  }

private:
  BoundaryConditionConfig config_;
};

} // namespace

std::unique_ptr<BoundaryCondition> createBoundaryCondition(const BoundaryConditionConfig& config) {
  if (config.type == "affine_displacement")
    return std::make_unique<AffineDisplacementBoundaryCondition>(config);
  throw std::runtime_error("Unsupported boundary condition type: " + config.type);
}

RegionalConstraintSystem::RegionalConstraintSystem(
    const vumat::pd::ParticleBlock& block,
    const BoundaryConditionConfig& config,
    double spacing)
    : config_(config), loadComponent_(config.component == "y" ? 1 : 0) {
  const double width = static_cast<double>(config.regionLayers - 1) * spacing +
                       1.0e-9 * spacing;
  support_ = selectBoundaryRegion(block, config.supportRegion, width);
  loaded_ = selectBoundaryRegion(block, config.loadRegion, width);
  vumat::pd::Vec3 center{0.0, 0.0, 0.0};
  for (const auto& x : block.referencePosition)
    for (int d = 0; d < 3; ++d)
      center[static_cast<size_t>(d)] += x[static_cast<size_t>(d)];
  for (double& value : center)
    value /= static_cast<double>(block.referencePosition.size());
  anchor_ = selectClosestParticle(block, support_, center);
}

RampKinematics RegionalConstraintSystem::ramp(double time) const {
  if (time >= config_.riseTime)
    return {config_.amplitude, 0.0, 0.0};
  const double pi = 3.14159265358979323846;
  const double phase = pi * time / config_.riseTime;
  return {0.5 * config_.amplitude * (1.0 - std::cos(phase)),
          0.5 * config_.amplitude * pi / config_.riseTime * std::sin(phase),
          0.5 * config_.amplitude * pi * pi /
              (config_.riseTime * config_.riseTime) * std::cos(phase)};
}

void RegionalConstraintSystem::apply(vumat::pd::ParticleBlock& block,
                                     vumat::pd::ExplicitDynamicState& state,
                                     double time) const {
  const RampKinematics prescribed = ramp(time);
  const auto setComponent = [&](size_t i, int d, double u, double v, double a) {
    block.displacement[i][static_cast<size_t>(d)] = u;
    state.velocity[i][static_cast<size_t>(d)] = v;
    state.acceleration[i][static_cast<size_t>(d)] = a;
  };
  for (size_t i : support_)
    setComponent(i, loadComponent_, 0.0, 0.0, 0.0);
  for (size_t i : loaded_)
    setComponent(i, loadComponent_, prescribed.displacement,
                 prescribed.velocity, prescribed.acceleration);
  setComponent(anchor_, lateralComponent(), 0.0, 0.0, 0.0);
  if (config_.planeStrain)
    for (size_t i = 0; i < block.referencePosition.size(); ++i)
      setComponent(i, 2, 0.0, 0.0, 0.0);
}

} // namespace demo
} // namespace vumat
