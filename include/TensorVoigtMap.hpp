#pragma once

#include <array>

namespace vumat {

std::array<double, 6> fullTensorToVumatVoigt(const std::array<double, 9>& full);
std::array<double, 9> vumatVoigtToFullTensor(const std::array<double, 6>& voigt);
std::array<double, 9> fullTensorToVumatDefgrad(const std::array<double, 9>& full);
std::array<double, 6> strainRateFullToVumatStrainInc(const std::array<double, 9>& rateFull,
                                                     double dt);

} // namespace vumat
