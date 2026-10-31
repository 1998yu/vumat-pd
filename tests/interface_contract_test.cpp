#include "VumatMaterialAdapter.hpp"
#include "VumatPdCorrespondenceMaterial.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using Tensor = std::array<double, 9>;
void close(double a, double b) {
  if (std::abs(a - b) > 1e-10 || !std::isfinite(a))
    throw std::runtime_error("ABI value mismatch: " + std::to_string(a) + " vs " +
                             std::to_string(b));
}
template <class F> void rejects(F f) {
  bool rejected = false;
  try {
    f();
  } catch (const std::runtime_error &) {
    rejected = true;
  }
  if (!rejected)
    throw std::runtime_error("Invalid input was silently accepted");
}
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    vumat::VumatMaterialConfig cfg;
    cfg.pluginPath = argv[1];
    cfg.cmname = "PROBE";
    cfg.props = {1};
    cfg.density = 7800;
    cfg.nstatev = 39;
    vumat::VumatMaterialAdapter adapter(cfg);
    // Literal expectations do not call the production packing functions.
    const Tensor rate{1, 6, 5, 6, 2, 4, 5, 4, 3};
    const Tensor oldF{11, 12, 13, 21, 22, 23, 31, 32, 33};
    const Tensor newF{41, 42, 43, 51, 52, 53, 61, 62, 63};
    const Tensor oldStretch{1, 6, 5, 6, 2, 4, 5, 4, 3};
    const Tensor newStretch{7, 12, 11, 12, 8, 10, 11, 10, 9};
    const Tensor spin{0, 0.1, -0.2, -0.1, 0, 0.3, 0.2, -0.3, 0};
    const double expectedOldF[9] = {11, 22, 33, 12, 23, 31, 21, 32, 13};
    const double expectedNewF[9] = {41, 52, 63, 42, 53, 61, 51, 62, 43};
    const double expectedSpin[3] = {0.3, 0.2, 0.1};
    for (size_t n : {1u, 255u, 256u, 257u, 513u}) {
      vumat::VumatBlockInput in;
      in.dt = 0.125;
      in.strainRateFull.assign(n, rate);
      in.relSpinIncFull.assign(n, spin);
      in.stretchOld.assign(n, oldStretch);
      in.stretchNew.assign(n, newStretch);
      in.defgradOld.assign(n, oldF);
      in.defgradNew.assign(n, newF);
      in.strainFullN.assign(n, Tensor{});
      in.stressFullN.assign(n, Tensor{});
      in.temperature.assign(n, 300);
      in.charLength.resize(n);
      in.stateOld.assign(n, std::vector<double>(39, 0));
      in.internalEnergyOld.assign(n, 10);
      in.inelasticEnergyOld.assign(n, 20);
      for (size_t i = 0; i < n; ++i) {
        in.stateOld[i][33] = i;
        in.charLength[i] = 2 + i * 0.001;
      }
      for (int step = 0; step < 2; ++step) {
        in.stepTime = step * in.dt;
        in.totalTime = 2 + step * in.dt;
        const auto result = adapter.updateBlock(in);
        if (result.stateNew.size() != n || result.stressFullNew.size() != n)
          throw std::runtime_error("Padding leaked into output");
        for (size_t i = 0; i < n; ++i) {
          // Independent full-tensor expectation: yz=4, xz=5, xy=6 weights.
          const Tensor expectedStress{1, 36, 25, 36, 4, 16, 25, 16, 9};
          for (int c = 0; c < 9; ++c) {
            close(result.stressFullNew[i][c], (step + 1) * 0.125 * expectedStress[c]);
            close(result.stateNew[i][c], expectedOldF[c]);
            close(result.stateNew[i][9 + c], expectedNewF[c]);
          }
          for (int c = 0; c < 6; ++c) {
            close(result.stateNew[i][18 + c], c + 1);
            close(result.stateNew[i][24 + c], c + 7);
          }
          for (int c = 0; c < 3; ++c)
            close(result.stateNew[i][30 + c], expectedSpin[c]);
          close(result.stateNew[i][33], i + step + 1);
          close(result.stateNew[i][34], 7800);
          close(result.stateNew[i][35], 2 + i * 0.001);
          close(result.stateNew[i][36], n > 256 ? 256 : n);
          close(result.stateNew[i][37], step * in.dt);
          close(result.stateNew[i][38], 2 + step * in.dt);
          close(result.temperatureNew[i], 300 + (step + 1) * (i + 0.125) + step);
          close(result.internalEnergyNew[i], 10 + (step + 1) * 21 * 0.125);
          close(result.inelasticEnergyNew[i], 20 + (step + 1) * 42 * 0.125);
        }
        in.stressFullN = result.stressFullNew;
        in.stateOld = result.stateNew;
        in.temperature = result.temperatureNew;
        in.internalEnergyOld = result.internalEnergyNew;
        in.inelasticEnergyOld = result.inelasticEnergyNew;
      }
      auto bad = in;
      bad.stretchOld.resize(n == 1 ? 2 : n - 1);
      rejects([&] { adapter.updateBlock(bad); });
      bad = in;
      bad.relSpinIncFull.resize(n == 1 ? 2 : n - 1);
      rejects([&] { adapter.updateBlock(bad); });
      bad = in;
      bad.dt = std::numeric_limits<double>::quiet_NaN();
      rejects([&] { adapter.updateBlock(bad); });
      rejects([&] { adapter.updateFixedBlockCached(bad); });
    }
    vumat::pd::VumatPdCorrespondenceConfig host;
    host.material = cfg;
    host.kinematicsMode = vumat::pd::KinematicsMode::LegacySmallStrain;
    rejects([&] { vumat::pd::VumatPdCorrespondenceMaterial bridge(host); });
    host.kinematicsMode = vumat::pd::KinematicsMode::Corotational;
    {
      vumat::pd::VumatPdCorrespondenceMaterial bridge(host);
      const Tensor identity{1, 0, 0, 0, 1, 0, 0, 0, 1};
      std::vector<Tensor> gradients(257, identity);
      vumat::pd::MaterialState state;
      bridge.initialize(gradients, state);
      for (size_t i = 0; i < 257; ++i)
        state.vumatStateN[i][33] = i;
      for (int step = 1; step <= 2; ++step) {
        auto output = bridge.update(gradients, state, 0.125, step * 0.125, step * 0.125);
        if (output.rawStress.size() != 257 || state.vumatStateN.size() != 257)
          throw std::runtime_error("Host padded state leaked");
        for (size_t i = 0; i < 257; ++i) {
          close(state.vumatStateN[i][33], i + step);
          close(state.vumatStateN[i][36], 256);
        }
      }
    }
    host.material.stressFrame = "spatial";
    rejects([&] { vumat::pd::VumatPdCorrespondenceMaterial bridge(host); });
    std::cout << "PASS actual Fortran bridge: distinct shear/F/stretch/spin, two-step history, "
                 "1/255/256/257/513 rows; malformed arrays and unsupported host modes rejected\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
