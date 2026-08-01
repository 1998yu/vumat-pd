#include "VumatPdCorrespondenceMaterial.hpp"
#include "CorotationalKinematics.hpp"

#include <stdexcept>
#include <utility>

namespace vumat {
namespace pd {
namespace {

Tensor3 zeroTensor() {
  return {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
}

Tensor3 identityTensor() {
  return {1.0, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 1.0};
}

Tensor3 legacySmallStrain(const Tensor3& f) {
  Tensor3 strain{};
  strain[0] = f[0] - 1.0;
  strain[4] = f[4] - 1.0;
  strain[8] = f[8] - 1.0;
  strain[1] = strain[3] = 0.5 * (f[1] + f[3]);
  strain[2] = strain[6] = 0.5 * (f[2] + f[6]);
  strain[5] = strain[7] = 0.5 * (f[5] + f[7]);
  return strain;
}

void validateState(size_t pointCount, const MaterialState& state) {
  if (state.deformationGradientN.size() != pointCount ||
      state.leftStretchN.size() != pointCount ||
      state.rotationN.size() != pointCount ||
      state.strainN.size() != pointCount ||
      state.rawStressN.size() != pointCount ||
      state.vumatStateN.size() != pointCount ||
      state.temperatureN.size() != pointCount ||
      state.internalEnergyN.size() != pointCount ||
      state.inelasticEnergyN.size() != pointCount)
    throw std::runtime_error("VUMAT material state is not initialized for this point block.");
}

} // namespace

VumatPdCorrespondenceMaterial::VumatPdCorrespondenceMaterial(
    VumatPdCorrespondenceConfig config)
    : config_(std::move(config)), adapter_(config_.material) {
  if (config_.charLength <= 0.0)
    throw std::runtime_error("VUMAT-PD material bridge requires charLength > 0.");
  if (!config_.initialState.empty() &&
      static_cast<int>(config_.initialState.size()) != config_.material.nstatev)
    throw std::runtime_error("Initial VUMAT state size does not match nstatev.");
}

void VumatPdCorrespondenceMaterial::initialize(
    const std::vector<Tensor3>& initialDeformationGradient,
    MaterialState& state) const {
  const size_t n = initialDeformationGradient.size();
  if (n == 0)
    throw std::runtime_error("VUMAT-PD material bridge requires at least one point.");

  state.deformationGradientN = initialDeformationGradient;
  state.leftStretchN.assign(n, identityTensor());
  state.rotationN.assign(n, identityTensor());
  state.strainN.resize(n);
  for (size_t i = 0; i < n; ++i)
    state.strainN[i] = config_.kinematicsMode == KinematicsMode::Corotational
        ? greenLagrangeStrain(initialDeformationGradient[i])
        : legacySmallStrain(initialDeformationGradient[i]);
  state.rawStressN.assign(n, zeroTensor());
  const std::vector<double> initialState =
      config_.initialState.empty()
          ? std::vector<double>(static_cast<size_t>(config_.material.nstatev), 0.0)
          : config_.initialState;
  state.vumatStateN.assign(n, initialState);
  state.temperatureN.assign(n, config_.referenceTemperature);
  state.internalEnergyN.assign(n, 0.0);
  state.inelasticEnergyN.assign(n, 0.0);
}

MaterialUpdateResult VumatPdCorrespondenceMaterial::update(
    const std::vector<Tensor3>& deformationGradient,
    MaterialState& state,
    double dt,
    double stepTime,
    double totalTime) const {
  if (dt <= 0.0)
    throw std::runtime_error("VUMAT-PD material update requires dt > 0.");
  const size_t n = deformationGradient.size();
  validateState(n, state);

  MaterialUpdateResult output;
  output.deformationGradient = deformationGradient;
  output.strain.resize(n);
  output.rotation.resize(n);
  output.leftStretch.resize(n);

  VumatBlockInput input;
  input.dt = dt;
  input.stepTime = stepTime;
  input.totalTime = totalTime;
  input.charLength.assign(n, config_.charLength);
  input.temperature = state.temperatureN;
  input.strainRateFull.resize(n);
  input.relSpinIncFull.resize(n);
  input.stretchOld = state.leftStretchN;
  input.stretchNew.resize(n);
  input.strainFullN = state.strainN;
  input.stressFullN = state.rawStressN;
  input.defgradOld = state.deformationGradientN;
  input.defgradNew = deformationGradient;
  input.stateOld = state.vumatStateN;
  input.internalEnergyOld = state.internalEnergyN;
  input.inelasticEnergyOld = state.inelasticEnergyN;
  for (size_t i = 0; i < n; ++i) {
    if (config_.kinematicsMode == KinematicsMode::LegacySmallStrain) {
      output.strain[i] = legacySmallStrain(deformationGradient[i]);
      output.rotation[i] = identityTensor();
      output.leftStretch[i] = identityTensor();
      for (size_t c = 0; c < 9; ++c)
        input.strainRateFull[i][c] =
            (output.strain[i][c] - state.strainN[i][c]) / dt;
      input.relSpinIncFull[i] = zeroTensor();
      // Reproduce the former bridge exactly: stretch and relative-spin
      // arrays were zero-initialized rather than computed.
      input.stretchOld[i] = zeroTensor();
      input.stretchNew[i] = zeroTensor();
      continue;
    }
    const CorotationalUpdate kinematics = updateCorotationalKinematics(
        state.deformationGradientN[i], deformationGradient[i],
        state.leftStretchN[i], state.rotationN[i], dt);
    output.strain[i] = greenLagrangeStrain(deformationGradient[i]);
    output.rotation[i] = kinematics.rotationNP1;
    output.leftStretch[i] = kinematics.leftStretchNP1;
    input.strainRateFull[i] = kinematics.unrotatedRateOfDeformation;
    input.relSpinIncFull[i] = kinematics.relativeSpinIncrement;
    input.stretchNew[i] = kinematics.leftStretchNP1;
  }

  const VumatBlockResult result = adapter_.updateBlock(input);
  output.rawStress.resize(n);
  for (size_t i = 0; i < n; ++i)
    output.rawStress[i] = config_.kinematicsMode == KinematicsMode::Corotational
        ? rotateCorotationalStressToSpatial(result.stressFullNew[i], output.rotation[i])
        : result.stressFullNew[i];
  output.internalEnergy = result.internalEnergyNew;
  output.inelasticEnergy = result.inelasticEnergyNew;

  state.deformationGradientN = deformationGradient;
  state.leftStretchN = output.leftStretch;
  state.rotationN = output.rotation;
  state.strainN = output.strain;
  state.rawStressN = result.stressFullNew;
  state.vumatStateN = result.stateNew;
  state.temperatureN = result.temperatureNew;
  state.internalEnergyN = result.internalEnergyNew;
  state.inelasticEnergyN = result.inelasticEnergyNew;
  return output;
}

} // namespace pd
} // namespace vumat
