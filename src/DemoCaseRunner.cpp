#include "BoundaryConditions.hpp"
#include "BoundaryValueCaseRunner.hpp"
#include "DemoCaseRunner.hpp"
#include "MaterialPointCaseRunner.hpp"
#include "DemoConfig.hpp"
#include "Geometry.hpp"
#include "PdCorrespondenceSkeleton.hpp"
#include "VumatPdCorrespondenceMaterial.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vumat {
namespace demo {
namespace {

struct ForceSummary {
  vumat::pd::Vec3 net{0.0, 0.0, 0.0};
  double leftReactionX = 0.0;
  double rightReactionX = 0.0;
};

struct MaterialRunSummary {
  std::string key;
  bool reference = false;
  double maxFError = 0.0;
  double maxStressReferenceError = 0.0;
  double maxNetForceNorm = 0.0;
  bool finite = true;
};

enum class KinematicsMode {
  UniaxialXx,
  SimpleShearXy,
};

double elasticUniaxialStrainStressXx(double youngModulus, double poissonRatio,
                                     double strainXx) {
  const double lambda =
      youngModulus * poissonRatio / ((1.0 + poissonRatio) * (1.0 - 2.0 * poissonRatio));
  const double mu = youngModulus / (2.0 * (1.0 + poissonRatio));
  return (lambda + 2.0 * mu) * strainXx;
}

double elasticShearStressXy(double youngModulus, double poissonRatio, double gammaXy) {
  const double mu = youngModulus / (2.0 * (1.0 + poissonRatio));
  return mu * gammaXy;
}

bool tensorFinite(const vumat::pd::Tensor3& tensor) {
  for (double value : tensor)
    if (!std::isfinite(value))
      return false;
  return true;
}

bool vectorFinite(const vumat::pd::Vec3& vector) {
  for (double value : vector)
    if (!std::isfinite(value))
      return false;
  return true;
}

bool stateFinite(const std::vector<std::vector<double>>& state) {
  for (const auto& pointState : state)
    for (double value : pointState)
      if (!std::isfinite(value))
        return false;
  return true;
}

double vectorNorm(const vumat::pd::Vec3& vector) {
  return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] +
                   vector[2] * vector[2]);
}

std::string joinChecks(const std::vector<std::string>& checks) {
  std::ostringstream out;
  for (size_t i = 0; i < checks.size(); ++i) {
    if (i > 0)
      out << "; ";
    out << checks[i];
  }
  return out.str();
}

std::string jsonEscape(const std::string& text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (char ch : text) {
    switch (ch) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped += ch;
      break;
    }
  }
  return escaped;
}

KinematicsMode parseKinematicsMode(const std::string& mode) {
  if (mode == "simple_shear_xy")
    return KinematicsMode::SimpleShearXy;
  return KinematicsMode::UniaxialXx;
}

vumat::pd::Tensor3 exactDeformationGradient(double strainXx) {
  return {1.0 + strainXx, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 1.0};
}

vumat::pd::Tensor3 exactDeformationGradient(double loadValue, KinematicsMode mode) {
  if (mode == KinematicsMode::SimpleShearXy) {
    return {1.0, loadValue, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0};
  }
  return {1.0 + loadValue, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 1.0};
}

double referenceStressXx(double youngModulus, double poissonRatio, double loadValue,
                         KinematicsMode mode) {
  if (mode == KinematicsMode::SimpleShearXy)
    return 0.0;
  return elasticUniaxialStrainStressXx(youngModulus, poissonRatio, loadValue);
}

double referenceStressXy(double youngModulus, double poissonRatio, double loadValue,
                         KinematicsMode mode) {
  if (mode == KinematicsMode::SimpleShearXy)
    return elasticShearStressXy(youngModulus, poissonRatio, loadValue);
  return 0.0;
}

double maxTensorError(const std::vector<vumat::pd::Tensor3>& tensors,
                      const vumat::pd::Tensor3& reference) {
  double maxError = 0.0;
  for (const auto& tensor : tensors)
    for (size_t i = 0; i < tensor.size(); ++i)
      maxError = std::max(maxError, std::abs(tensor[i] - reference[i]));
  return maxError;
}

std::vector<double> makeInitialState(const DemoMaterialConfig& material) {
  if (!material.initialState.empty())
    return material.initialState;
  return std::vector<double>(static_cast<size_t>(material.material.nstatev), 0.0);
}

double internalDamageIndicator(const DemoMaterialConfig& material,
                               const std::vector<double>& state) {
  if (material.internalIndicatorIndices.empty())
    return 0.0;
  double value = material.internalIndicatorReduction == "min"
                     ? std::numeric_limits<double>::infinity()
                     : -std::numeric_limits<double>::infinity();
  for (int index : material.internalIndicatorIndices) {
    const double component = state[static_cast<size_t>(index)];
    if (material.internalIndicatorReduction == "min")
      value = std::min(value, component);
    else
      value = std::max(value, component);
  }
  if (material.internalIndicatorTransform == "one_minus")
    value = 1.0 - value;
  return std::max(0.0, value);
}

double averageInternalDamageIndicator(const DemoMaterialConfig& material,
                                      const vumat::pd::MaterialState& state) {
  if (state.vumatStateN.empty())
    return 0.0;
  double sum = 0.0;
  for (const auto& pointState : state.vumatStateN)
    sum += internalDamageIndicator(material, pointState);
  return sum / static_cast<double>(state.vumatStateN.size());
}

double maxInternalDamageIndicator(const DemoMaterialConfig& material,
                                  const vumat::pd::MaterialState& state) {
  double maxValue = 0.0;
  for (const auto& pointState : state.vumatStateN)
    maxValue = std::max(maxValue, internalDamageIndicator(material, pointState));
  return maxValue;
}

ForceSummary summarizeForces(const vumat::pd::ParticleBlock& block,
                             const std::vector<vumat::pd::Vec3>& forceDensity) {
  ForceSummary summary;
  double minX = std::numeric_limits<double>::max();
  double maxX = -std::numeric_limits<double>::max();
  for (const auto& x : block.referencePosition) {
    minX = std::min(minX, x[0]);
    maxX = std::max(maxX, x[0]);
  }

  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    for (int d = 0; d < 3; ++d)
      summary.net[static_cast<size_t>(d)] += forceDensity[i][static_cast<size_t>(d)] *
                                             block.volume[i];
    if (std::abs(block.referencePosition[i][0] - minX) < 1.0e-12)
      summary.leftReactionX += forceDensity[i][0] * block.volume[i];
    if (std::abs(block.referencePosition[i][0] - maxX) < 1.0e-12)
      summary.rightReactionX += forceDensity[i][0] * block.volume[i];
  }
  return summary;
}

void writeMetadata(const DemoCaseConfig& config, const std::vector<MaterialRunSummary>& summaries,
                   int particleCount, int steps) {
  double maxFError = 0.0;
  double maxReferenceError = 0.0;
  double maxNetForceNorm = 0.0;
  bool finite = true;
  for (const auto& summary : summaries) {
    maxFError = std::max(maxFError, summary.maxFError);
    if (summary.reference)
      maxReferenceError = std::max(maxReferenceError, summary.maxStressReferenceError);
    maxNetForceNorm = std::max(maxNetForceNorm, summary.maxNetForceNorm);
    finite = finite && summary.finite;
  }

  const bool pass = finite &&
                    maxFError < config.tolerances.deformationGradientAbsolute &&
                    (summaries.empty() ||
                     maxReferenceError < config.tolerances.referenceStressAbsolute) &&
                    maxNetForceNorm < config.tolerances.netForceAbsolute;

  std::ofstream out(std::filesystem::path(config.output.directory) / "metadata.json");
  out << "{\n";
  out << "  \"name\": \"" << config.name << "\",\n";
  out << "  \"status\": \"" << (pass ? "pass" : "fail") << "\",\n";
  out << "  \"scope\": \"" << config.scope << "\",\n";
  out << "  \"particles\": " << particleCount << ",\n";
  out << "  \"steps\": " << steps << ",\n";
  out << "  \"maxDeformationGradientError\": " << std::setprecision(16) << maxFError << ",\n";
  out << "  \"maxReferenceStressError\": " << std::setprecision(16) << maxReferenceError << ",\n";
  out << "  \"maxJcRawStressError\": " << std::setprecision(16) << maxReferenceError << ",\n";
  out << "  \"maxNetForceNorm\": " << std::setprecision(16) << maxNetForceNorm << ",\n";
  out << "  \"maxDamage\": " << 0.0 << ",\n";
  out << "  \"allValuesFinite\": " << (finite ? "true" : "false") << ",\n";
  out << "  \"tolerances\": {\"deformationGradientAbsolute\": "
      << config.tolerances.deformationGradientAbsolute
      << ", \"referenceStressAbsolute\": "
      << config.tolerances.referenceStressAbsolute
      << ", \"netForceAbsolute\": " << config.tolerances.netForceAbsolute << "},\n";
  std::string notes = config.scope;
  if (!config.checks.empty())
    notes += ". Checks: " + joinChecks(config.checks);
  out << "  \"notes\": \"" << jsonEscape(notes) << "\",\n";
  out << "  \"checks\": [\n";
  for (size_t i = 0; i < config.checks.size(); ++i) {
    out << "    \"" << config.checks[i] << "\"";
    out << (i + 1 == config.checks.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"materials\": [\n";
  for (size_t i = 0; i < summaries.size(); ++i) {
    const auto& summary = summaries[i];
    out << "    {\"name\": \"" << summary.key
        << "\", \"reference\": " << (summary.reference ? "true" : "false")
        << ", \"maxDeformationGradientError\": " << std::setprecision(16)
        << summary.maxFError
        << ", \"maxStressReferenceError\": " << std::setprecision(16)
        << summary.maxStressReferenceError
        << ", \"maxNetForceNorm\": " << std::setprecision(16)
        << summary.maxNetForceNorm
        << ", \"maxDamage\": " << 0.0
        << ", \"finite\": " << (summary.finite ? "true" : "false") << "}";
    out << (i + 1 == summaries.size() ? "\n" : ",\n");
  }
  out << "  ]\n";
  out << "}\n";
}

MaterialRunSummary runMaterial(const DemoCaseConfig& config,
                               const DemoMaterialConfig& material,
                               std::ofstream& timeOut,
                               std::ofstream& fieldOut,
                               int steps,
                               double dt,
                               const BoundaryCondition& boundaryCondition,
                               const BoundaryConditionConfig& boundaryConditionConfig) {
  const KinematicsMode mode = parseKinematicsMode(boundaryConditionConfig.mode);

  vumat::pd::VumatPdCorrespondenceConfig pdConfig;
  pdConfig.material = material.material;
  pdConfig.charLength = config.geometry.spacing;
  pdConfig.referenceTemperature = material.referenceTemperature;
  pdConfig.initialState = makeInitialState(material);
  pdConfig.kinematicsMode = material.kinematicsMode == "legacy_small_strain"
      ? vumat::pd::KinematicsMode::LegacySmallStrain
      : vumat::pd::KinematicsMode::Corotational;

  auto block = makeParticlePatch(config.geometry.nx, config.geometry.ny, config.geometry.nz,
                                 config.geometry.spacing, config.geometry.horizon);
  vumat::pd::PdCorrespondenceSkeleton skeleton(config.geometry.horizon);
  vumat::pd::CorrespondenceState correspondenceState;
  skeleton.initialize(block, correspondenceState);
  vumat::pd::VumatPdCorrespondenceMaterial materialBridge(pdConfig);
  vumat::pd::MaterialState materialState;
  materialBridge.initialize(skeleton.computeDeformationGradients(block, correspondenceState),
                            materialState);

  MaterialRunSummary summary;
  summary.key = material.label;
  summary.reference = material.reference;
  vumat::pd::MaterialUpdateResult finalResult;
  std::vector<vumat::pd::Vec3> finalForceDensity;

  for (int step = 1; step <= steps; ++step) {
    boundaryCondition.apply(block, step, steps);
    const double loadValue = boundaryCondition.loadValue(step, steps);
    const vumat::pd::Tensor3 referenceDeformationGradient =
        exactDeformationGradient(loadValue, mode);
    const double referenceStressXxValue =
        referenceStressXx(material.elasticE, material.elasticNu, loadValue, mode);
    const double referenceStressXyValue =
        referenceStressXy(material.elasticE, material.elasticNu, loadValue, mode);

    const auto deformationGradient =
        skeleton.computeDeformationGradients(block, correspondenceState);
    auto result = materialBridge.update(deformationGradient, materialState, dt,
                                        static_cast<double>(step - 1) * dt,
                                        static_cast<double>(step - 1) * dt);
    const auto forceDensity = skeleton.computeForceDensity(
        block, correspondenceState, result.deformationGradient,
        result.rawStress);
    const double maxFError =
        maxTensorError(result.deformationGradient, referenceDeformationGradient);

    double rawStressSum = 0.0;
    double rawStressXySum = 0.0;
    double maxStressReferenceError = 0.0;
    for (size_t i = 0; i < block.referencePosition.size(); ++i) {
      summary.finite = summary.finite && tensorFinite(result.rawStress[i]) &&
                       vectorFinite(forceDensity[i]);
      rawStressSum += result.rawStress[i][0];
      const double stressXy = 0.5 * (result.rawStress[i][1] + result.rawStress[i][3]);
      rawStressXySum += stressXy;
      const double stressErrorXx = std::abs(result.rawStress[i][0] - referenceStressXxValue);
      const double stressErrorXy = std::abs(stressXy - referenceStressXyValue);
      maxStressReferenceError = std::max(maxStressReferenceError,
                                         std::max(stressErrorXx, stressErrorXy));
    }

    const double pointCount = static_cast<double>(block.referencePosition.size());
    const ForceSummary forceSummary = summarizeForces(block, forceDensity);
    const double netForceNorm = vectorNorm(forceSummary.net);
    const double avgInternalIndicator = averageInternalDamageIndicator(material, materialState);
    const double maxInternalIndicator = maxInternalDamageIndicator(material, materialState);

    summary.maxFError = std::max(summary.maxFError, maxFError);
    summary.maxStressReferenceError =
        std::max(summary.maxStressReferenceError, maxStressReferenceError);
    summary.maxNetForceNorm = std::max(summary.maxNetForceNorm, netForceNorm);
    summary.finite = summary.finite && vectorFinite(forceSummary.net) &&
                     stateFinite(materialState.vumatStateN) &&
                     std::isfinite(avgInternalIndicator) &&
                     std::isfinite(maxInternalIndicator) &&
                     std::isfinite(forceSummary.leftReactionX) &&
                     std::isfinite(forceSummary.rightReactionX);

    timeOut << material.label << ','
            << step << ','
            << step * dt << ','
            << loadValue << ','
            << maxFError << ','
            << rawStressSum / pointCount << ','
            << rawStressSum / pointCount << ','
            << rawStressXySum / pointCount << ','
            << rawStressXySum / pointCount << ','
            << referenceStressXxValue << ','
            << referenceStressXyValue << ','
            << maxStressReferenceError << ','
            << 0.0 << ','
            << 0.0 << ','
            << forceSummary.net[0] << ','
            << forceSummary.net[1] << ','
            << forceSummary.net[2] << ','
            << netForceNorm << ','
            << forceSummary.leftReactionX << ','
            << forceSummary.rightReactionX << ','
            << avgInternalIndicator << ','
            << maxInternalIndicator << ','
            << (summary.finite ? 1 : 0) << '\n';

    finalResult = std::move(result);
    finalForceDensity = forceDensity;
  }

  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const auto& x0 = block.referencePosition[i];
    const auto& u = block.displacement[i];
    const auto current = vumat::pd::Vec3{x0[0] + u[0], x0[1] + u[1], x0[2] + u[2]};
    const auto& f = finalResult.deformationGradient[i];
    fieldOut << material.label << ','
             << i << ','
             << x0[0] << ',' << x0[1] << ',' << x0[2] << ','
             << u[0] << ',' << u[1] << ',' << u[2] << ','
             << current[0] << ',' << current[1] << ',' << current[2] << ',';
    for (double value : f)
      fieldOut << value << ',';
    fieldOut << finalResult.strain[i][0] << ','
             << finalResult.rawStress[i][0] << ','
             << finalResult.rawStress[i][0] << ','
             << 0.0 << ','
             << internalDamageIndicator(material, materialState.vumatStateN[i]) << ','
             << finalForceDensity[i][0] << ','
             << finalForceDensity[i][1] << ','
             << finalForceDensity[i][2] << '\n';
  }

  return summary;
}

} // namespace

int runDemoCaseFromJson(const std::string& casePath, const std::string& pluginDirectory) {
  try {
    DemoCaseConfig config = loadDemoCaseConfigFromJson(casePath, pluginDirectory);
    if (config.type == "vumat_material_point_case")
      return runMaterialPointCase(config);
    if (config.type == "vumat_pd_boundary_case")
      return runBoundaryValueCase(config);

    const std::string outputRoot = config.output.directory;
    std::filesystem::create_directories(outputRoot);

    std::ofstream timeOut(std::filesystem::path(outputRoot) / "timeseries.csv");
    timeOut << "material,step,time,applied_strain,max_f_error,avg_raw_stress_xx,"
            << "avg_effective_stress_xx,avg_raw_stress_xy,avg_effective_stress_xy,"
            << "elastic_reference_stress_xx,elastic_reference_stress_xy,"
            << "max_stress_reference_error,max_damage,avg_damage,net_force_x,"
            << "net_force_y,net_force_z,net_force_norm,left_reaction_x,"
            << "right_reaction_x,avg_internal_indicator,max_internal_indicator,finite\n";

    std::ofstream fieldOut(std::filesystem::path(outputRoot) / "field_final.csv");
    fieldOut << "material,particle,x0,y0,z0,ux,uy,uz,x,y,z,"
             << "Fxx,Fxy,Fxz,Fyx,Fyy,Fyz,Fzx,Fzy,Fzz,"
             << "strain_xx,raw_stress_xx,effective_stress_xx,damage,"
             << "internal_indicator,force_x,force_y,force_z\n";

    auto boundaryCondition = createBoundaryCondition(config.boundaryCondition);

    std::vector<MaterialRunSummary> summaries;
    for (const auto& material : config.materials) {
      summaries.push_back(runMaterial(config, material, timeOut, fieldOut,
                                      config.timeIntegration.steps, config.timeIntegration.dt,
                                      *boundaryCondition, config.boundaryCondition));
    }

    writeMetadata(config, summaries,
                  config.geometry.nx * config.geometry.ny * config.geometry.nz,
                  config.timeIntegration.steps);

    bool pass = true;
    for (const auto& summary : summaries) {
      pass = pass && summary.finite &&
             summary.maxFError < config.tolerances.deformationGradientAbsolute &&
             summary.maxNetForceNorm < config.tolerances.netForceAbsolute;
      if (summary.reference)
        pass = pass &&
               summary.maxStressReferenceError < config.tolerances.referenceStressAbsolute;
      std::cout << summary.key
                << ": max_f_error=" << summary.maxFError
                << " max_stress_reference_error=" << summary.maxStressReferenceError
                << " max_damage=" << 0.0
                << " max_net_force_norm=" << summary.maxNetForceNorm
                << " finite=" << (summary.finite ? "true" : "false") << "\n";
    }

    std::cout << "VUMAT-PD demo case written to " << outputRoot << "\n";
    return pass ? 0 : 2;
  } catch (const std::exception& error) {
    std::cerr << "VUMAT-PD demo case failed: " << error.what() << "\n";
    return 1;
  }
}

} // namespace demo
} // namespace vumat
