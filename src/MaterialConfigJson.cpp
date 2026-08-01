#include "MaterialConfig.hpp"

#include "SimpleJson.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace vumat {
namespace {

std::string joinPath(const std::string& left, const std::string& right) {
  if (left.empty())
    return right;
  const char last = left.back();
  if (last == '/' || last == '\\')
    return left + right;
#ifdef _WIN32
  return left + "\\" + right;
#else
  return left + "/" + right;
#endif
}

bool hasPathSeparator(const std::string& path) {
  return path.find('/') != std::string::npos || path.find('\\') != std::string::npos;
}

bool isAbsolutePath(const std::string& path) {
  if (path.empty())
    return false;
  if (path[0] == '/' || path[0] == '\\')
    return true;
  return path.size() > 2 && std::isalpha(static_cast<unsigned char>(path[0])) &&
         path[1] == ':';
}

std::string dynamicLibraryName(const std::string& plugin) {
  if (hasPathSeparator(plugin) || plugin.find('.') != std::string::npos)
    return plugin;
#ifdef _WIN32
  return plugin + ".dll";
#elif defined(__APPLE__)
  return plugin + ".dylib";
#else
  return plugin + ".so";
#endif
}

std::string resolvePath(const std::string& pluginDirectory, const std::string& path) {
  if (path.empty())
    return path;
  if (isAbsolutePath(path))
    return path;
  return joinPath(pluginDirectory, path);
}

} // namespace

VumatMaterialConfig loadMaterialConfigFromJson(const std::string& configPath,
                                               const std::string& pluginDirectory) {
  const json::Value root = json::loadFile(configPath);
  const std::string schemaVersion = json::getString(root, "schemaVersion", false, "1.0");
  if (schemaVersion != "1.0")
    throw std::runtime_error("Unsupported VUMAT material schemaVersion: " + schemaVersion);

  VumatMaterialConfig config;
  config.name = json::getString(root, "name");
  config.cmname = json::getString(root, "cmname");
  config.symbolName = json::getString(root, "symbolName", false,
                                     json::getString(root, "symbol", false,
                                                     "vumat_adapter_call"));
  config.ndir = json::getInt(root, "ndir", false, 3);
  config.nshr = json::getInt(root, "nshr", false, 3);
  config.nstatev = json::getInt(root, "nstatev");
  config.nfieldv = json::getInt(root, "nfieldv", false, 1);
  config.props = json::getDoubleArray(root, "props");
  config.density = json::getDouble(root, "density");
  if (config.ndir != 3 || config.nshr != 3)
    throw std::runtime_error("The current VUMAT adapter requires ndir=3 and nshr=3.");
  if (config.nstatev < 0 || config.nfieldv < 0 || config.density <= 0.0)
    throw std::runtime_error("Invalid VUMAT dimensions or density in: " + configPath);

  const std::string explicitPath = json::getString(root, "pluginPath", false, "");
  if (!explicitPath.empty()) {
    config.pluginPath = resolvePath(pluginDirectory, explicitPath);
  } else {
    const std::string plugin = json::getString(root, "plugin", false, "");
    if (plugin.empty())
      throw std::runtime_error("Material config requires either pluginPath or plugin.");
    config.pluginPath = resolvePath(pluginDirectory, dynamicLibraryName(plugin));
  }

  return config;
}

} // namespace vumat
