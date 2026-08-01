#pragma once

#include "VumatTypes.hpp"

namespace vumat {

VumatMaterialConfig loadMaterialConfigFromJson(const std::string& configPath,
                                               const std::string& pluginDirectory = "");

} // namespace vumat
