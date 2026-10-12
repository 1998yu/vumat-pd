#include "DemoConfig.hpp"

#include "SimpleJson.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace vumat {
namespace demo {
namespace {

GeometryConfig loadGeometry(const json::Value& root) {
  GeometryConfig geometry;
  const json::Value* object = json::find(root, "geometry");
  if (object && object->type == json::Value::Type::Object) {
    geometry.type = json::getString(*object, "type", false, geometry.type);
    geometry.nx = json::getInt(*object, "nx", false, geometry.nx);
    geometry.ny = json::getInt(*object, "ny", false, geometry.ny);
    geometry.nz = json::getInt(*object, "nz", false, geometry.nz);
    geometry.spacing = json::getDouble(*object, "spacing", false, geometry.spacing);
    geometry.horizon = json::getDouble(*object, "horizon", false, geometry.horizon);
    return geometry;
  }

  const json::Value* grid = json::find(root, "grid");
  if (grid && grid->type == json::Value::Type::Object) {
    geometry.type = json::getString(*grid, "type", false, geometry.type);
    geometry.nx = json::getInt(*grid, "nx", false, geometry.nx);
    geometry.ny = json::getInt(*grid, "ny", false, geometry.ny);
    geometry.nz = json::getInt(*grid, "nz", false, geometry.nz);
    geometry.spacing = json::getDouble(*grid, "spacing", false, geometry.spacing);
    geometry.horizon = json::getDouble(*grid, "horizon", false, geometry.horizon);
  }
  return geometry;
}

TimeIntegrationConfig loadTimeIntegration(const json::Value& root) {
  TimeIntegrationConfig timeIntegration;
  const json::Value* object = json::find(root, "time_integration");
  if (object && object->type == json::Value::Type::Object) {
    timeIntegration.steps = json::getInt(*object, "steps", false, timeIntegration.steps);
    timeIntegration.dt = json::getDouble(*object, "dt", false, timeIntegration.dt);
    timeIntegration.totalTime =
        json::getDouble(*object, "totalTime", false, timeIntegration.totalTime);
    return timeIntegration;
  }

  timeIntegration.steps = json::getInt(root, "steps", false, timeIntegration.steps);
  timeIntegration.dt = json::getDouble(root, "dt", false, timeIntegration.dt);
  timeIntegration.totalTime = json::getDouble(root, "totalTime", false, timeIntegration.totalTime);
  return timeIntegration;
}

BoundaryConditionConfig loadBoundaryCondition(const json::Value& root) {
  BoundaryConditionConfig bc;
  const json::Value* object = json::find(root, "boundary_conditions");
  if (object && object->type == json::Value::Type::Object) {
    bc.type = json::getString(*object, "type", false, bc.type);
    bc.mode = json::getString(*object, "mode", false, bc.mode);
    bc.maxStrain = json::getDouble(*object, "maxStrain", false, bc.maxStrain);
    bc.amplitude = json::getDouble(*object, "amplitude", false, bc.amplitude);
    bc.riseTime = json::getDouble(*object, "riseTime", false, bc.riseTime);
    bc.transverseConstraint =
        json::getBool(*object, "transverseConstraint", false, bc.transverseConstraint);
    bc.supportRegion = json::getString(*object, "supportRegion", false, bc.supportRegion);
    bc.loadRegion = json::getString(*object, "loadRegion", false, bc.loadRegion);
    bc.component = json::getString(*object, "component", false, bc.component);
    bc.regionLayers = json::getInt(*object, "regionLayers", false, bc.regionLayers);
    bc.planeStrain = json::getBool(*object, "planeStrain", false, bc.planeStrain);
    bc.rigidBodySuppression =
        json::getBool(*object, "rigidBodySuppression", false, bc.rigidBodySuppression);
    return bc;
  }

  bc.maxStrain = json::getDouble(root, "maxStrain", false, bc.maxStrain);
  return bc;
}

DynamicsConfig loadDynamics(const json::Value& root) {
  DynamicsConfig dynamics;
  const json::Value* object = json::find(root, "dynamics");
  if (!object || object->type != json::Value::Type::Object)
    return dynamics;
  dynamics.density = json::getDouble(*object, "density", false, dynamics.density);
  dynamics.energyToleranceRelative = json::getDouble(
      *object, "energyToleranceRelative", false, dynamics.energyToleranceRelative);
  dynamics.netForceTolerance = json::getDouble(
      *object, "netForceTolerance", false, dynamics.netForceTolerance);
  dynamics.outputEvery =
      json::getInt(*object, "outputEvery", false, dynamics.outputEvery);
  dynamics.snapshotEvery =
      json::getInt(*object, "snapshotEvery", false, dynamics.snapshotEvery);
  dynamics.minimumPlasticStrain = json::getDouble(
      *object, "minimumPlasticStrain", false, dynamics.minimumPlasticStrain);
  dynamics.kineticToInternalLimit = json::getDouble(
      *object, "kineticToInternalLimit", false, dynamics.kineticToInternalLimit);
  return dynamics;
}

UnitSystemConfig loadUnits(const json::Value& root) {
  UnitSystemConfig units;
  const json::Value* object = json::find(root, "units");
  if (!object || object->type != json::Value::Type::Object)
    return units;
  units.length = json::getString(*object, "length", false, units.length);
  units.time = json::getString(*object, "time", false, units.time);
  units.force = json::getString(*object, "force", false, units.force);
  units.stress = json::getString(*object, "stress", false, units.stress);
  units.massDensity =
      json::getString(*object, "massDensity", false, units.massDensity);
  units.sourceDensity =
      json::getString(*object, "sourceDensity", false, units.sourceDensity);
  return units;
}

SimpleDamageConfig loadDamage(const json::Value& root) {
  SimpleDamageConfig damage;
  const json::Value* object = json::find(root, "damage");
  if (!object || object->type != json::Value::Type::Object)
    return damage;

  const std::string model = json::getString(*object, "model", false, "");
  if (model == "critical_stretch" || model == "Critical Stretch")
    damage.mode = "critical_stretch";
  else if (!model.empty())
    damage.mode = model;

  damage.initiation = json::getDouble(*object, "initiation", false, damage.initiation);
  if (damage.initiation <= 0.0)
    damage.initiation = json::getDouble(*object, "criticalStretch", false, damage.initiation);
  damage.failure = json::getDouble(*object, "failure", false, damage.failure);
  damage.deletionEnabled =
      json::getBool(*object, "deletionEnabled", false, damage.deletionEnabled);
  return damage;
}

OutputConfig loadOutput(const json::Value& root, const std::string& defaultName) {
  OutputConfig output;
  output.directory = "validation_cases/output/" + defaultName;
  const json::Value* object = json::find(root, "output");
  if (!object || object->type != json::Value::Type::Object)
    return output;
  output.directory = json::getString(*object, "directory", false, output.directory);
  output.csv = json::getBool(*object, "csv", false, output.csv);
  output.svg = json::getBool(*object, "svg", false, output.svg);
  output.metadata = json::getBool(*object, "metadata", false, output.metadata);
  return output;
}

std::vector<std::string> loadChecks(const json::Value& root) {
  std::vector<std::string> checks;
  const json::Value* object = json::find(root, "checks");
  if (!object || object->type != json::Value::Type::Array)
    return checks;
  for (const auto& entry : object->array) {
    if (entry.type == json::Value::Type::String)
      checks.push_back(entry.string);
  }
  return checks;
}

ValidationToleranceConfig loadTolerances(const json::Value& root) {
  ValidationToleranceConfig tolerances;
  const json::Value* object = json::find(root, "tolerances");
  if (!object || object->type != json::Value::Type::Object)
    return tolerances;
  tolerances.deformationGradientAbsolute = json::getDouble(
      *object, "deformationGradientAbsolute", false,
      tolerances.deformationGradientAbsolute);
  tolerances.referenceStressAbsolute = json::getDouble(
      *object, "referenceStressAbsolute", false,
      tolerances.referenceStressAbsolute);
  tolerances.netForceAbsolute = json::getDouble(
      *object, "netForceAbsolute", false, tolerances.netForceAbsolute);
  return tolerances;
}

std::vector<double> loadOptionalDoubleArray(const json::Value& object,
                                           const std::string& name) {
  const json::Value* value = json::find(object, name);
  if (!value || value->type != json::Value::Type::Array)
    return {};
  std::vector<double> result;
  result.reserve(value->array.size());
  for (const auto& entry : value->array) {
    if (entry.type == json::Value::Type::Number)
      result.push_back(entry.number);
  }
  return result;
}

std::vector<int> loadOptionalIntArray(const json::Value& object, const std::string& name) {
  const json::Value* value = json::find(object, name);
  if (!value || value->type != json::Value::Type::Array)
    return {};
  std::vector<int> result;
  for (const auto& entry : value->array)
    if (entry.type == json::Value::Type::Number)
      result.push_back(static_cast<int>(entry.number));
  return result;
}

DemoMaterialConfig loadMaterial(const json::Value& entry, const std::string& pluginDirectory) {
  DemoMaterialConfig material;
  material.label = json::getString(entry, "name");

  const std::string configPath =
      json::getString(entry, "materialConfigPath", false,
                      json::getString(entry, "configPath", false,
                                      json::getString(entry, "material_config_path", false, "")));
  if (configPath.empty())
    throw std::runtime_error("Material entry requires materialConfigPath or configPath.");

  material.material = loadMaterialConfigFromJson(configPath, pluginDirectory);
  material.reference = json::getBool(entry, "reference", false, false);
  material.elasticE = json::getDouble(entry, "elasticE", false, 0.0);
  material.elasticNu = json::getDouble(entry, "elasticNu", false, 0.0);
  material.referenceTemperature =
      json::getDouble(entry, "referenceTemperature", false, material.referenceTemperature);
  material.kinematicsMode =
      json::getString(entry, "kinematicsMode", false, material.kinematicsMode);
  if (material.kinematicsMode != "corotational" &&
      material.kinematicsMode != "legacy_small_strain")
    throw std::runtime_error("Unsupported kinematicsMode for material: " +
                             material.label);
  material.initialState = loadOptionalDoubleArray(entry, "initialState");
  material.equivalentPlasticStrainIndex =
      json::getInt(entry, "equivalentPlasticStrainIndex", false, -1);
  if (material.equivalentPlasticStrainIndex >= material.material.nstatev)
    throw std::runtime_error("equivalentPlasticStrainIndex is outside nstatev for material: " +
                             material.label);
  const json::Value* overrides = json::find(entry, "initialStateOverrides");
  if (overrides && overrides->type == json::Value::Type::Object) {
    if (material.initialState.empty())
      material.initialState.assign(static_cast<size_t>(material.material.nstatev), 0.0);
    for (const auto& item : overrides->object) {
      const int index = std::stoi(item.first);
      if (index < 0 || index >= material.material.nstatev ||
          item.second.type != json::Value::Type::Number)
        throw std::runtime_error("Invalid initialStateOverrides entry for material: " +
                                 material.label);
      material.initialState[static_cast<size_t>(index)] = item.second.number;
    }
  }
  if (!material.initialState.empty() &&
      material.initialState.size() != static_cast<size_t>(material.material.nstatev))
    throw std::runtime_error("initialState length does not match nstatev for material: " +
                             material.label);
  const json::Value* indicator = json::find(entry, "internalIndicator");
  if (indicator && indicator->type == json::Value::Type::Object) {
    material.internalIndicatorIndices = loadOptionalIntArray(*indicator, "indices");
    material.internalIndicatorReduction =
        json::getString(*indicator, "reduction", false, "max");
    material.internalIndicatorTransform =
        json::getString(*indicator, "transform", false, "identity");
    for (int index : material.internalIndicatorIndices)
      if (index < 0 || index >= material.material.nstatev)
        throw std::runtime_error("internalIndicator index is outside nstatev for material: " +
                                 material.label);
  }
  return material;
}

} // namespace

DemoCaseConfig loadDemoCaseConfigFromJson(const std::string& casePath,
                                         const std::string& pluginDirectory) {
  const json::Value root = json::loadFile(casePath);
  const std::string schemaVersion = json::getString(root, "schemaVersion", false, "1.0");
  if (schemaVersion != "1.0")
    throw std::runtime_error("Unsupported VUMAT-PD case schemaVersion: " + schemaVersion);

  DemoCaseConfig config;
  config.name = json::getString(root, "name");
  config.type = json::getString(root, "type", false, "vumat_pd_demo");
  config.scope = json::getString(root, "scope", false, "");
  config.classification = json::getString(root, "classification", false, "verification");
  config.units = loadUnits(root);
  config.geometry = loadGeometry(root);
  config.timeIntegration = loadTimeIntegration(root);
  config.boundaryCondition = loadBoundaryCondition(root);
  config.dynamics = loadDynamics(root);
  config.damage = loadDamage(root);
  config.output = loadOutput(root, config.name);
  config.tolerances = loadTolerances(root);
  config.checks = loadChecks(root);

  const json::Value* materials = json::find(root, "materials");
  if (!materials || materials->type != json::Value::Type::Array)
    throw std::runtime_error("Demo case requires a materials array.");

  for (const auto& entry : materials->array) {
    if (entry.type != json::Value::Type::Object)
      throw std::runtime_error("Each materials entry must be a JSON object.");
    config.materials.push_back(loadMaterial(entry, pluginDirectory));
  }

  if (config.timeIntegration.totalTime <= 0.0)
    config.timeIntegration.totalTime =
        config.timeIntegration.dt * static_cast<double>(config.timeIntegration.steps);

  if (config.type != "vumat_pd_demo_case" &&
      config.type != "vumat_pd_boundary_case" &&
      config.type != "vumat_material_point_case")
    throw std::runtime_error("Unsupported case type: " + config.type);
  if (config.geometry.spacing <= 0.0 || config.geometry.horizon <= 0.0)
    throw std::runtime_error("PD geometry requires positive spacing and horizon.");
  if (config.geometry.type == "patch") {
    if (config.geometry.nx < 2 || config.geometry.ny < 2 || config.geometry.nz < 2)
      throw std::runtime_error("PD patch geometry requires nx/ny/nz >= 2.");
  } else {
    throw std::runtime_error("Unsupported geometry type: " + config.geometry.type);
  }
  if (config.timeIntegration.steps < 1 || config.timeIntegration.dt <= 0.0)
    throw std::runtime_error("time_integration requires steps >= 1 and dt > 0.");
  const double expectedTotal = config.timeIntegration.dt * config.timeIntegration.steps;
  if (std::abs(config.timeIntegration.totalTime - expectedTotal) >
      1.0e-12 * std::max(1.0, expectedTotal))
    throw std::runtime_error("time_integration.totalTime must equal steps * dt.");
  if (config.type == "vumat_pd_demo_case" || config.type == "vumat_material_point_case") {
    if (config.boundaryCondition.type != "affine_displacement" ||
        (config.boundaryCondition.mode != "uniaxial_xx" &&
         config.boundaryCondition.mode != "simple_shear_xy" &&
         config.boundaryCondition.mode != "plane_strain_x_uniaxial_stress"))
      throw std::runtime_error("Unsupported boundary_conditions type or mode.");
  } else if (config.type == "vumat_pd_boundary_case") {
    if (config.boundaryCondition.type != "regional_smooth_displacement" ||
        config.boundaryCondition.amplitude == 0.0 ||
        config.boundaryCondition.riseTime <= 0.0 ||
        config.boundaryCondition.regionLayers < 1 ||
        !config.boundaryCondition.rigidBodySuppression)
      throw std::runtime_error("Boundary-value case requires regional smooth loading and rigid-body suppression.");
    const bool xCase = config.boundaryCondition.component == "x" &&
        config.boundaryCondition.supportRegion == "left" &&
        config.boundaryCondition.loadRegion == "right";
    const bool yCase = config.boundaryCondition.component == "y" &&
        config.boundaryCondition.supportRegion == "bottom" &&
        config.boundaryCondition.loadRegion == "top";
    if (!xCase && !yCase)
      throw std::runtime_error("Regional boundary case supports left/right-x or bottom/top-y loading.");
    // Boundary cases retain thickness sampling while varying grid and horizon.
    if (config.geometry.nz < 3 ||
        config.geometry.horizon / config.geometry.spacing < 1.5)
      throw std::runtime_error("PD boundary cases require nz>=3 and horizon/spacing>=1.5.");
    if (config.dynamics.density <= 0.0 || config.dynamics.outputEvery < 1 ||
        config.dynamics.snapshotEvery < 1 ||
        config.dynamics.energyToleranceRelative <= 0.0 ||
        config.dynamics.netForceTolerance <= 0.0 ||
        config.dynamics.kineticToInternalLimit <= 0.0)
      throw std::runtime_error("Invalid boundary-value dynamics configuration.");
    if (config.units.length != "mm" || config.units.time != "ms" ||
        config.units.force != "N" || config.units.stress != "MPa" ||
        config.units.massDensity != "N*ms^2/mm^4")
      throw std::runtime_error("Boundary-value cases currently require the documented mm-ms-N-MPa unit system.");
  }
  if (config.materials.empty())
    throw std::runtime_error("Demo case requires at least one material.");

  return config;
}

} // namespace demo
} // namespace vumat
