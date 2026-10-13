#include "SimpleDamageModel.hpp"

#include "TensorVoigtMap.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace vumat {

SimpleDamageModel::SimpleDamageModel(SimpleDamageConfig config) : config_(std::move(config)) {}

SimpleDamageState SimpleDamageModel::update(const std::array<double, 9>& strainFull,
                                            const std::vector<double>& stateNew,
                                            SimpleDamageState previous) const {
  if (config_.mode == "none")
    return previous;

  double metric = 0.0;
  if (config_.mode == "eqps_threshold") {
    if (config_.eqpsStateIndex < 0 ||
        static_cast<size_t>(config_.eqpsStateIndex) >= stateNew.size())
      throw std::runtime_error("eqpsStateIndex is outside the VUMAT state vector.");
    metric = stateNew[static_cast<size_t>(config_.eqpsStateIndex)];
  } else if (config_.mode == "max_principal_strain") {
    metric = maxPrincipalStrainSymmetric3x3(strainFull);
  } else {
    throw std::runtime_error("Unsupported simple damage mode: " + config_.mode);
  }

  const double denom = config_.failure - config_.initiation;
  if (denom <= 0.0)
    throw std::runtime_error("SimpleDamageModel requires failure > initiation.");

  SimpleDamageState next = previous;
  next.damage = std::max(previous.damage,
                         std::clamp((metric - config_.initiation) / denom, 0.0, 1.0));
  next.deleted = config_.deletionEnabled && next.damage >= 1.0;
  return next;
}

std::array<double, 9> SimpleDamageModel::degradeStress(const std::array<double, 9>& stress,
                                                       const SimpleDamageState& state) const {
  auto degraded = stress;
  const double factor = state.deleted ? 0.0 : (1.0 - std::clamp(state.damage, 0.0, 1.0));
  for (double& value : degraded)
    value *= factor;
  return degraded;
}

} // namespace vumat
