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
    geometry.length = json::getDouble(*object, "length", false, geometry.length);
    geometry.diameter = json::getDouble(*object, "diameter", false, geometry.diameter);
    geometry.initialGap = json::getDouble(*object, "initialGap", false, geometry.initialGap);
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
  dynamics.probeX = json::getDouble(*object, "probeX", false, dynamics.probeX);
  dynamics.referenceModel =
      json::getString(*object, "referenceModel", false, dynamics.referenceModel);
  dynamics.arrivalThresholdFraction = json::getDouble(
      *object, "arrivalThresholdFraction", false, dynamics.arrivalThresholdFraction);
  dynamics.arrivalToleranceRelative = json::getDouble(
      *object, "arrivalToleranceRelative", false, dynamics.arrivalToleranceRelative);
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
  dynamics.momentumImpulseToleranceRelative = json::getDouble(
      *object, "momentumImpulseToleranceRelative", false,
      dynamics.momentumImpulseToleranceRelative);
  dynamics.penetrationSpacingTolerance = json::getDouble(
      *object, "penetrationSpacingTolerance", false,
      dynamics.penetrationSpacingTolerance);
  return dynamics;
}

std::array<double, 3> loadVec3(const json::Value& object, const std::string& name,
                              const std::array<double, 3>& fallback) {
  const json::Value* value = json::find(object, name);
  if (!value) return fallback;
  if (value->type != json::Value::Type::Array || value->array.size() != 3)
    throw std::runtime_error(name + " must be a three-component numeric array.");
  std::array<double, 3> result{};
  for (size_t i = 0; i < 3; ++i) {
    if (value->array[i].type != json::Value::Type::Number)
      throw std::runtime_error(name + " must contain only numbers.");
    result[i] = value->array[i].number;
  }
  return result;
}

InitialConditionsConfig loadInitialConditions(const json::Value& root) {
  InitialConditionsConfig initial;
  const json::Value* object = json::find(root, "initial_conditions");
  if (object && object->type == json::Value::Type::Object)
    initial.velocity = loadVec3(*object, "velocity", initial.velocity);
  return initial;
}

RigidWallContactConfig loadContact(const json::Value& root) {
  RigidWallContactConfig contact;
  const json::Value* object = json::find(root, "contact");
  if (!object || object->type != json::Value::Type::Object) return contact;
  contact.enabled = json::getBool(*object, "enabled", false, contact.enabled);
  contact.point = loadVec3(*object, "point", contact.point);
  contact.normal = loadVec3(*object, "normal", contact.normal);
  contact.penaltyStiffness =
      json::getDouble(*object, "penaltyStiffness", false, contact.penaltyStiffness);
  contact.frictionCoefficient = json::getDouble(
      *object, "frictionCoefficient", false, contact.frictionCoefficient);
  return contact;
}

BenchmarkConfig loadBenchmark(const json::Value& root) {
  BenchmarkConfig benchmark;
  const json::Value* object = json::find(root, "benchmark");
  if (!object || object->type != json::Value::Type::Object) return benchmark;
  benchmark.name = json::getString(*object, "name", false, "");
  benchmark.source = json::getString(*object, "source", false, "");
  benchmark.citation = json::getString(*object, "citation", false, "");
  benchmark.doi = json::getString(*object, "doi", false, "");
  benchmark.referenceProfilePath =
      json::getString(*object, "referenceProfilePath", false, "");
  benchmark.originalVumatSourceHash =
      json::getString(*object, "originalVumatSourceHash", false, "");
  benchmark.experimentalFinalLength = json::getDouble(
      *object, "experimentalFinalLength", false, 0.0);
  benchmark.experimentalMushroomDiameter = json::getDouble(
      *object, "experimentalMushroomDiameter", false, 0.0);
  return benchmark;
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
  config.initialConditions = loadInitialConditions(root);
  config.contact = loadContact(root);
  config.benchmark = loadBenchmark(root);
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

  if (config.type != "vumat_pd_demo_case" && config.type != "vumat_pd_dynamic_case" &&
      config.type != "vumat_pd_boundary_case" &&
      config.type != "vumat_pd_impact_case" &&
      config.type != "vumat_material_point_case")
    throw std::runtime_error("Unsupported case type: " + config.type);
  if (config.geometry.spacing <= 0.0 || config.geometry.horizon <= 0.0)
    throw std::runtime_error("PD geometry requires positive spacing and horizon.");
  if (config.geometry.type == "patch") {
    if (config.geometry.nx < 2 || config.geometry.ny < 2 || config.geometry.nz < 2)
      throw std::runtime_error("PD patch geometry requires nx/ny/nz >= 2.");
  } else if (config.geometry.type == "cylinder") {
    if (config.geometry.length <= 0.0 || config.geometry.diameter <= 0.0 ||
        config.geometry.initialGap < 0.0)
      throw std::runtime_error("Cylinder geometry requires positive length/diameter and nonnegative gap.");
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
  } else if (config.type == "vumat_pd_dynamic_case") {
    if (config.boundaryCondition.type != "smooth_displacement_ramp_x" ||
        config.boundaryCondition.amplitude == 0.0 ||
        config.boundaryCondition.riseTime <= 0.0)
      throw std::runtime_error("Dynamic wave case requires a nonzero smooth displacement ramp.");
    if (config.dynamics.density <= 0.0 || config.dynamics.outputEvery < 1 ||
        config.dynamics.arrivalThresholdFraction <= 0.0 ||
        config.dynamics.arrivalToleranceRelative <= 0.0 ||
        config.dynamics.energyToleranceRelative <= 0.0 ||
        config.dynamics.netForceTolerance <= 0.0 ||
        config.dynamics.referenceModel != "constrained_p_wave")
      throw std::runtime_error("Invalid dynamics configuration.");
    if (config.materials.size() != 1)
      throw std::runtime_error("The dynamic wave validation currently requires one material.");
    if (config.materials.front().elasticE <= 0.0 ||
        config.materials.front().elasticNu <= -1.0 ||
        config.materials.front().elasticNu >= 0.5)
      throw std::runtime_error("Dynamic wave reference requires valid elastic E and nu.");
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
    // Refinement studies retain plane strain but vary the sampling density
    // through a fixed physical thickness and vary the horizon independently.
    if (config.geometry.nz < 3 ||
        config.geometry.horizon / config.geometry.spacing < 1.5)
      throw std::runtime_error("Plane-strain refinement cases require nz>=3 and horizon/spacing>=1.5.");
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
  } else {
    if (config.geometry.type != "cylinder" ||
        std::abs(config.geometry.horizon / config.geometry.spacing - 3.0) > 1.0e-12)
      throw std::runtime_error("Impact cases require a cylinder and horizon/spacing=3.");
    if (config.materials.size() != 1 || config.dynamics.density <= 0.0 ||
        config.dynamics.outputEvery < 1 || config.dynamics.snapshotEvery < 1 ||
        config.dynamics.energyToleranceRelative <= 0.0 ||
        config.dynamics.momentumImpulseToleranceRelative <= 0.0 ||
        config.dynamics.penetrationSpacingTolerance <= 0.0)
      throw std::runtime_error("Invalid impact dynamics configuration.");
    if (!config.contact.enabled || config.contact.penaltyStiffness <= 0.0 ||
        config.contact.frictionCoefficient < 0.0)
      throw std::runtime_error("Impact cases require enabled rigid-wall contact.");
    if (config.initialConditions.velocity[0] == 0.0 &&
        config.initialConditions.velocity[1] == 0.0 &&
        config.initialConditions.velocity[2] == 0.0)
      throw std::runtime_error("Impact cases require a nonzero uniform initial velocity.");
    if (config.damage.mode != "none" || config.damage.deletionEnabled)
      throw std::runtime_error("Adapter impact verification requires external damage and deletion disabled.");
    if (config.units.length != "mm" || config.units.time != "ms" ||
        config.units.force != "N" || config.units.stress != "MPa" ||
        config.units.massDensity != "N*ms^2/mm^4")
      throw std::runtime_error("Impact cases require the documented mm-ms-N-MPa unit system.");
  }
  if (config.materials.empty())
    throw std::runtime_error("Demo case requires at least one material.");

  return config;
}

} // namespace demo
} // namespace vumat
