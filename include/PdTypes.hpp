#pragma once

#include <array>
#include <vector>

namespace vumat {
namespace pd {

using Vec3 = std::array<double, 3>;
using Tensor3 = std::array<double, 9>;

struct ParticleBlock {
  std::vector<Vec3> referencePosition;
  std::vector<Vec3> displacement;
  std::vector<double> volume;
  std::vector<std::vector<int>> neighbors;
};

struct CorrespondenceState {
  std::vector<Tensor3> shapeTensorInverse;
};

struct MaterialState {
  std::vector<Tensor3> deformationGradientN;
  std::vector<Tensor3> leftStretchN;
  std::vector<Tensor3> rotationN;
  std::vector<Tensor3> strainN;
  // VUMAT stresses are stored in the corotational material frame.
  std::vector<Tensor3> rawStressN;
  std::vector<std::vector<double>> vumatStateN;
  std::vector<double> temperatureN;
  std::vector<double> internalEnergyN;
  std::vector<double> inelasticEnergyN;
};

struct MaterialUpdateResult {
  std::vector<Tensor3> deformationGradient;
  std::vector<Tensor3> strain;
  std::vector<Tensor3> rotation;
  std::vector<Tensor3> leftStretch;
  // Spatial Cauchy stress used by the PD force-state assembly.
  std::vector<Tensor3> rawStress;
  std::vector<double> internalEnergy;
  std::vector<double> inelasticEnergy;
};

} // namespace pd
} // namespace vumat
