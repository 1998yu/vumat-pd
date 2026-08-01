#include "VumatMaterialAdapter.hpp"

#include "TensorVoigtMap.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace vumat {

DynamicLibrary::DynamicLibrary(const std::string& path) {
#ifdef _WIN32
  handle_ = LoadLibraryA(path.c_str());
  if (!handle_)
    throw std::runtime_error("Failed to load VUMAT plugin: " + path);
#else
  handle_ = dlopen(path.c_str(), RTLD_NOW);
  if (!handle_)
    throw std::runtime_error(std::string("Failed to load VUMAT plugin: ") + dlerror());
#endif
}

DynamicLibrary::~DynamicLibrary() {
#ifdef _WIN32
  if (handle_)
    FreeLibrary(static_cast<HMODULE>(handle_));
#else
  if (handle_)
    dlclose(handle_);
#endif
}

void* DynamicLibrary::symbol(const std::string& name) const {
#ifdef _WIN32
  void* ptr = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name.c_str()));
#else
  void* ptr = dlsym(handle_, name.c_str());
#endif
  if (!ptr)
    throw std::runtime_error("Failed to resolve symbol in VUMAT plugin: " + name);
  return ptr;
}

VumatMaterialAdapter::VumatMaterialAdapter(VumatMaterialConfig config)
    : config_(std::move(config)),
      library_(std::make_shared<DynamicLibrary>(config_.pluginPath)) {
  if (config_.ndir != 3 || config_.nshr != 3)
    throw std::runtime_error("The minimal adapter currently supports 3D VUMAT only: ndir=3, nshr=3.");
  if (config_.nstatev < 0)
    throw std::runtime_error("nstatev must be non-negative.");
  if (config_.props.empty())
    throw std::runtime_error("VUMAT props cannot be empty.");

  call_ = reinterpret_cast<VumatCall>(library_->symbol(config_.symbolName));
}

int VumatMaterialAdapter::fortranIndex(int block, int component, int nblock) {
  return block + nblock * component;
}

size_t requireConsistentBlockSize(const VumatBlockInput& input, int nstatev) {
  const size_t nblock = input.strainRateFull.size();
  if (nblock == 0)
    throw std::runtime_error("VumatBlockInput requires at least one material point.");

  const auto requireSize = [nblock](size_t actual, const std::string& name) {
    if (actual != nblock)
      throw std::runtime_error(name + " size does not match VumatBlockInput block size.");
  };

  requireSize(input.charLength.size(), "charLength");
  requireSize(input.temperature.size(), "temperature");
  requireSize(input.strainFullN.size(), "strainFullN");
  requireSize(input.stressFullN.size(), "stressFullN");
  requireSize(input.defgradOld.size(), "defgradOld");
  requireSize(input.defgradNew.size(), "defgradNew");
  requireSize(input.stateOld.size(), "stateOld");
  requireSize(input.internalEnergyOld.size(), "internalEnergyOld");
  requireSize(input.inelasticEnergyOld.size(), "inelasticEnergyOld");

  for (size_t block = 0; block < nblock; ++block) {
    if (static_cast<int>(input.stateOld[block].size()) != nstatev)
      throw std::runtime_error("stateOld entry size does not match nstatev.");
  }

  return nblock;
}

VumatPointResult VumatMaterialAdapter::updatePoint(const VumatPointInput& input) const {
  VumatBlockInput block;
  block.dt = input.dt;
  block.stepTime = input.stepTime;
  block.totalTime = input.totalTime;
  block.charLength.push_back(input.charLength);
  block.temperature.push_back(input.temperature);
  block.strainRateFull.push_back(input.strainRateFull);
  block.relSpinIncFull.push_back(input.relSpinIncFull);
  block.strainFullN.push_back(input.strainFullN);
  block.stressFullN.push_back(input.stressFullN);
  block.stretchOld.push_back(input.stretchOld);
  block.stretchNew.push_back(input.stretchNew);
  block.defgradOld.push_back(input.defgradOld);
  block.defgradNew.push_back(input.defgradNew);
  block.stateOld.push_back(input.stateOld);
  block.internalEnergyOld.push_back(input.internalEnergyOld);
  block.inelasticEnergyOld.push_back(input.inelasticEnergyOld);

  auto result = updateBlock(block);
  if (result.stressFullNew.size() != 1 || result.stateNew.size() != 1)
    throw std::runtime_error("Internal error: single-point VUMAT update returned an invalid block.");

  VumatPointResult point;
  point.stressFullNew = result.stressFullNew[0];
  point.stateNew = std::move(result.stateNew[0]);
  point.temperatureNew = result.temperatureNew[0];
  point.internalEnergyNew = result.internalEnergyNew[0];
  point.inelasticEnergyNew = result.inelasticEnergyNew[0];
  return point;
}

VumatBlockResult VumatMaterialAdapter::updateBlock(const VumatBlockInput& input) const {
  if (input.dt <= 0.0)
    throw std::runtime_error("VUMAT update requires dt > 0.");

  constexpr size_t maxLegacyBlockSize = 256;
  if (input.strainRateFull.size() > maxLegacyBlockSize) {
    VumatBlockResult combined;
    const auto appendFirst = [](auto& destination, auto&& source, size_t count) {
      destination.insert(destination.end(),
                         std::make_move_iterator(source.begin()),
                         std::make_move_iterator(source.begin() +
                                                 static_cast<std::ptrdiff_t>(count)));
    };
    for (size_t begin = 0; begin < input.strainRateFull.size();
         begin += maxLegacyBlockSize) {
      const size_t end = std::min(begin + maxLegacyBlockSize,
                                  input.strainRateFull.size());
      VumatBlockInput chunk;
      chunk.dt = input.dt;
      chunk.stepTime = input.stepTime;
      chunk.totalTime = input.totalTime;
      const auto copyRange = [begin, end](const auto& source, auto& destination) {
        destination.assign(source.begin() + static_cast<std::ptrdiff_t>(begin),
                           source.begin() + static_cast<std::ptrdiff_t>(end));
      };
      copyRange(input.charLength, chunk.charLength);
      copyRange(input.temperature, chunk.temperature);
      copyRange(input.strainRateFull, chunk.strainRateFull);
      if (!input.relSpinIncFull.empty())
        copyRange(input.relSpinIncFull, chunk.relSpinIncFull);
      copyRange(input.strainFullN, chunk.strainFullN);
      copyRange(input.stressFullN, chunk.stressFullN);
      if (!input.stretchOld.empty())
        copyRange(input.stretchOld, chunk.stretchOld);
      if (!input.stretchNew.empty())
        copyRange(input.stretchNew, chunk.stretchNew);
      copyRange(input.defgradOld, chunk.defgradOld);
      copyRange(input.defgradNew, chunk.defgradNew);
      copyRange(input.stateOld, chunk.stateOld);
      copyRange(input.internalEnergyOld, chunk.internalEnergyOld);
      copyRange(input.inelasticEnergyOld, chunk.inelasticEnergyOld);
      const size_t validCount = end - begin;
      // Some legacy fixed-form VUMATs retain block-local storage assumptions.
      // Keep every batched call at a constant width and discard padded results;
      // duplicating the last valid point avoids introducing an artificial state.
      const auto padToLegacyBlock = [](auto& values) {
        while (values.size() < maxLegacyBlockSize)
          values.push_back(values.back());
      };
      padToLegacyBlock(chunk.charLength);
      padToLegacyBlock(chunk.temperature);
      padToLegacyBlock(chunk.strainRateFull);
      if (!chunk.relSpinIncFull.empty())
        padToLegacyBlock(chunk.relSpinIncFull);
      padToLegacyBlock(chunk.strainFullN);
      padToLegacyBlock(chunk.stressFullN);
      if (!chunk.stretchOld.empty())
        padToLegacyBlock(chunk.stretchOld);
      if (!chunk.stretchNew.empty())
        padToLegacyBlock(chunk.stretchNew);
      padToLegacyBlock(chunk.defgradOld);
      padToLegacyBlock(chunk.defgradNew);
      padToLegacyBlock(chunk.stateOld);
      padToLegacyBlock(chunk.internalEnergyOld);
      padToLegacyBlock(chunk.inelasticEnergyOld);
      VumatBlockResult result = updateBlock(chunk);
      appendFirst(combined.stressFullNew, std::move(result.stressFullNew), validCount);
      appendFirst(combined.stateNew, std::move(result.stateNew), validCount);
      appendFirst(combined.temperatureNew, std::move(result.temperatureNew), validCount);
      appendFirst(combined.internalEnergyNew, std::move(result.internalEnergyNew), validCount);
      appendFirst(combined.inelasticEnergyNew, std::move(result.inelasticEnergyNew), validCount);
    }
    return combined;
  }

  int nblock = static_cast<int>(requireConsistentBlockSize(input, config_.nstatev));
  int ndir = config_.ndir;
  int nshr = config_.nshr;
  int nstatev = config_.nstatev;
  int nfieldv = config_.nfieldv;
  int nprops = static_cast<int>(config_.props.size());
  int lanneal = 0;
  double stepTime = input.stepTime;
  double totalTime = input.totalTime;
  double dt = input.dt;

  constexpr int spatialDim = 3;
  const int ntens = ndir + nshr;
  const int defgradSize = ndir + nshr + nshr;
  const int fieldSize = std::max(1, nfieldv);
  const int stateSize = std::max(1, nstatev);

  std::array<char, 80> cmname{};
  cmname.fill(' ');
  const size_t copyCount = std::min(cmname.size(), config_.cmname.size());
  std::memcpy(cmname.data(), config_.cmname.data(), copyCount);

  WorkBuffer& work = work_;
  work.coordMp.assign(static_cast<size_t>(nblock * spatialDim), 0.0);
  work.charLength.assign(input.charLength.begin(), input.charLength.end());
  work.props = config_.props;
  work.density.assign(static_cast<size_t>(nblock), config_.density);
  work.strainInc.assign(static_cast<size_t>(nblock * ntens), 0.0);
  work.relSpinInc.assign(static_cast<size_t>(nblock * nshr), 0.0);
  work.tempOld.assign(input.temperature.begin(), input.temperature.end());
  work.stretchOld.assign(static_cast<size_t>(nblock * ntens), 0.0);
  work.defgradOld.assign(static_cast<size_t>(nblock * defgradSize), 0.0);
  work.fieldOld.assign(static_cast<size_t>(nblock * fieldSize), 0.0);
  work.stressOld.assign(static_cast<size_t>(nblock * ntens), 0.0);
  work.stateOld.assign(static_cast<size_t>(nblock * stateSize), 0.0);
  work.enerInternOld.assign(input.internalEnergyOld.begin(), input.internalEnergyOld.end());
  work.enerInelasOld.assign(input.inelasticEnergyOld.begin(), input.inelasticEnergyOld.end());

  work.tempNew.assign(input.temperature.begin(), input.temperature.end());
  work.stretchNew.assign(static_cast<size_t>(nblock * ntens), 0.0);
  work.defgradNew.assign(static_cast<size_t>(nblock * defgradSize), 0.0);
  work.fieldNew.assign(static_cast<size_t>(nblock * fieldSize), 0.0);
  work.stressNew.assign(static_cast<size_t>(nblock * ntens), 0.0);
  work.stateNew.assign(static_cast<size_t>(nblock * stateSize), 0.0);
  work.enerInternNew.assign(input.internalEnergyOld.begin(), input.internalEnergyOld.end());
  work.enerInelasNew.assign(input.inelasticEnergyOld.begin(), input.inelasticEnergyOld.end());

  for (int blockIndex = 0; blockIndex < nblock; ++blockIndex) {
    const auto stressVoigt = fullTensorToVumatVoigt(input.stressFullN[static_cast<size_t>(blockIndex)]);
    const auto strainIncVoigt =
        strainRateFullToVumatStrainInc(input.strainRateFull[static_cast<size_t>(blockIndex)], input.dt);
    const auto defgradOldVumat =
        fullTensorToVumatDefgrad(input.defgradOld[static_cast<size_t>(blockIndex)]);
    const auto defgradNewVumat =
        fullTensorToVumatDefgrad(input.defgradNew[static_cast<size_t>(blockIndex)]);
    const std::array<double, 9> identityStretch{
        1.0,0.0,0.0, 0.0,1.0,0.0, 0.0,0.0,1.0};
    const auto stretchOldVoigt = fullTensorToVumatVoigt(
        input.stretchOld.empty() ? identityStretch
                                 : input.stretchOld[static_cast<size_t>(blockIndex)]);
    const auto stretchNewVoigt = fullTensorToVumatVoigt(
        input.stretchNew.empty() ? identityStretch
                                 : input.stretchNew[static_cast<size_t>(blockIndex)]);

    for (int c = 0; c < ntens; ++c) {
      work.stressOld[fortranIndex(blockIndex, c, nblock)] = stressVoigt[static_cast<size_t>(c)];
      work.strainInc[fortranIndex(blockIndex, c, nblock)] = strainIncVoigt[static_cast<size_t>(c)];
      work.stretchOld[fortranIndex(blockIndex, c, nblock)] =
          stretchOldVoigt[static_cast<size_t>(c)];
      work.stretchNew[fortranIndex(blockIndex, c, nblock)] =
          stretchNewVoigt[static_cast<size_t>(c)];
    }
    if (!input.relSpinIncFull.empty()) {
      const auto& spin = input.relSpinIncFull[static_cast<size_t>(blockIndex)];
      const std::array<double, 3> spinVumat{spin[5], spin[6], spin[1]};
      for (int c = 0; c < nshr; ++c)
        work.relSpinInc[fortranIndex(blockIndex, c, nblock)] =
            spinVumat[static_cast<size_t>(c)];
    }
    for (int c = 0; c < defgradSize; ++c) {
      work.defgradOld[fortranIndex(blockIndex, c, nblock)] = defgradOldVumat[static_cast<size_t>(c)];
      work.defgradNew[fortranIndex(blockIndex, c, nblock)] = defgradNewVumat[static_cast<size_t>(c)];
    }
    for (int c = 0; c < nstatev; ++c)
      work.stateOld[fortranIndex(blockIndex, c, nblock)] =
          input.stateOld[static_cast<size_t>(blockIndex)][static_cast<size_t>(c)];
  }

  call_(&nblock, &ndir, &nshr, &nstatev, &nfieldv, &nprops, &lanneal,
        &stepTime, &totalTime, &dt, cmname.data(),
        work.coordMp.data(), work.charLength.data(), work.props.data(), work.density.data(),
        work.strainInc.data(), work.relSpinInc.data(), work.tempOld.data(), work.stretchOld.data(),
        work.defgradOld.data(), work.fieldOld.data(), work.stressOld.data(), work.stateOld.data(),
        work.enerInternOld.data(), work.enerInelasOld.data(), work.tempNew.data(),
        work.stretchNew.data(), work.defgradNew.data(), work.fieldNew.data(), work.stressNew.data(),
        work.stateNew.data(), work.enerInternNew.data(), work.enerInelasNew.data());

  VumatBlockResult result;
  result.stressFullNew.resize(static_cast<size_t>(nblock));
  result.stateNew.resize(static_cast<size_t>(nblock));
  result.temperatureNew.assign(work.tempNew.begin(), work.tempNew.end());
  result.internalEnergyNew.assign(work.enerInternNew.begin(), work.enerInternNew.end());
  result.inelasticEnergyNew.assign(work.enerInelasNew.begin(), work.enerInelasNew.end());

  for (int blockIndex = 0; blockIndex < nblock; ++blockIndex) {
    std::array<double, 6> stressNewVoigt{};
    for (int c = 0; c < ntens; ++c)
      stressNewVoigt[static_cast<size_t>(c)] =
          work.stressNew[fortranIndex(blockIndex, c, nblock)];
    result.stressFullNew[static_cast<size_t>(blockIndex)] =
        vumatVoigtToFullTensor(stressNewVoigt);

    result.stateNew[static_cast<size_t>(blockIndex)].resize(static_cast<size_t>(nstatev));
    for (int c = 0; c < nstatev; ++c)
      result.stateNew[static_cast<size_t>(blockIndex)][static_cast<size_t>(c)] =
          work.stateNew[fortranIndex(blockIndex, c, nblock)];
  }
  return result;
}

} // namespace vumat
