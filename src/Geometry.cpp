#include "Geometry.hpp"
#include "PdCorrespondenceSkeleton.hpp"

#include <cstddef>

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

} // namespace demo
} // namespace vumat
