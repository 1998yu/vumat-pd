#include "TensorVoigtMap.hpp"

namespace vumat {

std::array<double, 6> fullTensorToVumatVoigt(const std::array<double, 9>& full) {
  return {full[0],
          full[4],
          full[8],
          0.5 * (full[5] + full[7]),
          0.5 * (full[2] + full[6]),
          0.5 * (full[1] + full[3])};
}

std::array<double, 9> vumatVoigtToFullTensor(const std::array<double, 6>& voigt) {
  return {voigt[0], voigt[5], voigt[4],
          voigt[5], voigt[1], voigt[3],
          voigt[4], voigt[3], voigt[2]};
}

std::array<double, 9> fullTensorToVumatDefgrad(const std::array<double, 9>& full) {
  return {full[0], full[4], full[8], full[1], full[5], full[6], full[3], full[7], full[2]};
}

std::array<double, 6> strainRateFullToVumatStrainInc(const std::array<double, 9>& rateFull,
                                                     double dt) {
  auto voigt = fullTensorToVumatVoigt(rateFull);
  for (double& value : voigt)
    value *= dt;
  return voigt;
}

} // namespace vumat
