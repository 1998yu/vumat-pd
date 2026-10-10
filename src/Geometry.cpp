#include "Geometry.hpp"
#include "PdCorrespondenceSkeleton.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace vumat {
namespace demo {

vumat::pd::ParticleBlock makeParticlePatch(int nx, int ny, int nz, double spacing,
                                           double horizon) {
  vumat::pd::ParticleBlock block;
  const double yOffset = 0.5 * static_cast<double>(ny - 1) * spacing;
  const double zOffset = 0.5 * static_cast<double>(nz - 1) * spacing;
  const double volume = spacing * spacing * spacing;

  for (int i = 0; i < nx; ++i) {
    for (int j = 0; j < ny; ++j) {
      for (int k = 0; k < nz; ++k) {
        block.referencePosition.push_back({static_cast<double>(i) * spacing,
                                           static_cast<double>(j) * spacing - yOffset,
                                           static_cast<double>(k) * spacing - zOffset});
        block.displacement.push_back({0.0, 0.0, 0.0});
        block.volume.push_back(volume * ((i==0 || i==nx-1)?0.5:1.0) * ((j==0 || j==ny-1)?0.5:1.0) * ((k==0 || k==nz-1)?0.5:1.0));
      }
    }
  }

  block.neighbors = vumat::pd::buildNeighborList(block.referencePosition, horizon);
  return block;
}

vumat::pd::ParticleBlock makeCylinder(double length, double diameter, double spacing,
                                      double horizon, double initialGap) {
  if (length <= 0.0 || diameter <= 0.0 || spacing <= 0.0 || horizon <= 0.0 ||
      initialGap < 0.0)
    throw std::runtime_error("Cylinder geometry requires positive dimensions and nonnegative gap.");

  vumat::pd::ParticleBlock block;
  const double radius = 0.5 * diameter;
  const double volume = spacing * spacing * spacing;
  const int nx = static_cast<int>(std::floor(length / spacing + 0.5)) + 1;
  const int nr = static_cast<int>(std::floor(radius / spacing + 1.0e-12));
  for (int i = 0; i < nx; ++i) {
    const double x = initialGap + std::min(length, static_cast<double>(i) * spacing);
    for (int j = -nr; j <= nr; ++j) {
      for (int k = -nr; k <= nr; ++k) {
        const double y = static_cast<double>(j) * spacing;
        const double z = static_cast<double>(k) * spacing;
        if (y * y + z * z <= radius * radius + 1.0e-12 * diameter * diameter) {
          block.referencePosition.push_back({x, y, z});
          block.displacement.push_back({0.0, 0.0, 0.0});
          block.volume.push_back(volume);
        }
      }
    }
  }
  if (block.referencePosition.size() < 32)
    throw std::runtime_error("Cylinder discretisation is too coarse for 3D correspondence.");
  block.neighbors = vumat::pd::buildNeighborList(block.referencePosition, horizon);
  return block;
}

} // namespace demo
} // namespace vumat
