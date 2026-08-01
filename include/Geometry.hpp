#pragma once

#include "PdTypes.hpp"

namespace vumat {
namespace demo {

vumat::pd::ParticleBlock makeParticlePatch(int nx, int ny, int nz, double spacing,
                                           double horizon);
vumat::pd::ParticleBlock makeCylinder(double length, double diameter, double spacing,
                                      double horizon, double initialGap = 0.0);

} // namespace demo
} // namespace vumat
