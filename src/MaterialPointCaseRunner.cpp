#include "MaterialPointCaseRunner.hpp"

#include "VumatMaterialAdapter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace vumat {
namespace demo {
namespace {

bool finiteTensor(const std::array<double, 9>& values) {
  return std::all_of(values.begin(), values.end(),
                     [](double value) { return std::isfinite(value); });
}

bool finiteState(const std::vector<double>& values) {
  return std::all_of(values.begin(), values.end(),
                     [](double value) { return std::isfinite(value); });
}

double referenceStress(const DemoMaterialConfig& material, double load,
                       const std::string& mode) {
  const double e = material.elasticE;
  const double nu = material.elasticNu;
  if (e <= 0.0)
    return 0.0;
  const double mu = e / (2.0 * (1.0 + nu));
  if (mode == "simple_shear_xy")
    return mu * load;
  if (mode == "plane_strain_x_uniaxial_stress")
    return e / (1.0 - nu * nu) * load;
  const double lambda = e * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
  return (lambda + 2.0 * mu) * load;
}

double indicator(const DemoMaterialConfig& material,
                 const std::vector<double>& state) {
  if (material.internalIndicatorIndices.empty())
    return 0.0;
  double value = material.internalIndicatorReduction == "min"
                     ? std::numeric_limits<double>::infinity()
                     : -std::numeric_limits<double>::infinity();
  for (int index : material.internalIndicatorIndices) {
    const double component = state[static_cast<size_t>(index)];
    value = material.internalIndicatorReduction == "min"
                ? std::min(value, component)
                : std::max(value, component);
  }
  if (material.internalIndicatorTransform == "one_minus")
    value = 1.0 - value;
  return std::max(0.0, value);
}

struct Summary {
  std::string name;
  bool reference = false;
  bool finite = true;
  double maxReferenceError = 0.0;
  double finalStress = 0.0;
  double finalIndicator = 0.0;
  double finalInternalEnergy = 0.0;
  double finalInelasticEnergy = 0.0;
};

Summary runMaterial(const DemoCaseConfig& config,
                    const DemoMaterialConfig& material,
                    std::ofstream& csv) {
  VumatMaterialAdapter adapter(material.material);
  VumatPointInput input;
  input.dt = config.timeIntegration.dt;
  input.charLength = config.geometry.spacing;
  input.temperature = material.referenceTemperature;
  input.stateOld = material.initialState.empty()
                       ? std::vector<double>(static_cast<size_t>(material.material.nstatev), 0.0)
                       : material.initialState;

  Summary summary;
  summary.name = material.label;
  summary.reference = material.reference;
  double previousLateralStrain = 0.0;

  for (int step = 1; step <= config.timeIntegration.steps; ++step) {
    const double previousLoad = config.boundaryCondition.maxStrain *
                                static_cast<double>(step - 1) /
                                static_cast<double>(config.timeIntegration.steps);
    const double load = config.boundaryCondition.maxStrain *
                        static_cast<double>(step) /
                        static_cast<double>(config.timeIntegration.steps);
    const double rate = (load - previousLoad) / input.dt;
    input.stepTime = static_cast<double>(step - 1) * input.dt;
    input.totalTime = input.stepTime;
    input.strainRateFull.fill(0.0);
    input.strainFullN.fill(0.0);
    input.defgradOld = {1.0, 0.0, 0.0,
                        0.0, 1.0, 0.0,
                        0.0, 0.0, 1.0};
    input.defgradNew = input.defgradOld;
    if (config.boundaryCondition.mode == "simple_shear_xy") {
      input.strainRateFull[1] = 0.5 * rate;
      input.strainRateFull[3] = 0.5 * rate;
      input.strainFullN[1] = 0.5 * previousLoad;
      input.strainFullN[3] = 0.5 * previousLoad;
      input.defgradOld[1] = previousLoad;
      input.defgradNew[1] = load;
    } else {
      input.strainRateFull[0] = rate;
      input.strainFullN[0] = previousLoad;
      input.defgradOld[0] = 1.0 + previousLoad;
      input.defgradNew[0] = 1.0 + load;
    }

    VumatPointResult result;
    double lateralStrain = 0.0;
    if (config.boundaryCondition.mode == "plane_strain_x_uniaxial_stress") {
      const auto evaluate = [&](double trialLateral) {
        VumatPointInput trial = input;
        trial.strainRateFull[4] = (trialLateral - previousLateralStrain) / input.dt;
        trial.strainFullN[4] = previousLateralStrain;
        trial.defgradOld[4] = 1.0 + previousLateralStrain;
        trial.defgradNew[4] = 1.0 + trialLateral;
        return adapter.updatePoint(trial);
      };
      double lower = -load;
      double upper = 0.25 * load;
      VumatPointResult lowerResult = evaluate(lower);
      VumatPointResult upperResult = evaluate(upper);
      double lowerResidual = lowerResult.stressFullNew[4];
      double upperResidual = upperResult.stressFullNew[4];
      if (lowerResidual * upperResidual > 0.0)
        throw std::runtime_error("Could not bracket zero lateral stress in material-point reference.");
      for (int iteration = 0; iteration < 36; ++iteration) {
        lateralStrain = 0.5 * (lower + upper);
        result = evaluate(lateralStrain);
        const double residual = result.stressFullNew[4];
        if (std::abs(residual) < 1.0e-8)
          break;
        if (lowerResidual * residual <= 0.0) {
          upper = lateralStrain;
          upperResidual = residual;
        } else {
          lower = lateralStrain;
          lowerResidual = residual;
        }
      }
      previousLateralStrain = lateralStrain;
    } else {
      result = adapter.updatePoint(input);
    }
    const double stress = config.boundaryCondition.mode == "simple_shear_xy"
                              ? 0.5 * (result.stressFullNew[1] + result.stressFullNew[3])
                              : result.stressFullNew[0];
    const double reference = referenceStress(material, load,
                                             config.boundaryCondition.mode);
    const double error = std::abs(stress - reference);
    const double stateIndicator = indicator(material, result.stateNew);
    const bool finite = finiteTensor(result.stressFullNew) &&
                        finiteState(result.stateNew) &&
                        std::isfinite(result.temperatureNew) &&
                        std::isfinite(result.internalEnergyNew) &&
                        std::isfinite(result.inelasticEnergyNew) &&
                        std::isfinite(stateIndicator);
    summary.finite = summary.finite && finite;
    if (material.reference)
      summary.maxReferenceError = std::max(summary.maxReferenceError, error);
    summary.finalStress = stress;
    summary.finalIndicator = stateIndicator;
    summary.finalInternalEnergy = result.internalEnergyNew;
    summary.finalInelasticEnergy = result.inelasticEnergyNew;

    csv << material.label << ',' << step << ',' << step * input.dt << ','
        << load << ',' << stress << ',' << reference << ',' << error << ','
        << stateIndicator << ',' << result.internalEnergyNew << ','
        << result.inelasticEnergyNew << ',' << result.temperatureNew << ','
        << lateralStrain << ',' << (finite ? 1 : 0) << '\n';

    input.stressFullN = result.stressFullNew;
    input.stateOld = result.stateNew;
    input.temperature = result.temperatureNew;
    input.internalEnergyOld = result.internalEnergyNew;
    input.inelasticEnergyOld = result.inelasticEnergyNew;
  }
  return summary;
}

} // namespace

int runMaterialPointCase(const DemoCaseConfig& config) {
  std::filesystem::create_directories(config.output.directory);
  std::ofstream csv(config.output.directory + "/timeseries.csv");
  csv << std::setprecision(16);
  csv << "material,step,time,applied_strain,stress,elastic_reference_stress,"
         "reference_error,state_indicator,internal_energy,inelastic_energy,"
         "temperature,lateral_strain,finite\n";

  std::vector<Summary> summaries;
  for (const auto& material : config.materials)
    summaries.push_back(runMaterial(config, material, csv));

  bool pass = true;
  for (const auto& summary : summaries) {
    pass = pass && summary.finite;
    if (summary.reference)
      pass = pass && summary.maxReferenceError < config.tolerances.referenceStressAbsolute;
  }

  std::ofstream metadata(config.output.directory + "/metadata.json");
  metadata << std::setprecision(16);
  metadata << "{\n  \"name\": \"" << config.name << "\",\n"
           << "  \"status\": \"" << (pass ? "pass" : "fail") << "\",\n"
           << "  \"scope\": \"" << config.scope << "\",\n"
           << "  \"steps\": " << config.timeIntegration.steps << ",\n"
           << "  \"mode\": \"" << config.boundaryCondition.mode << "\",\n"
           << "  \"materials\": [\n";
  for (size_t i = 0; i < summaries.size(); ++i) {
    const auto& s = summaries[i];
    metadata << "    {\"name\": \"" << s.name << "\", \"reference\": "
             << (s.reference ? "true" : "false")
             << ", \"finite\": " << (s.finite ? "true" : "false")
             << ", \"maxReferenceError\": " << s.maxReferenceError
             << ", \"finalStress\": " << s.finalStress
             << ", \"finalIndicator\": " << s.finalIndicator
             << ", \"finalInternalEnergy\": " << s.finalInternalEnergy
             << ", \"finalInelasticEnergy\": " << s.finalInelasticEnergy << "}";
    metadata << (i + 1 == summaries.size() ? "\n" : ",\n");
  }
  metadata << "  ]\n}\n";

  for (const auto& s : summaries)
    std::cout << s.name << ": final_stress=" << s.finalStress
              << " max_reference_error=" << s.maxReferenceError
              << " state_indicator=" << s.finalIndicator
              << " finite=" << (s.finite ? "true" : "false") << '\n';
  std::cout << "Material-point case written to " << config.output.directory << '\n';
  return pass ? 0 : 2;
}

} // namespace demo
} // namespace vumat
