#pragma once

#include "PdTypes.hpp"

#include <string>
#include <vector>

namespace vumat {
namespace demo {

std::vector<size_t> selectBoundaryRegion(const vumat::pd::ParticleBlock& block,
                                         const std::string& side,
                                         double width);
size_t selectClosestParticle(const vumat::pd::ParticleBlock& block,
                             const std::vector<size_t>& candidates,
                             const vumat::pd::Vec3& target);

} // namespace demo
} // namespace vumat
