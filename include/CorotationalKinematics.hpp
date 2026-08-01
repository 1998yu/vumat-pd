#pragma once

#include "PdTypes.hpp"

namespace vumat {
namespace pd {

struct CorotationalUpdate {
  Tensor3 velocityGradient{};
  Tensor3 unrotatedRateOfDeformation{};
  Tensor3 relativeSpinIncrement{};
  Tensor3 rotationNP1{};
  Tensor3 leftStretchNP1{};
};

// Flanagan--Taylor kinematic update used by Peridigm correspondence models.
CorotationalUpdate updateCorotationalKinematics(
    const Tensor3& deformationGradientN,
    const Tensor3& deformationGradientNP1,
    const Tensor3& leftStretchN,
    const Tensor3& rotationN,
    double dt);

Tensor3 rotateCorotationalStressToSpatial(const Tensor3& corotationalStress,
                                          const Tensor3& rotation);

Tensor3 greenLagrangeStrain(const Tensor3& deformationGradient);

} // namespace pd
} // namespace vumat
