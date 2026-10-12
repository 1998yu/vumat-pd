// Neo-Hookean spatial-stress demo; reuses the current PD host and adapter.
#include "ExplicitTimeIntegrator.hpp"
#include "Geometry.hpp"
#include "MaterialConfig.hpp"
#include "PdCorrespondenceSkeleton.hpp"
#include "VumatMaterialAdapter.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using vumat::pd::ParticleBlock;
using vumat::pd::Tensor3;
using vumat::pd::Vec3;

constexpr double kPi = 3.14159265358979323846;

struct MotionState {
  double angle = 0.0;
  double angularVelocity = 0.0;
  double angularAcceleration = 0.0;
};

struct PointKinematics {
  Vec3 position{};
  Vec3 velocity{};
  Vec3 acceleration{};
};

std::string executableDirectory(const char* argv0) {
  const std::filesystem::path path(argv0 ? argv0 : "");
  return path.has_parent_path() ? path.parent_path().string() : ".";
}

MotionState smoothAngle(double time, double riseTime, double maximumAngle) {
  if (time >= riseTime)
    return {maximumAngle, 0.0, 0.0};
  const double phase = kPi * time / riseTime;
  return {
      0.5 * maximumAngle * (1.0 - std::cos(phase)),
      0.5 * maximumAngle * kPi / riseTime * std::sin(phase),
      0.5 * maximumAngle * kPi * kPi / (riseTime * riseTime) * std::cos(phase)};
}

Vec3 add(const Vec3& a, const Vec3& b) {
  return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
}

Vec3 subtract(const Vec3& a, const Vec3& b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Vec3 scale(const Vec3& a, double factor) {
  return {factor * a[0], factor * a[1], factor * a[2]};
}

Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a[1] * b[2] - a[2] * b[1],
          a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}

double dot(const Vec3& a, const Vec3& b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

double norm(const Vec3& a) {
  return std::sqrt(dot(a, a));
}

double determinant(const Tensor3& a) {
  return a[0] * (a[4] * a[8] - a[5] * a[7]) -
         a[1] * (a[3] * a[8] - a[5] * a[6]) +
         a[2] * (a[3] * a[7] - a[4] * a[6]);
}

Tensor3 inverse(const Tensor3& a) {
  const double det = determinant(a);
  if (std::abs(det) < 1.0e-12)
    throw std::runtime_error("Singular deformation gradient in hyperelastic update.");
  return {(a[4] * a[8] - a[5] * a[7]) / det,
          -(a[1] * a[8] - a[2] * a[7]) / det,
          (a[1] * a[5] - a[2] * a[4]) / det,
          -(a[3] * a[8] - a[5] * a[6]) / det,
          (a[0] * a[8] - a[2] * a[6]) / det,
          -(a[0] * a[5] - a[2] * a[3]) / det,
          (a[3] * a[7] - a[4] * a[6]) / det,
          -(a[0] * a[7] - a[1] * a[6]) / det,
          (a[0] * a[4] - a[1] * a[3]) / det};
}

Tensor3 multiply(const Tensor3& a, const Tensor3& b) {
  Tensor3 out{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      for (int k = 0; k < 3; ++k)
        out[static_cast<size_t>(3 * i + j)] +=
            a[static_cast<size_t>(3 * i + k)] * b[static_cast<size_t>(3 * k + j)];
  return out;
}

Tensor3 rateOfDeformation(const Tensor3& oldF, const Tensor3& newF, double dt) {
  Tensor3 fDot{};
  for (size_t i = 0; i < 9; ++i) fDot[i] = (newF[i] - oldF[i]) / dt;
  const Tensor3 l = multiply(fDot, inverse(newF));
  Tensor3 d{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      d[static_cast<size_t>(3 * i + j)] =
          0.5 * (l[static_cast<size_t>(3 * i + j)] +
                 l[static_cast<size_t>(3 * j + i)]);
  return d;
}

double vonMises(const Tensor3& stress) {
  const double sxx = stress[0];
  const double syy = stress[4];
  const double szz = stress[8];
  const double sxy = 0.5 * (stress[1] + stress[3]);
  const double sxz = 0.5 * (stress[2] + stress[6]);
  const double syz = 0.5 * (stress[5] + stress[7]);
  return std::sqrt(std::max(0.0, 0.5 * ((sxx - syy) * (sxx - syy) +
                                        (syy - szz) * (syy - szz) +
                                        (szz - sxx) * (szz - sxx)) +
                                  3.0 * (sxy * sxy + sxz * sxz + syz * syz)));
}

bool finiteTensor(const Tensor3& tensor) {
  return std::all_of(tensor.begin(), tensor.end(),
                     [](double value) { return std::isfinite(value); });
}

PointKinematics twistKinematics(const Vec3& reference,
                                const MotionState& motion) {
  const double c = std::cos(motion.angle);
  const double s = std::sin(motion.angle);
  const Vec3 target{reference[0], c * reference[1] - s * reference[2],
                    s * reference[1] + c * reference[2]};
  const Vec3 derivative{0.0, -s * reference[1] - c * reference[2],
                        c * reference[1] - s * reference[2]};
  const Vec3 secondDerivative{0.0, -c * reference[1] + s * reference[2],
                              -s * reference[1] - c * reference[2]};
  return {target, scale(derivative, motion.angularVelocity),
          add(scale(secondDerivative, motion.angularVelocity * motion.angularVelocity),
              scale(derivative, motion.angularAcceleration))};
}

PointKinematics bendKinematics(const Vec3& reference, double length,
                               const MotionState& motion) {
  const double theta = motion.angle;
  const double c = std::cos(theta);
  const double s = std::sin(theta);
  const Vec3 q{reference[0] - length, reference[1], reference[2]};

  // The end centre follows a constant-curvature circular arc of arc length L.
  // The theta -> 0 limit is handled explicitly for the initial frame.
  double cx = length;
  double cy = 0.0;
  double dcx = 0.0;
  double dcy = 0.5 * length;
  double d2cx = -length / 3.0;
  double d2cy = 0.0;
  if (std::abs(theta) > 1.0e-7) {
    const double inv = 1.0 / theta;
    const double inv2 = inv * inv;
    const double inv3 = inv2 * inv;
    cx = length * s * inv;
    cy = length * (1.0 - c) * inv;
    dcx = length * (theta * c - s) * inv2;
    dcy = length * (theta * s - (1.0 - c)) * inv2;
    d2cx = length * (-theta * theta * s - 2.0 * theta * c + 2.0 * s) * inv3;
    d2cy = length * (theta * theta * c - 2.0 * theta * s + 2.0 * (1.0 - c)) * inv3;
  }

  const Vec3 rotated{c * q[0] - s * q[1], s * q[0] + c * q[1], q[2]};
  const Vec3 derivative{dcx - s * q[0] - c * q[1],
                        dcy + c * q[0] - s * q[1], 0.0};
  const Vec3 secondDerivative{d2cx - c * q[0] + s * q[1],
                              d2cy - s * q[0] - c * q[1], 0.0};
  const Vec3 target{cx + rotated[0], cy + rotated[1], rotated[2]};
  return {target, scale(derivative, motion.angularVelocity),
          add(scale(secondDerivative, motion.angularVelocity * motion.angularVelocity),
              scale(derivative, motion.angularAcceleration))};
}

void prescribePoint(size_t index, const PointKinematics& prescribed,
                    ParticleBlock& block, vumat::pd::ExplicitDynamicState& state) {
  block.displacement[index] = subtract(prescribed.position, block.referencePosition[index]);
  state.velocity[index] = prescribed.velocity;
  state.acceleration[index] = prescribed.acceleration;
}

void applyConstraints(const std::string& scenario, double time, double riseTime,
                      double maximumAngle, double length,
                      const std::vector<char>& support,
                      const std::vector<char>& loaded, ParticleBlock& block,
                      vumat::pd::ExplicitDynamicState& state) {
  const MotionState motion = smoothAngle(time, riseTime, maximumAngle);
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    if (support[i]) {
      prescribePoint(i, {block.referencePosition[i], {0.0, 0.0, 0.0},
                         {0.0, 0.0, 0.0}}, block, state);
    } else if (loaded[i]) {
      const PointKinematics prescribed = scenario == "torsion"
          ? twistKinematics(block.referencePosition[i], motion)
          : bendKinematics(block.referencePosition[i], length, motion);
      prescribePoint(i, prescribed, block, state);
    }
  }
}

void writeSnapshot(std::ofstream& out, int step, double time,
                   const ParticleBlock& block,
                   const std::vector<Tensor3>* deformationGradient,
                   const std::vector<Tensor3>* stress,
                   const std::vector<char>& support,
                   const std::vector<char>& loaded) {
  for (size_t i = 0; i < block.referencePosition.size(); ++i) {
    const Vec3 current = add(block.referencePosition[i], block.displacement[i]);
    const double equivalent = stress ? vonMises((*stress)[i]) : 0.0;
    const double jacobian = deformationGradient ? determinant((*deformationGradient)[i]) : 1.0;
    out << step << ',' << std::setprecision(16) << time << ',' << i << ','
        << block.referencePosition[i][0] << ',' << block.referencePosition[i][1] << ','
        << block.referencePosition[i][2] << ',' << current[0] << ',' << current[1]
        << ',' << current[2] << ',' << equivalent << ',' << jacobian << ','
        << static_cast<int>(support[i]) << ',' << static_cast<int>(loaded[i]) << '\n';
  }
}

} // namespace

int main(int argc, char** argv) {
  try {
    const std::string scenario = argc > 1 ? argv[1] : "bending";
    if (scenario != "bending" && scenario != "torsion")
      throw std::runtime_error("Scenario must be 'bending' or 'torsion'.");
    const std::string outputDirectory = argc > 2 ? argv[2]
        : "demos/output/hyperelastic/" + scenario;
    const int steps = argc > 3 ? std::stoi(argv[3]) : 2400;
    const int snapshotEvery = argc > 4 ? std::stoi(argv[4]) : 24;
    const double angleDegrees = argc > 5 ? std::stod(argv[5])
        : (scenario == "bending" ? 150.0 : 540.0);
    if (steps < 1 || snapshotEvery < 1 || !std::isfinite(angleDegrees) || angleDegrees <= 0.0)
      throw std::runtime_error("Steps, snapshot interval, and angle must be finite and positive.");

    const int nx = 41;
    const int ny = 5;
    const int nz = 5;
    const double spacing = 2.0;
    const double horizon = 3.0 * spacing;
    const double length = static_cast<double>(nx - 1) * spacing;
    const double density = 0.0078;
    const double dt = 0.0005;
    const double totalTime = dt * static_cast<double>(steps);
    const double maximumAngle = angleDegrees * kPi / 180.0;
    const double dampingCoefficient = 8.0; // 1/ms; gentle dynamic relaxation.
    const int boundaryLayers = 3;

    std::filesystem::create_directories(outputDirectory);
    std::ofstream snapshots(outputDirectory + "/field_snapshots.csv");
    std::ofstream timeseries(outputDirectory + "/timeseries.csv");
    if (!snapshots || !timeseries)
      throw std::runtime_error("Unable to open output CSV files.");
    snapshots << "step,time_ms,particle,x0_mm,y0_mm,z0_mm,x_mm,y_mm,z_mm,"
                 "von_mises_MPa,J,support,loaded\n";
    timeseries << "step,time_ms,angle_deg,reaction_force_N,reaction_moment_Nmm,"
                  "max_displacement_mm,max_von_mises_MPa,min_J,max_J,kinetic_energy_Nmm,finite\n";

    ParticleBlock block = vumat::demo::makeParticlePatch(nx, ny, nz, spacing, horizon);
    const size_t particleCount = block.referencePosition.size();
    std::vector<char> support(particleCount, 0);
    std::vector<char> loaded(particleCount, 0);
    const double supportLimit = static_cast<double>(boundaryLayers - 1) * spacing + 1.0e-9;
    const double loadLimit = length - supportLimit;
    for (size_t i = 0; i < particleCount; ++i) {
      support[i] = block.referencePosition[i][0] <= supportLimit;
      loaded[i] = block.referencePosition[i][0] >= loadLimit;
    }

    vumat::pd::PdCorrespondenceSkeleton skeleton(horizon);
    vumat::pd::CorrespondenceState correspondenceState;
    skeleton.initialize(block, correspondenceState);

    const std::string pluginDirectory = executableDirectory(argv[0]);
    const auto initialDeformationGradient =
        skeleton.computeDeformationGradients(block, correspondenceState);
    vumat::VumatMaterialAdapter hyperelasticMaterial(
        vumat::loadMaterialConfigFromJson("configs/neohookean.json", pluginDirectory));
    std::vector<Tensor3> hyperelasticFOld = initialDeformationGradient;
    std::vector<Tensor3> hyperelasticStress(particleCount, Tensor3{});
    std::vector<std::vector<double>> hyperelasticState(
        particleCount, std::vector<double>(4, 0.0));
    for (auto& item : hyperelasticState) item[0] = 1.0;
    std::vector<double> hyperelasticInternalEnergy(particleCount, 0.0);
    std::vector<double> hyperelasticInelasticEnergy(particleCount, 0.0);
    std::vector<std::vector<double>> intactBonds(particleCount);
    for (size_t i = 0; i < particleCount; ++i)
      intactBonds[i].assign(block.neighbors[i].size(), 0.0);

    vumat::pd::ExplicitTimeIntegrator integrator(density);
    vumat::pd::ExplicitDynamicState dynamicState;
    integrator.initialize(block, dynamicState);
    applyConstraints(scenario, 0.0, totalTime, maximumAngle, length,
                     support, loaded, block, dynamicState);
    writeSnapshot(snapshots, 0, 0.0, block, nullptr, nullptr, support, loaded);
    timeseries << "0,0,0,0,0,0,0,1,1,0,1\n";

    bool finite = true;
    double finalMaximumDisplacement = 0.0;
    double finalMaximumStress = 0.0;
    double finalMinJ = 1.0;
    double finalMaxJ = 1.0;
    double finalReactionForce = 0.0;
    double finalReactionMoment = 0.0;
    const auto started = std::chrono::steady_clock::now();

    for (int step = 1; step <= steps; ++step) {
      const double time = dt * static_cast<double>(step);
      integrator.predictDisplacement(block, dynamicState, dt);
      applyConstraints(scenario, time, totalTime, maximumAngle, length,
                       support, loaded, block, dynamicState);

      const auto deformationGradient =
          skeleton.computeDeformationGradients(block, correspondenceState);
      std::vector<Tensor3> currentStress;
      std::vector<Vec3> force;
      {
        vumat::VumatBlockInput input;
        input.dt = dt;
        input.stepTime = time;
        input.totalTime = time;
        input.charLength.assign(particleCount, spacing);
        input.temperature.assign(particleCount, 293.0);
        input.strainRateFull.resize(particleCount);
        input.strainFullN.assign(particleCount, Tensor3{});
        input.stressFullN = hyperelasticStress;
        input.defgradOld = hyperelasticFOld;
        input.defgradNew = deformationGradient;
        input.stateOld = hyperelasticState;
        input.internalEnergyOld = hyperelasticInternalEnergy;
        input.inelasticEnergyOld = hyperelasticInelasticEnergy;
        for (size_t i = 0; i < particleCount; ++i)
          input.strainRateFull[i] = rateOfDeformation(
              hyperelasticFOld[i], deformationGradient[i], dt);
        auto result = hyperelasticMaterial.updateBlock(input);
        hyperelasticStress = std::move(result.stressFullNew);
        hyperelasticState = std::move(result.stateNew);
        hyperelasticInternalEnergy = std::move(result.internalEnergyNew);
        hyperelasticInelasticEnergy = std::move(result.inelasticEnergyNew);
        currentStress = hyperelasticStress;
        force = skeleton.computeForceDensity(block, correspondenceState,
            deformationGradient, currentStress, intactBonds);
        hyperelasticFOld = deformationGradient;
      }

      // Viscous dynamic relaxation suppresses ringing while preserving the
      // actual PD internal-force solve for all unconstrained particles.
      for (size_t i = 0; i < particleCount; ++i)
        if (!support[i] && !loaded[i])
          for (int d = 0; d < 3; ++d)
            force[i][static_cast<size_t>(d)] -= dampingCoefficient * density *
                dynamicState.velocity[i][static_cast<size_t>(d)];
      integrator.correctVelocity(dynamicState, force, dt);
      applyConstraints(scenario, time, totalTime, maximumAngle, length,
                       support, loaded, block, dynamicState);

      double maximumDisplacement = 0.0;
      double maximumStress = 0.0;
      double minJ = std::numeric_limits<double>::infinity();
      double maxJ = -std::numeric_limits<double>::infinity();
      double kineticEnergy = 0.0;
      Vec3 reactionForce{};
      Vec3 reactionMoment{};
      const MotionState motion = smoothAngle(time, totalTime, maximumAngle);
      Vec3 loadedCentre = scenario == "torsion"
          ? Vec3{length, 0.0, 0.0}
          : bendKinematics({length, 0.0, 0.0}, length, motion).position;

      for (size_t i = 0; i < particleCount; ++i) {
        maximumDisplacement = std::max(maximumDisplacement, norm(block.displacement[i]));
        maximumStress = std::max(maximumStress, vonMises(currentStress[i]));
        const double jacobian = determinant(deformationGradient[i]);
        minJ = std::min(minJ, jacobian);
        maxJ = std::max(maxJ, jacobian);
        kineticEnergy += 0.5 * density * block.volume[i] *
                         dot(dynamicState.velocity[i], dynamicState.velocity[i]);
        finite = finite && finiteTensor(deformationGradient[i]) &&
                 finiteTensor(currentStress[i]) && std::isfinite(jacobian) &&
                 std::isfinite(force[i][0]) && std::isfinite(force[i][1]) &&
                 std::isfinite(force[i][2]);
        if (loaded[i]) {
          const Vec3 reaction = scale(subtract(scale(dynamicState.acceleration[i], density),
                                                force[i]), block.volume[i]);
          reactionForce = add(reactionForce, reaction);
          const Vec3 current = add(block.referencePosition[i], block.displacement[i]);
          reactionMoment = add(reactionMoment,
                               cross(subtract(current, loadedCentre), reaction));
        }
      }
      const double reactionForceMagnitude = norm(reactionForce);
      const double reactionMomentComponent = scenario == "torsion"
          ? reactionMoment[0] : reactionMoment[2];
      const double angle = motion.angle * 180.0 / kPi;
      timeseries << step << ',' << std::setprecision(16) << time << ',' << angle << ','
                 << reactionForceMagnitude << ',' << reactionMomentComponent << ','
                 << maximumDisplacement << ',' << maximumStress << ',' << minJ << ','
                 << maxJ << ',' << kineticEnergy << ',' << (finite ? 1 : 0) << '\n';
      if (step % snapshotEvery == 0 || step == steps)
        writeSnapshot(snapshots, step, time, block, &deformationGradient,
                      &currentStress, support, loaded);

      finalMaximumDisplacement = maximumDisplacement;
      finalMaximumStress = maximumStress;
      finalMinJ = minJ;
      finalMaxJ = maxJ;
      finalReactionForce = reactionForceMagnitude;
      finalReactionMoment = reactionMomentComponent;
      if (!finite)
        throw std::runtime_error("Non-finite value encountered during the 3D solve.");
      if (step % std::max(1, steps / 10) == 0)
        std::cout << scenario << ": " << (100 * step / steps) << "% angle="
                  << angle << " deg max_vm=" << maximumStress << " MPa min_J="
                  << minJ << '\n';
    }

    const double runtimeSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    const bool completed = finite && finalMinJ > 0.0;
    std::ofstream metadata(outputDirectory + "/metadata.json");
    metadata << "{\n"
             << "  \"name\": \"large_motion_3d_" << scenario << "\",\n"
             << "  \"status\": \"" << (completed ? "completed" : "failed") << "\",\n"
             << "  \"interpretation\": \"3D VUMAT-PD boundary-value demonstration; not a benchmark validation\",\n"
             << "  \"scenario\": \"" << scenario << "\",\n"
             << "  \"kinematics\": \"finite rotation\",\n"
             << "  \"boundaryCondition\": \"three-layer fixed base and three-layer prescribed rigid-motion end\",\n"
             << "  \"particles\": " << particleCount << ",\n"
             << "  \"grid\": [" << nx << ", " << ny << ", " << nz << "],\n"
             << "  \"spacing_mm\": " << spacing << ",\n"
             << "  \"horizon_mm\": " << horizon << ",\n"
             << "  \"dt_ms\": " << dt << ",\n"
             << "  \"steps\": " << steps << ",\n"
             << "  \"maximumAngle_deg\": " << angleDegrees << ",\n"
             << "  \"material\": {\"model\": \""
             << "compressible Neo-Hookean VUMAT"
             << "\", \"E_MPa\": 2000.0, \"nu\": 0.30},\n"
             << "  \"dynamicRelaxationCoefficient_per_ms\": " << dampingCoefficient << ",\n"
             << "  \"finalMaximumDisplacement_mm\": " << finalMaximumDisplacement << ",\n"
             << "  \"finalMaximumVonMises_MPa\": " << finalMaximumStress << ",\n"
             << "  \"finalMinJ\": " << finalMinJ << ",\n"
             << "  \"finalMaxJ\": " << finalMaxJ << ",\n"
             << "  \"finalReactionForce_N\": " << finalReactionForce << ",\n"
             << "  \"finalReactionMoment_Nmm\": " << finalReactionMoment << ",\n"
             << "  \"runtimeSeconds\": " << runtimeSeconds << "\n"
             << "}\n";
    std::cout << "Completed " << scenario << " in " << runtimeSeconds
              << " s; outputs: " << outputDirectory << '\n';
    return completed ? 0 : 2;
  } catch (const std::exception& error) {
    std::cerr << "3D large-motion case failed: " << error.what() << '\n';
    return 1;
  }
}
