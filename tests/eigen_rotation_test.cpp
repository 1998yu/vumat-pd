// Independent matrix-exponential oracle; Eigen is optional and test-only.
#include "CorotationalKinematics.hpp"
#include <Eigen/Core>
#include <unsupported/Eigen/MatrixFunctions>
#include <algorithm>
#include <cmath>
#include <iostream>

int main() {
  using vumat::pd::Tensor3;
  const Tensor3 identity{1, 0, 0, 0, 1, 0, 0, 0, 1};
  double maximum = 0.0;
  for (const auto &axis :
       {Eigen::Vector3d(1, 0, 0), Eigen::Vector3d(0, 1, 0), Eigen::Vector3d(0, 0, 1),
        Eigen::Vector3d(1, 2, 3).normalized().eval()}) {
    for (double angle : {0.0, 1e-10, 0.01, 0.5, 3.0, -0.5}) {
      const auto w = axis * angle;
      Eigen::Matrix3d spin;
      spin << 0, -w.z(), w.y(), w.z(), 0, -w.x(), -w.y(), w.x(), 0;
      Tensor3 next = identity;
      // dt=1, F_n=I, V_n=I produces L=spin and material spin=spin.
      // This isolates exp(spin); it is not a finite-motion host validation.
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
          next[3 * i + j] += spin(i, j);
      const auto result =
          vumat::pd::updateCorotationalKinematics(identity, next, identity, identity, 1.0);
      const Eigen::Matrix3d reference = spin.exp();
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
          maximum = std::max(maximum, std::abs(result.rotationNP1[3 * i + j] - reference(i, j)));
    }
  }
  std::cout << "Eigen matrix-exp comparison: 24 rotations, max absolute error=" << maximum << '\n';
  return maximum < 2e-13 ? 0 : 1;
}
