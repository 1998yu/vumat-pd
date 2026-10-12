#pragma once

#include "PdTypes.hpp"

namespace vumat {
namespace demo {

vumat::pd::ParticleBlock makeParticlePatch(int nx, int ny, int nz, double spacing,
                                           double horizon);

} // namespace demo
} // namespace vumat
