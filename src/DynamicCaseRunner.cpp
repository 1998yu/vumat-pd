#include "DynamicCaseRunner.hpp"

#include "ExplicitTimeIntegrator.hpp"
#include "Geometry.hpp"
#include "PdCorrespondenceSkeleton.hpp"
#include "PdSimpleDamage.hpp"
#include "VumatPdCorrespondenceMaterial.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace vumat {
namespace demo {
namespace {

struct RampValue {
  double displacement = 0.0;
  double velocity = 0.0;
  double acceleration = 0.0;
};

RampValue smoothRamp(double time, double amplitude, double riseTime) {
  if (time >= riseTime)
    return {amplitude, 0.0, 0.0};
  const double phase = 3.14159265358979323846 * time / riseTime;
  return {0.5 * amplitude * (1.0 - std::cos(phase)),
          0.5 * amplitude * 3.14159265358979323846 / riseTime * std::sin(phase),
          0.5 * amplitude * 3.14159265358979323846 * 3.14159265358979323846 /
              (riseTime * riseTime) * std::cos(phase)};
}

double minX(const pd::ParticleBlock& block) {
  double result = std::numeric_limits<double>::infinity();
  for (const auto& x : block.referencePosition)
    result = std::min(result, x[0]);
  return result;
}

void applyConstraints(pd::ParticleBlock& block,
                      pd::ExplicitDynamicState& state,
                      double time,
                      const BoundaryConditionConfig& bc,
                      double loadedX) {
  const RampValue ramp = smoothRamp(time, bc.amplitude, bc.riseTime);
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    if (std::abs(block.referencePosition[i][0] - loadedX) < 1.0e-12) {
      block.displacement[i][0] = ramp.displacement;
      state.velocity[i][0] = ramp.velocity;
      state.acceleration[i][0] = ramp.acceleration;
    }
    if (bc.transverseConstraint) {
      block.displacement[i][1] = 0.0;
      block.displacement[i][2] = 0.0;
      state.velocity[i][1] = 0.0;
      state.velocity[i][2] = 0.0;
      state.acceleration[i][1] = 0.0;
      state.acceleration[i][2] = 0.0;
    }
  }
}

std::vector<size_t> probeIndices(const pd::ParticleBlock& block, double probeX) {
  double nearest = std::numeric_limits<double>::infinity();
  for (const auto& x : block.referencePosition)
    nearest = std::min(nearest, std::abs(x[0] - probeX));
  std::vector<size_t> indices;
  for (size_t i = 0; i < block.referencePosition.size(); ++i)
    if (std::abs(std::abs(block.referencePosition[i][0] - probeX) - nearest) < 1.0e-12)
      indices.push_back(i);
  return indices;
}

bool finiteTensor(const pd::Tensor3& tensor) {
  return std::all_of(tensor.begin(), tensor.end(),
                     [](double value) { return std::isfinite(value); });
}

double elasticEnergy(const pd::ParticleBlock& block,
                     const pd::MaterialUpdateResult& material) {
  double energy = 0.0;
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const auto& s = material.rawStress[i];
    const auto& e = material.strain[i];
    const double density = s[0] * e[0] + s[4] * e[4] + s[8] * e[8] +
                           2.0 * (s[1] * e[1] + s[2] * e[2] + s[5] * e[5]);
    energy += 0.5 * density * block.volume[i];
  }
  return energy;
}

double kineticEnergy(const pd::ParticleBlock& block,
                     const pd::ExplicitDynamicState& state,
                     double density) {
  double energy = 0.0;
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const auto& v = state.velocity[i];
    energy += 0.5 * density * block.volume[i] *
              (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  }
  return energy;
}

} // namespace

int runDynamicCase(const DemoCaseConfig& config) {
  const auto& materialSpec = config.materials.front();
  auto block = makeParticlePatch(config.geometry.nx, config.geometry.ny,
                                 config.geometry.nz, config.geometry.spacing,
                                 config.geometry.horizon);

  pd::PdCorrespondenceSkeleton skeleton(config.geometry.horizon);
  pd::CorrespondenceState correspondenceState;
  skeleton.initialize(block, correspondenceState);

  pd::VumatPdCorrespondenceConfig materialConfig;
  materialConfig.material = materialSpec.material;
  materialConfig.charLength = config.geometry.spacing;
  materialConfig.referenceTemperature = materialSpec.referenceTemperature;
  materialConfig.kinematicsMode =
      materialSpec.kinematicsMode == "legacy_small_strain"
      ? pd::KinematicsMode::LegacySmallStrain
      : pd::KinematicsMode::Corotational;
  materialConfig.initialState = materialSpec.initialState.empty()
      ? std::vector<double>(static_cast<size_t>(materialSpec.material.nstatev), 0.0)
      : materialSpec.initialState;
  pd::VumatPdCorrespondenceMaterial materialBridge(materialConfig);
  pd::MaterialState materialState;
  materialBridge.initialize(skeleton.computeDeformationGradients(block, correspondenceState),
                            materialState);

  pd::PdSimpleDamage damageModel(config.damage);
  pd::PdDamageState damageState;
  damageModel.initialize(block, damageState);

  pd::ExplicitTimeIntegrator integrator(config.dynamics.density);
  pd::ExplicitDynamicState dynamicState;
  integrator.initialize(block, dynamicState);
  const double loadedX = minX(block);
  applyConstraints(block, dynamicState, 0.0, config.boundaryCondition, loadedX);

  const std::vector<size_t> probes = probeIndices(block, config.dynamics.probeX);
  const double lambda = materialSpec.elasticE * materialSpec.elasticNu /
      ((1.0 + materialSpec.elasticNu) * (1.0 - 2.0 * materialSpec.elasticNu));
  const double mu = materialSpec.elasticE / (2.0 * (1.0 + materialSpec.elasticNu));
  const double theoreticalWaveSpeed =
      std::sqrt((lambda + 2.0 * mu) / config.dynamics.density);
  const double actualProbeX = block.referencePosition[probes.front()][0];
  const double expectedArrivalTime = (actualProbeX - loadedX) / theoreticalWaveSpeed;
  const double peakBoundaryVelocity = std::abs(config.boundaryCondition.amplitude) *
      3.14159265358979323846 / (2.0 * config.boundaryCondition.riseTime);
  const double arrivalThreshold =
      config.dynamics.arrivalThresholdFraction * peakBoundaryVelocity;

  std::filesystem::create_directories(config.output.directory);
  std::ofstream timeOut(config.output.directory + "/timeseries.csv");
  std::ofstream fieldOut(config.output.directory + "/field_final.csv");
  if (!timeOut || !fieldOut)
    throw std::runtime_error("Failed to open dynamic validation output files.");
  timeOut << "step,time,boundary_displacement,boundary_velocity,probe_velocity_x,"
             "probe_stress_xx,max_velocity_x,kinetic_energy,elastic_energy,"
             "external_work,relative_energy_error,net_internal_force_x,max_damage,finite\n";

  double measuredArrivalTime = std::numeric_limits<double>::quiet_NaN();
  double externalWork = 0.0;
  double previousPower = 0.0;
  double maxRelativeEnergyError = 0.0;
  double maxNetInternalForce = 0.0;
  double maxDamage = 0.0;
  bool allFinite = true;
  pd::MaterialUpdateResult finalMaterial;
  pd::PdDamageResult finalDamage;
  std::vector<pd::Vec3> finalForce;

  for (int step = 1; step <= config.timeIntegration.steps; ++step) {
    const double time = step * config.timeIntegration.dt;
    integrator.predictDisplacement(block, dynamicState, config.timeIntegration.dt);
    applyConstraints(block, dynamicState, time, config.boundaryCondition, loadedX);

    const auto deformationGradient =
        skeleton.computeDeformationGradients(block, correspondenceState);
    auto material = materialBridge.update(
        deformationGradient, materialState, config.timeIntegration.dt,
        (step - 1) * config.timeIntegration.dt,
        (step - 1) * config.timeIntegration.dt);
    auto damage = damageModel.update(block, material.strain, material.rawStress,
                                     materialState.vumatStateN, damageState);
    auto force = skeleton.computeForceDensity(
        block, correspondenceState, material.deformationGradient,
        damage.effectiveStress, damage.bondDamage);
    integrator.correctVelocity(dynamicState, force, config.timeIntegration.dt);
    applyConstraints(block, dynamicState, time, config.boundaryCondition, loadedX);

    double probeVelocity = 0.0;
    double probeStress = 0.0;
    for (size_t index : probes) {
      probeVelocity += dynamicState.velocity[index][0];
      probeStress += material.rawStress[index][0];
    }
    probeVelocity /= static_cast<double>(probes.size());
    probeStress /= static_cast<double>(probes.size());
    if (!std::isfinite(measuredArrivalTime) && std::abs(probeVelocity) >= arrivalThreshold)
      measuredArrivalTime = time;

    double maxVelocity = 0.0;
    double netInternalForceX = 0.0;
    double externalForceX = 0.0;
    const RampValue ramp = smoothRamp(time, config.boundaryCondition.amplitude,
                                      config.boundaryCondition.riseTime);
    for (size_t i = 0; i < block.referencePosition.size(); ++i) {
      maxVelocity = std::max(maxVelocity, std::abs(dynamicState.velocity[i][0]));
      netInternalForceX += force[i][0] * block.volume[i];
      maxDamage = std::max(maxDamage, damage.damageIndex[i]);
      allFinite = allFinite && finiteTensor(material.rawStress[i]) &&
                  finiteTensor(material.strain[i]) && std::isfinite(damage.damageIndex[i]);
      for (int d = 0; d < 3; ++d)
        allFinite = allFinite && std::isfinite(dynamicState.velocity[i][d]) &&
                    std::isfinite(dynamicState.acceleration[i][d]) &&
                    std::isfinite(force[i][d]);
      if (std::abs(block.referencePosition[i][0] - loadedX) < 1.0e-12)
        externalForceX += (config.dynamics.density * ramp.acceleration - force[i][0]) *
                          block.volume[i];
    }
    const double power = externalForceX * ramp.velocity;
    externalWork += 0.5 * config.timeIntegration.dt * (previousPower + power);
    previousPower = power;
    const double kinetic = kineticEnergy(block, dynamicState, config.dynamics.density);
    const double elastic = elasticEnergy(block, material);
    const double energyScale = std::max({std::abs(externalWork), kinetic + std::abs(elastic),
                                         1.0e-30});
    const double relativeEnergyError =
        std::abs(kinetic + elastic - externalWork) / energyScale;
    maxRelativeEnergyError = std::max(maxRelativeEnergyError, relativeEnergyError);
    maxNetInternalForce = std::max(maxNetInternalForce, std::abs(netInternalForceX));
    allFinite = allFinite && std::isfinite(kinetic) && std::isfinite(elastic) &&
                std::isfinite(externalWork) && std::isfinite(relativeEnergyError);

    if (step % config.dynamics.outputEvery == 0 || step == 1 ||
        step == config.timeIntegration.steps) {
      timeOut << step << ',' << std::setprecision(16) << time << ','
              << ramp.displacement << ',' << ramp.velocity << ','
              << probeVelocity << ',' << probeStress << ',' << maxVelocity << ','
              << kinetic << ',' << elastic << ',' << externalWork << ','
              << relativeEnergyError << ',' << netInternalForceX << ','
              << maxDamage << ',' << (allFinite ? 1 : 0) << '\n';
    }
    finalMaterial = std::move(material);
    finalDamage = std::move(damage);
    finalForce = std::move(force);
  }

  const double relativeArrivalError = std::isfinite(measuredArrivalTime)
      ? std::abs(measuredArrivalTime - expectedArrivalTime) / expectedArrivalTime
      : std::numeric_limits<double>::infinity();
  const bool pass = allFinite && maxDamage == 0.0 &&
      relativeArrivalError <= config.dynamics.arrivalToleranceRelative &&
      maxRelativeEnergyError <= config.dynamics.energyToleranceRelative &&
      maxNetInternalForce <= config.dynamics.netForceTolerance;

  fieldOut << "particle,x0,y0,z0,ux,uy,uz,vx,vy,vz,ax,ay,az,"
              "strain_xx,raw_stress_xx,damage,force_x,force_y,force_z\n";
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    fieldOut << i;
    for (double value : block.referencePosition[i]) fieldOut << ',' << value;
    for (double value : block.displacement[i]) fieldOut << ',' << value;
    for (double value : dynamicState.velocity[i]) fieldOut << ',' << value;
    for (double value : dynamicState.acceleration[i]) fieldOut << ',' << value;
    fieldOut << ',' << finalMaterial.strain[i][0]
             << ',' << finalMaterial.rawStress[i][0]
             << ',' << finalDamage.damageIndex[i]
             << ',' << finalForce[i][0]
             << ',' << finalForce[i][1]
             << ',' << finalForce[i][2] << '\n';
  }

  std::ofstream metadata(config.output.directory + "/metadata.json");
  metadata << "{\n"
           << "  \"name\": \"" << config.name << "\",\n"
           << "  \"status\": \"" << (pass ? "pass" : "fail") << "\",\n"
           << "  \"scope\": \"" << config.scope << "\",\n"
           << "  \"particles\": " << block.referencePosition.size() << ",\n"
           << "  \"steps\": " << config.timeIntegration.steps << ",\n"
           << "  \"theoreticalWaveSpeed\": " << std::setprecision(16)
           << theoreticalWaveSpeed << ",\n"
           << "  \"probeX\": " << actualProbeX << ",\n"
           << "  \"expectedArrivalTime\": " << expectedArrivalTime << ",\n"
           << "  \"measuredArrivalTime\": " << measuredArrivalTime << ",\n"
           << "  \"relativeArrivalError\": " << relativeArrivalError << ",\n"
           << "  \"arrivalToleranceRelative\": "
           << config.dynamics.arrivalToleranceRelative << ",\n"
           << "  \"maxRelativeEnergyError\": " << maxRelativeEnergyError << ",\n"
           << "  \"energyToleranceRelative\": "
           << config.dynamics.energyToleranceRelative << ",\n"
           << "  \"maxNetInternalForce\": " << maxNetInternalForce << ",\n"
           << "  \"netForceTolerance\": " << config.dynamics.netForceTolerance << ",\n"
           << "  \"maxDamage\": " << maxDamage << ",\n"
           << "  \"allValuesFinite\": " << (allFinite ? "true" : "false") << "\n"
           << "}\n";

  std::cout << config.name << ": theoretical_wave_speed=" << theoreticalWaveSpeed
            << " expected_arrival=" << expectedArrivalTime
            << " measured_arrival=" << measuredArrivalTime
            << " relative_error=" << relativeArrivalError
            << " max_energy_error=" << maxRelativeEnergyError
            << " finite=" << (allFinite ? "true" : "false") << '\n';
  return pass ? 0 : 2;
}

} // namespace demo
} // namespace vumat
