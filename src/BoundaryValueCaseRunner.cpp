#include "BoundaryValueCaseRunner.hpp"

#include "BoundaryConditions.hpp"
#include "ExplicitTimeIntegrator.hpp"
#include "Geometry.hpp"
#include "ParticleSets.hpp"
#include "PdCorrespondenceSkeleton.hpp"
#include "PdSimpleDamage.hpp"
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
#include <string>
#include <vector>

namespace vumat {
namespace demo {
namespace {

struct RunSummary {
  std::string material;
  bool pass = false;
  bool finite = true;
  double runtimeSeconds = 0.0;
  double maxNetInternalForce = 0.0;
  double finalEnergyError = 0.0;
  double maxEnergyError = 0.0;
  double maxKineticToInternal = 0.0;
  double finalKineticToInternal = 0.0;
  double maxEqps = 0.0;
  double maxInternalIndicator = 0.0;
  double earlyElasticSlope = 0.0;
  double analyticalPlaneStrainSlope = 0.0;
  double earlyElasticRelativeError = 0.0;
  double finalReaction = 0.0;
  double finalNominalStress = 0.0;
  double finalEngineeringStrain = 0.0;
};

std::string jsonNumber(double value) {
  if (!std::isfinite(value))
    return "null";
  std::ostringstream stream;
  stream << std::setprecision(15) << value;
  return stream.str();
}

bool finiteTensor(const pd::Tensor3& value) {
  return std::all_of(value.begin(), value.end(),
                     [](double x) { return std::isfinite(x); });
}

double vectorNorm(const pd::Vec3& value) {
  return std::sqrt(value[0]*value[0]+value[1]*value[1]+value[2]*value[2]);
}

double contraction(const pd::Tensor3& stress, const pd::Tensor3& strain) {
  return stress[0]*strain[0]+stress[4]*strain[4]+stress[8]*strain[8]+
      2.0*(stress[1]*strain[1]+stress[2]*strain[2]+stress[5]*strain[5]);
}

double kineticEnergy(const pd::ParticleBlock& block,
                     const pd::ExplicitDynamicState& state,
                     double density) {
  double value = 0.0;
  for (size_t i=0;i<block.referencePosition.size();++i) {
    const auto& v=state.velocity[i];
    value += 0.5*density*block.volume[i]*(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
  }
  return value;
}

double meanCoordinate(const pd::ParticleBlock& block,
                      const std::vector<size_t>& indices,
                      int component,
                      bool displacement) {
  double sum=0.0;
  for(size_t i:indices)
    sum += displacement ? block.displacement[i][static_cast<size_t>(component)]
                        : block.referencePosition[i][static_cast<size_t>(component)];
  return sum/static_cast<double>(indices.size());
}

double internalIndicator(const DemoMaterialConfig& material,
                         const std::vector<double>& state) {
  if(material.internalIndicatorIndices.empty()) return 0.0;
  double value=material.internalIndicatorReduction=="min"
      ? std::numeric_limits<double>::infinity()
      : -std::numeric_limits<double>::infinity();
  for(int index:material.internalIndicatorIndices) {
    const double item=state[static_cast<size_t>(index)];
    value=material.internalIndicatorReduction=="min" ? std::min(value,item)
                                                       : std::max(value,item);
  }
  if(material.internalIndicatorTransform=="one_minus") value=1.0-value;
  return value;
}

std::vector<double> initialState(const DemoMaterialConfig& material) {
  return material.initialState.empty()
      ? std::vector<double>(static_cast<size_t>(material.material.nstatev),0.0)
      : material.initialState;
}

void writeSnapshot(std::ofstream& out, const std::string& materialName,
                   int step, double time, const pd::ParticleBlock& block,
                   const pd::ExplicitDynamicState& dynamicState,
                   const pd::MaterialUpdateResult& result,
                   const pd::PdDamageResult& damage,
                   const pd::MaterialState& materialState,
                   const DemoMaterialConfig& material, int axis) {
  for(size_t i=0;i<block.referencePosition.size();++i) {
    const auto& x0=block.referencePosition[i];
    const auto& u=block.displacement[i];
    const auto& v=dynamicState.velocity[i];
    const double eqps=material.equivalentPlasticStrainIndex>=0
        ? materialState.vumatStateN[i][static_cast<size_t>(material.equivalentPlasticStrainIndex)]
        : 0.0;
    out << materialName << ',' << step << ',' << time << ',' << i << ','
        << x0[0] << ',' << x0[1] << ',' << x0[2] << ','
        << u[0] << ',' << u[1] << ',' << u[2] << ','
        << x0[0]+u[0] << ',' << x0[1]+u[1] << ',' << x0[2]+u[2] << ','
        << v[0] << ',' << v[1] << ',' << v[2] << ','
        << result.strain[i][static_cast<size_t>(4*axis)] << ','
        << result.rawStress[i][static_cast<size_t>(4*axis)] << ','
        << eqps << ',' << internalIndicator(material,materialState.vumatStateN[i]) << ','
        << damage.damageIndex[i] << '\n';
  }
}

RunSummary runMaterial(const DemoCaseConfig& config,
                       const DemoMaterialConfig& material,
                       std::ofstream& timeseries,
                       std::ofstream& snapshots) {
  const auto started=std::chrono::steady_clock::now();
  auto block=makeParticlePatch(config.geometry.nx,config.geometry.ny,config.geometry.nz,
                               config.geometry.spacing,config.geometry.horizon);
  RegionalConstraintSystem constraints(block,config.boundaryCondition,
                                       config.geometry.spacing);
  const int axis=constraints.loadComponent();
  const int lateral=constraints.lateralComponent();
  const double loadingSign=config.boundaryCondition.amplitude>0.0?1.0:-1.0;
  const double gaugeLength=std::abs(meanCoordinate(block,constraints.loaded(),axis,false)-
                                    meanCoordinate(block,constraints.support(),axis,false));
  const std::string negativeSide=lateral==0?"left":"bottom";
  const std::string positiveSide=lateral==0?"right":"top";
  const auto lateralNegative=selectBoundaryRegion(block,negativeSide,1.0e-9*config.geometry.spacing);
  const auto lateralPositive=selectBoundaryRegion(block,positiveSide,1.0e-9*config.geometry.spacing);
  const double lateralGauge=std::abs(meanCoordinate(block,lateralPositive,lateral,false)-
                                     meanCoordinate(block,lateralNegative,lateral,false));
  const int lateralCount=lateral==0?config.geometry.nx:config.geometry.ny;
  (void)lateralCount;
  const int crossCount=axis==0?config.geometry.ny:config.geometry.nx;
  const double crossSectionArea=static_cast<double>((crossCount-1)*(config.geometry.nz-1))*
                                config.geometry.spacing*config.geometry.spacing;

  pd::PdCorrespondenceSkeleton skeleton(config.geometry.horizon);
  pd::CorrespondenceState correspondenceState;
  skeleton.initialize(block,correspondenceState);
  pd::VumatPdCorrespondenceConfig materialConfig;
  materialConfig.material=material.material;
  materialConfig.charLength=config.geometry.spacing;
  materialConfig.referenceTemperature=material.referenceTemperature;
  materialConfig.kinematicsMode =
      material.kinematicsMode == "legacy_small_strain"
      ? pd::KinematicsMode::LegacySmallStrain
      : pd::KinematicsMode::Corotational;
  materialConfig.initialState=initialState(material);
  pd::VumatPdCorrespondenceMaterial bridge(materialConfig);
  pd::MaterialState materialState;
  bridge.initialize(skeleton.computeDeformationGradients(block,correspondenceState),materialState);
  pd::PdSimpleDamage damageModel(config.damage);
  pd::PdDamageState damageState;
  damageModel.initialize(block,damageState);
  pd::ExplicitTimeIntegrator integrator(config.dynamics.density);
  pd::ExplicitDynamicState dynamicState;
  integrator.initialize(block,dynamicState);
  constraints.apply(block,dynamicState,0.0);

  RunSummary summary;
  summary.material=material.label;
  summary.analyticalPlaneStrainSlope=material.elasticE/(1.0-material.elasticNu*material.elasticNu);
  double internalEnergy=0.0;
  double externalWork=0.0;
  double previousPower=0.0;
  double slopeX2=0.0,slopeXY=0.0;
  pd::MaterialUpdateResult finalResult;
  pd::PdDamageResult finalDamage;

  for(int step=1;step<=config.timeIntegration.steps;++step) {
    const double time=static_cast<double>(step)*config.timeIntegration.dt;
    integrator.predictDisplacement(block,dynamicState,config.timeIntegration.dt);
    constraints.apply(block,dynamicState,time);
    const auto previousStress=materialState.rawStressN;
    const auto previousStrain=materialState.strainN;
    const auto deformationGradient=skeleton.computeDeformationGradients(block,correspondenceState);
    auto result=bridge.update(deformationGradient,materialState,config.timeIntegration.dt,
                              time,time);
    auto damage=damageModel.update(block,result.strain,result.rawStress,
                                   materialState.vumatStateN,damageState);
    auto force=skeleton.computeForceDensity(block,correspondenceState,
        result.deformationGradient,damage.effectiveStress,damage.bondDamage);
    integrator.correctVelocity(dynamicState,force,config.timeIntegration.dt);
    constraints.apply(block,dynamicState,time);

    for(size_t i=0;i<block.referencePosition.size();++i) {
      pd::Tensor3 averageStress{},strainIncrement{};
      for(size_t c=0;c<9;++c) {
        averageStress[c]=0.5*(previousStress[i][c]+result.rawStress[i][c]);
        strainIncrement[c]=result.strain[i][c]-previousStrain[i][c];
      }
      internalEnergy += contraction(averageStress,strainIncrement)*block.volume[i];
    }

    pd::Vec3 netInternal{0.0,0.0,0.0};
    double reaction=0.0,averageStress=0.0,averageAxialStrain=0.0;
    double maxEqps=0.0,maxIndicator=0.0;
    const RampKinematics ramp=constraints.ramp(time);
    std::vector<char> boundary(block.referencePosition.size(),0);
    for(size_t i:constraints.support()) boundary[i]=1;
    for(size_t i:constraints.loaded()) boundary[i]=1;
    double interiorVolume=0.0;
    for(size_t i=0;i<block.referencePosition.size();++i) {
      for(int d=0;d<3;++d)
        netInternal[static_cast<size_t>(d)] += force[i][static_cast<size_t>(d)]*block.volume[i];
      if(block.referencePosition[i][0]>=2.0-1e-8 && block.referencePosition[i][0]<=98.0+1e-8) {
        const double w=block.volume[i]*((std::abs(block.referencePosition[i][0]-2.0)<1e-8 || std::abs(block.referencePosition[i][0]-98.0)<1e-8)?0.5:1.0);
        averageStress += loadingSign*result.rawStress[i][static_cast<size_t>(4*axis)]*w;
        averageAxialStrain += loadingSign*result.strain[i][static_cast<size_t>(4*axis)]*w;
        interiorVolume+=w;
      }
      if(material.equivalentPlasticStrainIndex>=0)
        maxEqps=std::max(maxEqps,materialState.vumatStateN[i][static_cast<size_t>(material.equivalentPlasticStrainIndex)]);
      maxIndicator=std::max(maxIndicator,internalIndicator(material,materialState.vumatStateN[i]));
      summary.finite=summary.finite && finiteTensor(result.rawStress[i]) &&
          finiteTensor(result.strain[i]) && std::isfinite(damage.damageIndex[i]) &&
          std::isfinite(force[i][0]) && std::isfinite(force[i][1]) && std::isfinite(force[i][2]);
      for(double value:materialState.vumatStateN[i]) summary.finite=summary.finite&&std::isfinite(value);
    }
    for(size_t i:constraints.loaded())
      reaction += (config.dynamics.density*ramp.acceleration-force[i][static_cast<size_t>(axis)])*
                  block.volume[i];
    averageStress/=interiorVolume;
    averageAxialStrain/=interiorVolume;
    const double power=reaction*ramp.velocity;
    externalWork += 0.5*config.timeIntegration.dt*(previousPower+power);
    previousPower=power;
    const double kinetic=kineticEnergy(block,dynamicState,config.dynamics.density);
    const double scale=std::max({std::abs(externalWork),std::abs(internalEnergy)+kinetic,1.0e-12});
    const double energyError=std::abs(kinetic+internalEnergy-externalWork)/scale;
    const double kineticRatio=kinetic/std::max(std::abs(internalEnergy),1.0e-12);
    const double engineeringStrain=loadingSign*ramp.displacement/gaugeLength;
    const double lateralStrain=(meanCoordinate(block,lateralPositive,lateral,true)-
                                meanCoordinate(block,lateralNegative,lateral,true))/lateralGauge;
    const double nominalStress=loadingSign*reaction/crossSectionArea;
    summary.maxNetInternalForce=std::max(summary.maxNetInternalForce,vectorNorm(netInternal));
    summary.maxEnergyError=std::max(summary.maxEnergyError,energyError);
    summary.maxKineticToInternal=std::max(summary.maxKineticToInternal,kineticRatio);
    summary.finalKineticToInternal=kineticRatio;
    summary.maxEqps=std::max(summary.maxEqps,maxEqps);
    summary.maxInternalIndicator=std::max(summary.maxInternalIndicator,maxIndicator);
    summary.finalEnergyError=energyError;
    summary.finalReaction=reaction;
    summary.finalNominalStress=nominalStress;
    summary.finalEngineeringStrain=engineeringStrain;
    summary.finite=summary.finite && std::isfinite(kinetic) && std::isfinite(internalEnergy) &&
        std::isfinite(externalWork) && std::isfinite(energyError);
    if(engineeringStrain>1.0e-6 && engineeringStrain<=1.0e-3) {
      slopeX2+=engineeringStrain*engineeringStrain;
      slopeXY+=engineeringStrain*averageStress;
    }

    if(step%config.dynamics.outputEvery==0 || step==1 || step==config.timeIntegration.steps)
      timeseries << material.label << ',' << step << ',' << std::setprecision(16) << time << ','
          << ramp.displacement << ',' << ramp.velocity << ',' << reaction << ','
          << engineeringStrain << ',' << lateralStrain << ',' << nominalStress << ','
          << averageAxialStrain << ',' << averageStress << ',' << maxEqps << ','
          << maxIndicator << ',' << kinetic << ',' << internalEnergy << ',' << externalWork << ','
          << energyError << ',' << vectorNorm(netInternal) << ',' << (summary.finite?1:0) << '\n';
    if(step%config.dynamics.snapshotEvery==0 || step==config.timeIntegration.steps)
      writeSnapshot(snapshots,material.label,step,time,block,dynamicState,result,damage,
                    materialState,material,axis);
    finalResult=std::move(result);
    finalDamage=std::move(damage);
  }

  summary.earlyElasticSlope=slopeX2>0.0?slopeXY/slopeX2:0.0;
  summary.earlyElasticRelativeError=summary.analyticalPlaneStrainSlope>0.0
      ? std::abs(summary.earlyElasticSlope-summary.analyticalPlaneStrainSlope)/
          summary.analyticalPlaneStrainSlope
      : 0.0;
  summary.runtimeSeconds=std::chrono::duration<double>(
      std::chrono::steady_clock::now()-started).count();
  summary.pass=summary.finite && summary.maxNetInternalForce<=config.dynamics.netForceTolerance &&
      summary.finalEnergyError<=config.dynamics.energyToleranceRelative &&
      summary.finalKineticToInternal<=config.dynamics.kineticToInternalLimit &&
      summary.maxEqps>=config.dynamics.minimumPlasticStrain;
  return summary;
}

} // namespace

int runBoundaryValueCase(const DemoCaseConfig& config) {
  std::filesystem::create_directories(config.output.directory);
  std::ofstream timeseries(config.output.directory+"/timeseries.csv");
  std::ofstream snapshots(config.output.directory+"/field_snapshots.csv");
  if(!timeseries||!snapshots)
    throw std::runtime_error("Failed to open boundary-value output files.");
  timeseries << "material,step,time_ms,load_displacement_mm,load_velocity_mm_per_ms,"
      "reaction_force_N,engineering_strain,lateral_strain,nominal_stress_MPa,"
      "average_axial_strain,average_axial_stress_MPa,max_eqps,max_internal_indicator,"
      "kinetic_energy_Nmm,internal_energy_Nmm,external_work_Nmm,relative_energy_error,"
      "net_internal_force_N,finite\n";
  snapshots << std::setprecision(16);
  snapshots << "material,step,time_ms,particle,x0_mm,y0_mm,z0_mm,ux_mm,uy_mm,uz_mm,"
      "x_mm,y_mm,z_mm,vx_mm_per_ms,vy_mm_per_ms,vz_mm_per_ms,axial_strain,"
      "axial_stress_MPa,eqps,internal_indicator,simple_damage\n";

  std::vector<RunSummary> summaries;
  for(const auto& material:config.materials) {
    RunSummary summary=runMaterial(config,material,timeseries,snapshots);
    std::cout << config.name << '/' << summary.material << ": particles="
              << config.geometry.nx*config.geometry.ny*config.geometry.nz
              << " runtime_s=" << summary.runtimeSeconds
              << " energy_error=" << summary.finalEnergyError
              << " max_eqps=" << summary.maxEqps
              << " pass=" << (summary.pass?"true":"false") << '\n';
    summaries.push_back(summary);
  }

  bool pass=std::all_of(summaries.begin(),summaries.end(),
                        [](const RunSummary& item){return item.pass;});
  std::ofstream metadata(config.output.directory+"/metadata.json");
  metadata << "{\n  \"name\": \"" << config.name << "\",\n"
      << "  \"solver\": \"correspondence peridynamics\",\n"
      << "  \"status\": \"" << (pass?"pass":"fail") << "\",\n"
      << "  \"classification\": \"" << config.classification << "\",\n"
      << "  \"scope\": \"" << config.scope << "\",\n"
      << "  \"units\": {\"length\": \"mm\", \"time\": \"ms\", \"force\": \"N\", "
         "\"stress\": \"MPa\", \"solverMassDensity\": \"N*ms^2/mm^4\", "
         "\"sourceDensity\": \"kg/m^3\"},\n"
      << "  \"densityConversion\": \"rho_solver = rho_kg_per_m3 * 1e-6\",\n"
      << "  \"particles\": " << config.geometry.nx*config.geometry.ny*config.geometry.nz << ",\n"
      << "  \"grid\": [" << config.geometry.nx << ',' << config.geometry.ny << ','
      << config.geometry.nz << "],\n  \"spacing_mm\": " << config.geometry.spacing
      << ",\n  \"horizon_mm\": " << config.geometry.horizon
      << ",\n  \"horizonSpacingRatio\": " << config.geometry.horizon/config.geometry.spacing
      << ",\n  \"neighborSearch\": \"cell-linked-list\",\n"
      << "  \"planeStrain\": " << (config.boundaryCondition.planeStrain?"true":"false") << ",\n  \"outerSimpleDamage\": false,\n"
      << "  \"boundarySets\": {\"support\": \"" << config.boundaryCondition.supportRegion
      << "\", \"loaded\": \"" << config.boundaryCondition.loadRegion
      << "\", \"regionLayers\": " << config.boundaryCondition.regionLayers
      << ", \"rigidBodySuppression\": \"two lateral components at support centre and z at support upper midplane point\"},\n"
      << "  \"dt_ms\": " << config.timeIntegration.dt << ",\n  \"steps\": "
      << config.timeIntegration.steps << ",\n  \"materials\": [\n";
  for(size_t i=0;i<summaries.size();++i) {
    const auto& s=summaries[i];
    const auto& material=config.materials[i];
    metadata << "    {\"name\": \"" << s.material << "\", \"status\": \""
        << (s.pass?"pass":"fail") << "\", \"plugin_file\": \""
        << std::filesystem::path(material.material.pluginPath).filename().string() << "\", \"sourceDensity_kg_per_m3\": "
        << jsonNumber(material.material.density) << ", \"solverDensity\": " << jsonNumber(config.dynamics.density)
        << ", \"runtimeSeconds\": " << jsonNumber(s.runtimeSeconds)
        << ", \"maxNetInternalForce_N\": " << jsonNumber(s.maxNetInternalForce)
        << ", \"finalEnergyError\": " << jsonNumber(s.finalEnergyError)
        << ", \"maxEnergyError\": " << jsonNumber(s.maxEnergyError)
        << ", \"maxKineticToInternal\": " << jsonNumber(s.maxKineticToInternal)
        << ", \"finalKineticToInternal\": " << jsonNumber(s.finalKineticToInternal)
        << ", \"maxEqps\": " << jsonNumber(s.maxEqps)
        << ", \"maxInternalIndicator\": " << jsonNumber(s.maxInternalIndicator)
        << ", \"earlyElasticSlope_MPa\": " << jsonNumber(s.earlyElasticSlope)
        << ", \"analyticalPlaneStrainSlope_MPa\": " << jsonNumber(s.analyticalPlaneStrainSlope)
        << ", \"earlyElasticRelativeError\": " << jsonNumber(s.earlyElasticRelativeError)
        << ", \"finalReaction_N\": " << jsonNumber(s.finalReaction)
        << ", \"finalNominalStress_MPa\": " << jsonNumber(s.finalNominalStress)
        << ", \"finalEngineeringStrain\": " << jsonNumber(s.finalEngineeringStrain)
        << ", \"finite\": " << (s.finite?"true":"false") << "}"
        << (i+1==summaries.size()?"\n":",\n");
  }
  metadata << "  ],\n  \"acceptance\": {\"energyToleranceRelative\": "
      << config.dynamics.energyToleranceRelative << ", \"netForceTolerance_N\": "
      << config.dynamics.netForceTolerance << ", \"minimumPlasticStrain\": "
      << config.dynamics.minimumPlasticStrain << ", \"kineticToInternalLimit\": "
      << config.dynamics.kineticToInternalLimit << "},\n"
      << "  \"materialComparisonCaveat\": \"Internal state variables are model-specific observers and are not mapped to a common bond-breaking rule.\",\n"
      << "  \"parameterMatchingPolicy\": \"Only identifiable macroscopic properties are matched; formulation-specific or hard-coded parameters are not claimed to be equivalent.\",\n"
      << "  \"legacyBlockPolicy\": \"Material-independent fixed-width blocks of 256 points; a non-full final block duplicates its last valid point and discards padded outputs.\"\n}\n";
  return pass?0:2;
}

} // namespace demo
} // namespace vumat
