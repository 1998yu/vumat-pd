#include "VumatPdCorrespondenceMaterial.hpp"
#include "CorotationalKinematics.hpp"

#include <stdexcept>
#include <utility>
#include <algorithm>
#include <omp.h>

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
  for(int t=0;t<omp_get_max_threads();++t)
    threadAdapters_.push_back(std::make_unique<VumatMaterialAdapter>(config_.material));
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
  #pragma omp parallel for schedule(static)
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


  output.rawStress.resize(n);
  output.internalEnergy.resize(n);
  output.inelasticEnergy.resize(n);
  constexpr size_t width=256;
  const size_t batches=(n+width-1)/width;
  #pragma omp parallel
  {
    VumatBlockInput input;
    input.dt=dt;input.stepTime=stepTime;input.totalTime=totalTime;
    input.charLength.assign(width,config_.charLength);
    input.temperature.resize(width);input.strainRateFull.resize(width);
    input.relSpinIncFull.resize(width);input.stretchOld.resize(width);input.stretchNew.resize(width);
    input.strainFullN.resize(width);input.stressFullN.resize(width);
    input.defgradOld.resize(width);input.defgradNew.resize(width);
    input.stateOld.assign(width,std::vector<double>(static_cast<size_t>(config_.material.nstatev)));
    input.internalEnergyOld.resize(width);input.inelasticEnergyOld.resize(width);
    #pragma omp for schedule(static)
    for(size_t b=0;b<batches;++b) {
      const size_t begin=b*width,count=std::min(width,n-begin);
      for(size_t j=0;j<count;++j) {
        const size_t i=begin+j;
        const auto kinematics=updateCorotationalKinematics(state.deformationGradientN[i],deformationGradient[i],state.leftStretchN[i],state.rotationN[i],dt);
        output.strain[i]=greenLagrangeStrain(deformationGradient[i]);
        output.rotation[i]=kinematics.rotationNP1;output.leftStretch[i]=kinematics.leftStretchNP1;
        input.temperature[j]=state.temperatureN[i];input.strainRateFull[j]=kinematics.unrotatedRateOfDeformation;
        input.relSpinIncFull[j]=kinematics.relativeSpinIncrement;
        input.stretchOld[j]=state.leftStretchN[i];input.stretchNew[j]=kinematics.leftStretchNP1;
        input.strainFullN[j]=state.strainN[i];input.stressFullN[j]=state.rawStressN[i];
        input.defgradOld[j]=state.deformationGradientN[i];input.defgradNew[j]=deformationGradient[i];
        input.stateOld[j]=state.vumatStateN[i];input.internalEnergyOld[j]=state.internalEnergyN[i];input.inelasticEnergyOld[j]=state.inelasticEnergyN[i];
      }
      for(size_t j=count;j<width;++j) {
        const size_t last=count-1;
        input.temperature[j]=input.temperature[last];input.strainRateFull[j]=input.strainRateFull[last];input.relSpinIncFull[j]=input.relSpinIncFull[last];
        input.stretchOld[j]=input.stretchOld[last];input.stretchNew[j]=input.stretchNew[last];
        input.strainFullN[j]=input.strainFullN[last];input.stressFullN[j]=input.stressFullN[last];
        input.defgradOld[j]=input.defgradOld[last];input.defgradNew[j]=input.defgradNew[last];
        input.stateOld[j]=input.stateOld[last];input.internalEnergyOld[j]=input.internalEnergyOld[last];input.inelasticEnergyOld[j]=input.inelasticEnergyOld[last];
      }
      const auto& result=threadAdapters_[omp_get_thread_num()]->updateFixedBlockCached(input);
      for(size_t j=0;j<count;++j) {
        const size_t i=begin+j;
        output.rawStress[i]=rotateCorotationalStressToSpatial(result.stressFullNew[j],output.rotation[i]);
        output.internalEnergy[i]=result.internalEnergyNew[j];output.inelasticEnergy[i]=result.inelasticEnergyNew[j];
        state.deformationGradientN[i]=deformationGradient[i];state.leftStretchN[i]=output.leftStretch[i];state.rotationN[i]=output.rotation[i];
        state.strainN[i]=output.strain[i];state.rawStressN[i]=result.stressFullNew[j];
        state.vumatStateN[i]=result.stateNew[j];state.temperatureN[i]=result.temperatureNew[j];
        state.internalEnergyN[i]=result.internalEnergyNew[j];state.inelasticEnergyN[i]=result.inelasticEnergyNew[j];
      }
    }
  }
  return output;
}

} // namespace pd
} // namespace vumat
