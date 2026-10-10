#pragma once

#include "VumatTypes.hpp"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace vumat {

class DynamicLibrary {
public:
  explicit DynamicLibrary(const std::string& path);
  ~DynamicLibrary();

  DynamicLibrary(const DynamicLibrary&) = delete;
  DynamicLibrary& operator=(const DynamicLibrary&) = delete;

  void* symbol(const std::string& name) const;

private:
  void* handle_ = nullptr;
};

class VumatMaterialAdapter {
public:
  explicit VumatMaterialAdapter(VumatMaterialConfig config);

  VumatPointResult updatePoint(const VumatPointInput& input) const;
  VumatBlockResult updateBlock(const VumatBlockInput& input) const;
  const VumatBlockResult& updateFixedBlockCached(const VumatBlockInput& input) const;

private:
  using VumatCall = void (*)(
      int*, int*, int*, int*, int*, int*, int*,
      double*, double*, double*, char*,
      double*, double*, double*, double*, double*, double*,
      double*, double*, double*, double*, double*, double*, double*,
      double*, double*, double*, double*, double*, double*, double*,
      double*, double*);

  static int fortranIndex(int block, int component, int nblock);

  struct WorkBuffer {
    std::vector<double> coordMp;
    std::vector<double> charLength;
    std::vector<double> props;
    std::vector<double> density;
    std::vector<double> strainInc;
    std::vector<double> relSpinInc;
    std::vector<double> tempOld;
    std::vector<double> stretchOld;
    std::vector<double> defgradOld;
    std::vector<double> fieldOld;
    std::vector<double> stressOld;
    std::vector<double> stateOld;
    std::vector<double> enerInternOld;
    std::vector<double> enerInelasOld;
    std::vector<double> tempNew;
    std::vector<double> stretchNew;
    std::vector<double> defgradNew;
    std::vector<double> fieldNew;
    std::vector<double> stressNew;
    std::vector<double> stateNew;
    std::vector<double> enerInternNew;
    std::vector<double> enerInelasNew;
  };

  VumatMaterialConfig config_;
  std::shared_ptr<DynamicLibrary> library_;
  VumatCall call_ = nullptr;
  mutable WorkBuffer work_;
  mutable VumatBlockResult cachedResult_;
};

} // namespace vumat
