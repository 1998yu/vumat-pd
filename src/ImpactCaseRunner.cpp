#include "ImpactCaseRunner.hpp"

#include "ExplicitTimeIntegrator.hpp"
#include "Geometry.hpp"
#include "PdCorrespondenceSkeleton.hpp"
#include "PdSimpleDamage.hpp"
#include "RigidWallContact.hpp"
#include "VumatPdCorrespondenceMaterial.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace vumat {
namespace demo {
namespace {

double contraction(const pd::Tensor3& stress, const pd::Tensor3& strain) {
  return stress[0] * strain[0] + stress[4] * strain[4] + stress[8] * strain[8] +
      2.0 * (stress[1] * strain[1] + stress[2] * strain[2] + stress[5] * strain[5]);
}

bool finiteTensor(const pd::Tensor3& value) {
  return std::all_of(value.begin(), value.end(),
                     [](double item) { return std::isfinite(item); });
}

double vonMises(const pd::Tensor3& stress) {
  const double mean = (stress[0] + stress[4] + stress[8]) / 3.0;
  const double sxx = stress[0] - mean;
  const double syy = stress[4] - mean;
  const double szz = stress[8] - mean;
  return std::sqrt(1.5 * (sxx * sxx + syy * syy + szz * szz +
                          2.0 * (stress[1] * stress[1] + stress[2] * stress[2] +
                                 stress[5] * stress[5])));
}

double kineticEnergy(const pd::ParticleBlock& block,
                     const pd::ExplicitDynamicState& state, double density) {
  double value = 0.0;
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const auto& v = state.velocity[i];
    value += 0.5 * density * block.volume[i] *
        (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  }
  return value;
}

pd::Vec3 momentum(const pd::ParticleBlock& block,
                  const pd::ExplicitDynamicState& state, double density) {
  pd::Vec3 value{0.0, 0.0, 0.0};
  for (size_t i = 0; i < block.referencePosition.size(); ++i)
    for (int d = 0; d < 3; ++d)
      value[static_cast<size_t>(d)] += density * block.volume[i] *
          state.velocity[i][static_cast<size_t>(d)];
  return value;
}

double averageRearVelocity(const pd::ParticleBlock& block,
                           const pd::ExplicitDynamicState& state, double spacing) {
  double maximum = -std::numeric_limits<double>::infinity();
  for (const auto& x : block.referencePosition) maximum = std::max(maximum, x[0]);
  double sum = 0.0;
  size_t count = 0;
  for (size_t i = 0; i < block.referencePosition.size(); ++i)
    if (maximum - block.referencePosition[i][0] <= 0.51 * spacing) {
      sum += state.velocity[i][0];
      ++count;
    }
  return count ? sum / static_cast<double>(count) : 0.0;
}

void writeSnapshot(std::ofstream& out, int step, double time,
                   const pd::ParticleBlock& block,
                   const pd::ExplicitDynamicState& dynamicState,
                   const pd::MaterialUpdateResult& material,
                   const pd::MaterialState& materialState,
                   const DemoMaterialConfig& materialConfig) {
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const double eqps = materialConfig.equivalentPlasticStrainIndex >= 0
        ? materialState.vumatStateN[i][static_cast<size_t>(materialConfig.equivalentPlasticStrainIndex)]
        : 0.0;
    const double temperature = materialState.vumatStateN[i].size() > 2
        ? materialState.vumatStateN[i][2] : materialConfig.referenceTemperature;
    out << step << ',' << std::setprecision(16) << time << ',' << i;
    for (double value : block.referencePosition[i]) out << ',' << value;
    for (int d = 0; d < 3; ++d)
      out << ',' << block.referencePosition[i][static_cast<size_t>(d)] +
          block.displacement[i][static_cast<size_t>(d)];
    for (double value : dynamicState.velocity[i]) out << ',' << value;
    out << ',' << material.rawStress[i][0] << ',' << material.rawStress[i][4]
        << ',' << material.rawStress[i][8] << ',' << vonMises(material.rawStress[i])
        << ',' << eqps << ',' << temperature << '\n';
  }
}

struct FinalDimensions {
  double length = 0.0;
  double diameter = 0.0;
  double minX = 0.0;
  double maxX = 0.0;
  double centerY = 0.0;
  double centerZ = 0.0;
};

FinalDimensions finalDimensions(const pd::ParticleBlock& block) {
  FinalDimensions result;
  result.minX = std::numeric_limits<double>::infinity();
  result.maxX = -std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const double x = block.referencePosition[i][0] + block.displacement[i][0];
    result.minX = std::min(result.minX, x);
    result.maxX = std::max(result.maxX, x);
    result.centerY += block.referencePosition[i][1] + block.displacement[i][1];
    result.centerZ += block.referencePosition[i][2] + block.displacement[i][2];
  }
  result.centerY /= static_cast<double>(block.referencePosition.size());
  result.centerZ /= static_cast<double>(block.referencePosition.size());
  double maxRadius = 0.0;
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const double y = block.referencePosition[i][1] + block.displacement[i][1] - result.centerY;
    const double z = block.referencePosition[i][2] + block.displacement[i][2] - result.centerZ;
    maxRadius = std::max(maxRadius, std::sqrt(y * y + z * z));
  }
  result.length = result.maxX - result.minX;
  result.diameter = 2.0 * maxRadius;
  return result;
}

void writeFinalProfile(const std::string& path, const pd::ParticleBlock& block,
                       const FinalDimensions& dimensions, double spacing) {
  const int bins = std::max(8, static_cast<int>(std::ceil(dimensions.length / spacing)) + 1);
  std::vector<double> radius(static_cast<size_t>(bins), 0.0);
  std::vector<int> count(static_cast<size_t>(bins), 0);
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const double x = block.referencePosition[i][0] + block.displacement[i][0];
    const double y = block.referencePosition[i][1] + block.displacement[i][1] - dimensions.centerY;
    const double z = block.referencePosition[i][2] + block.displacement[i][2] - dimensions.centerZ;
    const double fraction = dimensions.length > 0.0 ? (x - dimensions.minX) / dimensions.length : 0.0;
    const int bin = std::clamp(static_cast<int>(std::floor(fraction * (bins - 1))), 0, bins - 1);
    radius[static_cast<size_t>(bin)] =
        std::max(radius[static_cast<size_t>(bin)], std::sqrt(y * y + z * z));
    ++count[static_cast<size_t>(bin)];
  }
  std::ofstream out(path);
  out << "axial_mm,radius_mm,diameter_mm,point_count\n";
  for (int bin = 0; bin < bins; ++bin)
    if (count[static_cast<size_t>(bin)] > 0)
      out << std::setprecision(16)
          << dimensions.length * static_cast<double>(bin) / static_cast<double>(bins - 1)
          << ',' << radius[static_cast<size_t>(bin)] << ','
          << 2.0 * radius[static_cast<size_t>(bin)] << ','
          << count[static_cast<size_t>(bin)] << '\n';
}

std::string jsonNumber(double value) {
  if (!std::isfinite(value)) return "null";
  std::ostringstream out;
  out << std::setprecision(15) << value;
  return out.str();
}

} // namespace

int runImpactCase(const DemoCaseConfig& config) {
  const auto started = std::chrono::steady_clock::now();
  const auto& materialConfig = config.materials.front();
  auto block = makeCylinder(config.geometry.length, config.geometry.diameter,
                            config.geometry.spacing, config.geometry.horizon,
                            config.geometry.initialGap);

  pd::PdCorrespondenceSkeleton skeleton(config.geometry.horizon);
  pd::CorrespondenceState correspondenceState;
  skeleton.initialize(block, correspondenceState);

  pd::VumatPdCorrespondenceConfig bridgeConfig;
  bridgeConfig.material = materialConfig.material;
  bridgeConfig.charLength = config.geometry.spacing;
  bridgeConfig.referenceTemperature = materialConfig.referenceTemperature;
  bridgeConfig.kinematicsMode =
      materialConfig.kinematicsMode == "legacy_small_strain"
      ? pd::KinematicsMode::LegacySmallStrain
      : pd::KinematicsMode::Corotational;
  bridgeConfig.initialState = materialConfig.initialState.empty()
      ? std::vector<double>(static_cast<size_t>(materialConfig.material.nstatev), 0.0)
      : materialConfig.initialState;
  pd::VumatPdCorrespondenceMaterial bridge(bridgeConfig);
  pd::MaterialState materialState;
  bridge.initialize(skeleton.computeDeformationGradients(block, correspondenceState),
                    materialState);

  pd::PdSimpleDamage damage(config.damage);
  pd::PdDamageState damageState;
  damage.initialize(block, damageState);
  pd::ExplicitTimeIntegrator integrator(config.dynamics.density);
  pd::ExplicitDynamicState dynamicState;
  integrator.initialize(block, dynamicState);
  for (auto& velocity : dynamicState.velocity) velocity = config.initialConditions.velocity;
  pd::RigidWallContact contact(config.contact);

  std::filesystem::create_directories(config.output.directory);
  std::ofstream timeseries(config.output.directory + "/timeseries.csv");
  std::ofstream snapshots(config.output.directory + "/field_snapshots.csv");
  if (!timeseries || !snapshots)
    throw std::runtime_error("Failed to open impact output files.");
  timeseries << "step,time_ms,contact_force_N,rear_velocity_mm_per_ms,momentum_x_Nms,"
      "momentum_change_x_Nms,wall_impulse_Nms,relative_momentum_impulse_error,"
      "kinetic_energy_Nmm,internal_energy_Nmm,contact_energy_Nmm,friction_work_Nmm,"
      "relative_energy_error,max_penetration_mm,active_contact_particles,max_von_mises_MPa,"
      "max_eqps,max_temperature_K,net_internal_force_N,finite\n";
  snapshots << "step,time_ms,particle,x0_mm,y0_mm,z0_mm,x_mm,y_mm,z_mm,"
      "vx_mm_per_ms,vy_mm_per_ms,vz_mm_per_ms,stress_xx_MPa,stress_yy_MPa,"
      "stress_zz_MPa,von_mises_MPa,eqps,temperature_K\n";

  const double initialKinetic = kineticEnergy(block, dynamicState, config.dynamics.density);
  const pd::Vec3 initialMomentum = momentum(block, dynamicState, config.dynamics.density);
  double internalEnergy = 0.0;
  double wallImpulse = 0.0;
  double previousContactForce = 0.0;
  double frictionWork = 0.0;
  double previousFrictionPower = 0.0;
  double maxPenetration = 0.0;
  bool finite = true;
  pd::MaterialUpdateResult finalMaterial;

  for (int step = 1; step <= config.timeIntegration.steps; ++step) {
    const double time = static_cast<double>(step) * config.timeIntegration.dt;
    integrator.predictDisplacement(block, dynamicState, config.timeIntegration.dt);
    const auto previousStress = materialState.rawStressN;
    const auto previousStrain = materialState.strainN;
    const auto deformationGradient =
        skeleton.computeDeformationGradients(block, correspondenceState);
    auto material = bridge.update(deformationGradient, materialState,
                                  config.timeIntegration.dt,
                                  time - config.timeIntegration.dt, time);
    auto damageResult = damage.update(block, material.strain, material.rawStress,
                                      materialState.vumatStateN, damageState);
    auto internalForce = skeleton.computeForceDensity(
        block, correspondenceState, material.deformationGradient,
        damageResult.effectiveStress, damageResult.bondDamage);
    const auto contactResult = contact.evaluate(block, dynamicState);
    std::vector<pd::Vec3> totalForce = internalForce;
    for (size_t i = 0; i < totalForce.size(); ++i)
      for (int d = 0; d < 3; ++d)
        totalForce[i][static_cast<size_t>(d)] +=
            contactResult.forceDensity[i][static_cast<size_t>(d)];
    integrator.correctVelocity(dynamicState, totalForce, config.timeIntegration.dt);

    for (size_t i = 0; i < block.referencePosition.size(); ++i) {
      pd::Tensor3 averageStress{}, strainIncrement{};
      for (size_t c = 0; c < 9; ++c) {
        averageStress[c] = 0.5 * (previousStress[i][c] + material.rawStress[i][c]);
        strainIncrement[c] = material.strain[i][c] - previousStrain[i][c];
      }
      internalEnergy += contraction(averageStress, strainIncrement) * block.volume[i];
    }
    wallImpulse += 0.5 * config.timeIntegration.dt *
        (previousContactForce + contactResult.forceOnBody[0]);
    previousContactForce = contactResult.forceOnBody[0];
    frictionWork += 0.5 * config.timeIntegration.dt *
        (previousFrictionPower + contactResult.frictionPower);
    previousFrictionPower = contactResult.frictionPower;
    maxPenetration = std::max(maxPenetration, contactResult.maxPenetration);

    const pd::Vec3 currentMomentum = momentum(block, dynamicState, config.dynamics.density);
    const double momentumChange = currentMomentum[0] - initialMomentum[0];
    const double momentumError = std::abs(momentumChange - wallImpulse) /
        std::max({std::abs(momentumChange), std::abs(wallImpulse), 1.0e-12});
    const double kinetic = kineticEnergy(block, dynamicState, config.dynamics.density);
    const double energyError = std::abs(
        kinetic + internalEnergy + contactResult.potentialEnergy - frictionWork - initialKinetic) /
        std::max(initialKinetic, 1.0e-12);
    double maxStress = 0.0, maxEqps = 0.0, maxTemperature = materialConfig.referenceTemperature;
    pd::Vec3 netInternal{0.0, 0.0, 0.0};
    for (size_t i = 0; i < block.referencePosition.size(); ++i) {
      maxStress = std::max(maxStress, vonMises(material.rawStress[i]));
      if (materialConfig.equivalentPlasticStrainIndex >= 0)
        maxEqps = std::max(maxEqps, materialState.vumatStateN[i]
            [static_cast<size_t>(materialConfig.equivalentPlasticStrainIndex)]);
      if (materialState.vumatStateN[i].size() > 2)
        maxTemperature = std::max(maxTemperature, materialState.vumatStateN[i][2]);
      finite = finite && finiteTensor(material.rawStress[i]) &&
          finiteTensor(material.strain[i]);
      for (int d = 0; d < 3; ++d) {
        netInternal[static_cast<size_t>(d)] +=
            internalForce[i][static_cast<size_t>(d)] * block.volume[i];
        finite = finite && std::isfinite(dynamicState.velocity[i][static_cast<size_t>(d)]) &&
            std::isfinite(dynamicState.acceleration[i][static_cast<size_t>(d)]) &&
            std::isfinite(totalForce[i][static_cast<size_t>(d)]);
      }
      for (double value : materialState.vumatStateN[i])
        finite = finite && std::isfinite(value);
    }
    const double netInternalMagnitude = std::sqrt(
        netInternal[0] * netInternal[0] + netInternal[1] * netInternal[1] +
        netInternal[2] * netInternal[2]);
    finite = finite && std::isfinite(kinetic) && std::isfinite(internalEnergy) &&
        std::isfinite(energyError) && std::isfinite(momentumError);

    if (step % config.dynamics.outputEvery == 0 || step == 1 ||
        step == config.timeIntegration.steps)
      timeseries << step << ',' << std::setprecision(16) << time << ','
          << contactResult.normalReaction << ','
          << averageRearVelocity(block, dynamicState, config.geometry.spacing) << ','
          << currentMomentum[0] << ',' << momentumChange << ',' << wallImpulse << ','
          << momentumError << ',' << kinetic << ',' << internalEnergy << ','
          << contactResult.potentialEnergy << ',' << frictionWork << ',' << energyError << ','
          << contactResult.maxPenetration << ',' << contactResult.activeParticles << ','
          << maxStress << ',' << maxEqps << ',' << maxTemperature << ','
          << netInternalMagnitude << ',' << (finite ? 1 : 0) << '\n';
    if (step % config.dynamics.snapshotEvery == 0 || step == 1 ||
        step == config.timeIntegration.steps)
      writeSnapshot(snapshots, step, time, block, dynamicState, material,
                    materialState, materialConfig);
    finalMaterial = std::move(material);
  }

  const auto finalContact = contact.evaluate(block, dynamicState);
  const pd::Vec3 finalMomentum = momentum(block, dynamicState, config.dynamics.density);
  const double momentumChange = finalMomentum[0] - initialMomentum[0];
  const double momentumError = std::abs(momentumChange - wallImpulse) /
      std::max({std::abs(momentumChange), std::abs(wallImpulse), 1.0e-12});
  const double finalKinetic = kineticEnergy(block, dynamicState, config.dynamics.density);
  const double finalEnergyError = std::abs(
      finalKinetic + internalEnergy + finalContact.potentialEnergy - frictionWork - initialKinetic) /
      std::max(initialKinetic, 1.0e-12);
  const FinalDimensions dimensions = finalDimensions(block);
  writeFinalProfile(config.output.directory + "/final_profile.csv", block, dimensions,
                    config.geometry.spacing);

  const double lengthError = config.benchmark.experimentalFinalLength > 0.0
      ? std::abs(dimensions.length - config.benchmark.experimentalFinalLength) /
          config.benchmark.experimentalFinalLength : std::numeric_limits<double>::quiet_NaN();
  const double diameterError = config.benchmark.experimentalMushroomDiameter > 0.0
      ? std::abs(dimensions.diameter - config.benchmark.experimentalMushroomDiameter) /
          config.benchmark.experimentalMushroomDiameter : std::numeric_limits<double>::quiet_NaN();
  const bool pass = finite && momentumError <= config.dynamics.momentumImpulseToleranceRelative &&
      finalEnergyError <= config.dynamics.energyToleranceRelative &&
      maxPenetration <= config.dynamics.penetrationSpacingTolerance * config.geometry.spacing;
  const double runtime = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - started).count();

  std::ofstream metadata(config.output.directory + "/metadata.json");
  metadata << "{\n  \"name\": \"" << config.name << "\",\n"
      << "  \"status\": \"" << (pass ? "pass" : "fail") << "\",\n"
      << "  \"classification\": \"" << config.classification << "\",\n"
      << "  \"scope\": \"" << config.scope << "\",\n"
      << "  \"benchmark\": {\"name\": \"" << config.benchmark.name
      << "\", \"source\": \"" << config.benchmark.source
      << "\", \"citation\": \"" << config.benchmark.citation
      << "\", \"doi\": \"" << config.benchmark.doi
      << "\", \"referenceProfilePath\": \"" << config.benchmark.referenceProfilePath << "\"},\n"
      << "  \"units\": {\"length\": \"mm\", \"time\": \"ms\", \"force\": \"N\", "
         "\"stress\": \"MPa\", \"solverMassDensity\": \"N*ms^2/mm^4\", "
         "\"sourceDensity\": \"kg/m^3\"},\n"
      << "  \"densityConversion\": \"rho_solver = rho_kg_per_m3 * 1e-6\",\n"
      << "  \"geometry\": {\"type\": \"cylinder\", \"initialLength_mm\": "
      << config.geometry.length << ", \"initialDiameter_mm\": " << config.geometry.diameter
      << ", \"initialGap_mm\": " << config.geometry.initialGap << "},\n"
      << "  \"particles\": " << block.referencePosition.size() << ",\n"
      << "  \"spacing_mm\": " << config.geometry.spacing << ",\n"
      << "  \"horizon_mm\": " << config.geometry.horizon << ",\n"
      << "  \"horizonSpacingRatio\": " << config.geometry.horizon / config.geometry.spacing << ",\n"
      << "  \"dt_ms\": " << config.timeIntegration.dt << ",\n"
      << "  \"steps\": " << config.timeIntegration.steps << ",\n"
      << "  \"initialVelocity_mm_per_ms\": [" << config.initialConditions.velocity[0] << ','
      << config.initialConditions.velocity[1] << ',' << config.initialConditions.velocity[2] << "],\n"
      << "  \"contact\": {\"enabled\": true, \"point_mm\": ["
      << config.contact.point[0] << ',' << config.contact.point[1] << ',' << config.contact.point[2]
      << "], \"normal\": [" << config.contact.normal[0] << ',' << config.contact.normal[1] << ','
      << config.contact.normal[2] << "], \"penaltyStiffness_N_per_mm4\": "
      << config.contact.penaltyStiffness << ", \"frictionCoefficient\": "
      << config.contact.frictionCoefficient << "},\n"
      << "  \"materialConfiguration\": \"" << materialConfig.material.name << "\",\n"
      << "  \"originalVumatSourceHashSha256\": \""
      << config.benchmark.originalVumatSourceHash << "\",\n"
      << "  \"outerSimpleDamage\": false,\n  \"materialDeletion\": false,\n"
      << "  \"runtimeSeconds\": " << jsonNumber(runtime) << ",\n"
      << "  \"finalLength_mm\": " << jsonNumber(dimensions.length) << ",\n"
      << "  \"mushroomDiameter_mm\": " << jsonNumber(dimensions.diameter) << ",\n"
      << "  \"experimentalFinalLength_mm\": " << jsonNumber(config.benchmark.experimentalFinalLength) << ",\n"
      << "  \"experimentalMushroomDiameter_mm\": " << jsonNumber(config.benchmark.experimentalMushroomDiameter) << ",\n"
      << "  \"finalLengthRelativeError\": " << jsonNumber(lengthError) << ",\n"
      << "  \"mushroomDiameterRelativeError\": " << jsonNumber(diameterError) << ",\n"
      << "  \"profileError\": null,\n"
      << "  \"finalEnergyError\": " << jsonNumber(finalEnergyError) << ",\n"
      << "  \"momentumImpulseRelativeError\": " << jsonNumber(momentumError) << ",\n"
      << "  \"maximumPenetration_mm\": " << jsonNumber(maxPenetration) << ",\n"
      << "  \"acceptance\": {\"finite\": " << (finite ? "true" : "false")
      << ", \"momentumImpulse\": "
      << (momentumError <= config.dynamics.momentumImpulseToleranceRelative ? "true" : "false")
      << ", \"energy\": "
      << (finalEnergyError <= config.dynamics.energyToleranceRelative ? "true" : "false")
      << ", \"penetration\": "
      << (maxPenetration <= config.dynamics.penetrationSpacingTolerance * config.geometry.spacing
          ? "true" : "false") << "}\n}\n";

  std::cout << config.name << ": particles=" << block.referencePosition.size()
      << " runtime_s=" << runtime << " final_length=" << dimensions.length
      << " diameter=" << dimensions.diameter << " energy_error=" << finalEnergyError
      << " momentum_error=" << momentumError << " penetration=" << maxPenetration
      << " pass=" << (pass ? "true" : "false") << '\n';
  return pass ? 0 : 2;
}

} // namespace demo
} // namespace vumat
