#include "TensorVoigtMap.hpp"

#include <algorithm>
#include <cmath>

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

double maxPrincipalStrainSymmetric3x3(const std::array<double, 9>& full) {
  const double a = full[0];
  const double b = 0.5 * (full[1] + full[3]);
  const double c = 0.5 * (full[2] + full[6]);
  const double d = full[4];
  const double e = 0.5 * (full[5] + full[7]);
  const double f = full[8];

  const double p1 = b * b + c * c + e * e;
  if (p1 == 0.0)
    return std::max({a, d, f});

  const double q = (a + d + f) / 3.0;
  const double a11 = a - q;
  const double a22 = d - q;
  const double a33 = f - q;
  const double p2 = a11 * a11 + a22 * a22 + a33 * a33 + 2.0 * p1;
  const double p = std::sqrt(p2 / 6.0);

  const double invP = 1.0 / p;
  const double b11 = a11 * invP;
  const double b12 = b * invP;
  const double b13 = c * invP;
  const double b22 = a22 * invP;
  const double b23 = e * invP;
  const double b33 = a33 * invP;

  const double detB =
      b11 * (b22 * b33 - b23 * b23) -
      b12 * (b12 * b33 - b23 * b13) +
      b13 * (b12 * b23 - b22 * b13);
  const double r = std::clamp(detB / 2.0, -1.0, 1.0);
  const double pi = 3.141592653589793238462643383279502884;
  const double phi = std::acos(r) / 3.0;

  const double eig1 = q + 2.0 * p * std::cos(phi);
  const double eig3 = q + 2.0 * p * std::cos(phi + 2.0 * pi / 3.0);
  const double eig2 = 3.0 * q - eig1 - eig3;
  return std::max({eig1, eig2, eig3});
}

} // namespace vumat
