#include "Geometry.hpp"
#include "PdCorrespondenceSkeleton.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

int main() {
  auto block = vumat::demo::makeParticlePatch(9, 9, 9, 1.0, 2.0);
  vumat::pd::PdCorrespondenceSkeleton pd(2.0);
  vumat::pd::CorrespondenceState state;
  pd.initialize(block, state);
  double volume = 0.0;
  for (double v : block.volume)
    volume += v;
  if (std::abs(volume - 512.0) > 1e-12)
    return 1;
  // Affine completeness must hold even for truncated boundary neighborhoods.
  for (size_t i = 0; i < block.volume.size(); ++i) {
    const auto &x = block.referencePosition[i];
    block.displacement[i] = {0.02 * x[0] + 0.03 * x[1], -0.01 * x[1], 0.04 * x[2]};
  }
  const vumat::pd::Tensor3 expected{1.02, 0.03, 0, 0, 0.99, 0, 0, 0, 1.04};
  auto f = pd.computeDeformationGradients(block, state);
  for (const auto &tensor : f)
    for (size_t c = 0; c < 9; ++c)
      if (std::abs(tensor[c] - expected[c]) > 1e-12)
        return 2;
  // A local diagnostic of the unstabilized gradient, NOT proof of a global
  // zero-energy eigenmode: symmetric neighborhoods cannot observe this pattern.
  for (int i = 0; i < 9; ++i)
    for (int j = 0; j < 9; ++j)
      for (int k = 0; k < 9; ++k)
        block.displacement[(i * 9 + j) * 9 + k] = {((i + j + k) % 2 ? 1.0 : -1.0) * 0.01, 0, 0};
  f = pd.computeDeformationGradients(block, state);
  double error = 0.0;
  for (int i = 3; i <= 5; ++i)
    for (int j = 3; j <= 5; ++j)
      for (int k = 3; k <= 5; ++k)
        for (int c = 0; c < 9; ++c)
          error = std::max(error, std::abs(f[(i * 9 + j) * 9 + k][c] - (c % 4 == 0 ? 1.0 : 0.0)));
  if (error > 1e-12)
    return 3;
  std::cout << "PASS volume and affine completeness; unresolved alternating displacement is "
               "invisible at 27 interior nodes (max |F-I|="
            << error << ")\n";
}
