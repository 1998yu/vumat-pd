#pragma once

#include "PdTypes.hpp"
#include "VumatMaterialAdapter.hpp"

#include <vector>

namespace vumat {
namespace pd {

enum class KinematicsMode {
  Corotational,
  LegacySmallStrain
};

struct VumatPdCorrespondenceConfig {
  VumatMaterialConfig material;
  double charLength = 1.0;
  double referenceTemperature = 293.0;
  std::vector<double> initialState;
  KinematicsMode kinematicsMode = KinematicsMode::Corotational;
};

class VumatPdCorrespondenceMaterial {
public:
  explicit VumatPdCorrespondenceMaterial(VumatPdCorrespondenceConfig config);

  void initialize(const std::vector<Tensor3>& initialDeformationGradient,
                  MaterialState& state) const;

  MaterialUpdateResult update(const std::vector<Tensor3>& deformationGradient,
                              MaterialState& state,
                              double dt,
                              double stepTime,
                              double totalTime) const;

private:
  VumatPdCorrespondenceConfig config_;
  VumatMaterialAdapter adapter_;
  std::vector<std::unique_ptr<VumatMaterialAdapter>> threadAdapters_;
};

} // namespace pd
} // namespace vumat
