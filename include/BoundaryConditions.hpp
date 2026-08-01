#pragma once

#include "DemoConfig.hpp"
#include "ExplicitTimeIntegrator.hpp"
#include "PdTypes.hpp"

#include <memory>

namespace vumat {
namespace demo {

class BoundaryCondition {
public:
  virtual ~BoundaryCondition() = default;

  virtual void apply(vumat::pd::ParticleBlock& block, int step, int steps) const = 0;
  virtual double loadValue(int step, int steps) const = 0;
};

std::unique_ptr<BoundaryCondition> createBoundaryCondition(const BoundaryConditionConfig& config);

struct RampKinematics {
  double displacement = 0.0;
  double velocity = 0.0;
  double acceleration = 0.0;
};

class RegionalConstraintSystem {
public:
  RegionalConstraintSystem(const vumat::pd::ParticleBlock& block,
                           const BoundaryConditionConfig& config,
                           double spacing);

  RampKinematics ramp(double time) const;
  void apply(vumat::pd::ParticleBlock& block,
             vumat::pd::ExplicitDynamicState& state,
             double time) const;

  const std::vector<size_t>& support() const { return support_; }
  const std::vector<size_t>& loaded() const { return loaded_; }
  size_t anchor() const { return anchor_; }
  int loadComponent() const { return loadComponent_; }
  int lateralComponent() const { return loadComponent_ == 0 ? 1 : 0; }

private:
  BoundaryConditionConfig config_;
  std::vector<size_t> support_;
  std::vector<size_t> loaded_;
  size_t anchor_ = 0;
  int loadComponent_ = 0;
};

} // namespace demo
} // namespace vumat
