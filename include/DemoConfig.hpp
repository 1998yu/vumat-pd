#pragma once

#include "MaterialConfig.hpp"
#include "SimpleDamageModel.hpp"

#include <array>
#include <string>
#include <vector>

namespace vumat {
namespace demo {

struct GeometryConfig {
  std::string type = "patch";
  int nx = 1;
  int ny = 1;
  int nz = 1;
  double spacing = 1.0;
  double horizon = 1.0;
  double length = 0.0;
  double diameter = 0.0;
  double initialGap = 0.0;
};

struct TimeIntegrationConfig {
  int steps = 1;
  double dt = 1.0e-6;
  double totalTime = 0.0;
};

struct BoundaryConditionConfig {
  std::string type = "affine_displacement";
  std::string mode = "uniaxial_xx";
  double maxStrain = 0.0;
  double amplitude = 0.0;
  double riseTime = 0.0;
  bool transverseConstraint = false;
  std::string supportRegion = "left";
  std::string loadRegion = "right";
  std::string component = "x";
  int regionLayers = 1;
  bool planeStrain = false;
  bool rigidBodySuppression = false;
};

struct DynamicsConfig {
  double density = 0.0;
  double probeX = 0.0;
  std::string referenceModel = "constrained_p_wave";
  double arrivalThresholdFraction = 0.05;
  double arrivalToleranceRelative = 0.25;
  double energyToleranceRelative = 1.0e-3;
  double netForceTolerance = 1.0e-8;
  int outputEvery = 1;
  int snapshotEvery = 0;
  double minimumPlasticStrain = 0.0;
  double kineticToInternalLimit = 1.0;
  double momentumImpulseToleranceRelative = 0.01;
  double penetrationSpacingTolerance = 0.2;
};

struct InitialConditionsConfig {
  std::array<double, 3> velocity{0.0, 0.0, 0.0};
};

struct RigidWallContactConfig {
  bool enabled = false;
  std::array<double, 3> point{0.0, 0.0, 0.0};
  std::array<double, 3> normal{1.0, 0.0, 0.0};
  double penaltyStiffness = 0.0;
  double frictionCoefficient = 0.0;
};

struct BenchmarkConfig {
  std::string name;
  std::string source;
  std::string citation;
  std::string doi;
  std::string referenceProfilePath;
  std::string originalVumatSourceHash;
  double experimentalFinalLength = 0.0;
  double experimentalMushroomDiameter = 0.0;
};

struct UnitSystemConfig {
  std::string length = "mm";
  std::string time = "ms";
  std::string force = "N";
  std::string stress = "MPa";
  std::string massDensity = "N*ms^2/mm^4";
  std::string sourceDensity = "kg/m^3";
};

struct OutputConfig {
  std::string directory;
  bool csv = true;
  bool svg = true;
  bool metadata = true;
};

struct ValidationToleranceConfig {
  double deformationGradientAbsolute = 1.0e-12;
  double referenceStressAbsolute = 1.0e-6;
  double netForceAbsolute = 1.0e-7;
};

struct DemoMaterialConfig {
  std::string label;
  VumatMaterialConfig material;
  bool reference = false;
  double elasticE = 0.0;
  double elasticNu = 0.0;
  double referenceTemperature = 293.0;
  std::string kinematicsMode = "corotational";
  std::vector<double> initialState;
  int equivalentPlasticStrainIndex = -1;
  std::vector<int> internalIndicatorIndices;
  std::string internalIndicatorReduction = "max";
  std::string internalIndicatorTransform = "identity";
};

struct DemoCaseConfig {
  std::string name;
  std::string type;
  std::string scope;
  std::string classification;
  UnitSystemConfig units;
  GeometryConfig geometry;
  TimeIntegrationConfig timeIntegration;
  BoundaryConditionConfig boundaryCondition;
  DynamicsConfig dynamics;
  InitialConditionsConfig initialConditions;
  RigidWallContactConfig contact;
  BenchmarkConfig benchmark;
  SimpleDamageConfig damage;
  OutputConfig output;
  ValidationToleranceConfig tolerances;
  std::vector<DemoMaterialConfig> materials;
  std::vector<std::string> checks;
};

DemoCaseConfig loadDemoCaseConfigFromJson(const std::string& casePath,
                                         const std::string& pluginDirectory = "");

} // namespace demo
} // namespace vumat
