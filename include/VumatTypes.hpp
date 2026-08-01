#pragma once

#include <array>
#include <string>
#include <vector>

namespace vumat {

struct VumatMaterialConfig {
  std::string name;
  std::string cmname;
  std::string pluginPath;
  std::string symbolName = "vumat_adapter_call";
  int ndir = 3;
  int nshr = 3;
  int nstatev = 0;
  int nfieldv = 1;
  std::vector<double> props;
  double density = 0.0;
};

struct VumatPointInput {
  double dt = 0.0;
  double stepTime = 0.0;
  double totalTime = 0.0;
  double charLength = 1.0;
  double temperature = 0.0;
  std::array<double, 9> strainRateFull{};
  std::array<double, 9> relSpinIncFull{};
  std::array<double, 9> strainFullN{};
  std::array<double, 9> stressFullN{};
  std::array<double, 9> stretchOld{1.0, 0.0, 0.0,
                                  0.0, 1.0, 0.0,
                                  0.0, 0.0, 1.0};
  std::array<double, 9> stretchNew{1.0, 0.0, 0.0,
                                  0.0, 1.0, 0.0,
                                  0.0, 0.0, 1.0};
  std::array<double, 9> defgradOld{1.0, 0.0, 0.0,
                                   0.0, 1.0, 0.0,
                                   0.0, 0.0, 1.0};
  std::array<double, 9> defgradNew{1.0, 0.0, 0.0,
                                   0.0, 1.0, 0.0,
                                   0.0, 0.0, 1.0};
  std::vector<double> stateOld;
  double internalEnergyOld = 0.0;
  double inelasticEnergyOld = 0.0;
};

struct VumatPointResult {
  std::array<double, 9> stressFullNew{};
  std::vector<double> stateNew;
  double temperatureNew = 0.0;
  double internalEnergyNew = 0.0;
  double inelasticEnergyNew = 0.0;
};

struct VumatBlockInput {
  double dt = 0.0;
  double stepTime = 0.0;
  double totalTime = 0.0;
  std::vector<double> charLength;
  std::vector<double> temperature;
  std::vector<std::array<double, 9>> strainRateFull;
  std::vector<std::array<double, 9>> relSpinIncFull;
  std::vector<std::array<double, 9>> strainFullN;
  std::vector<std::array<double, 9>> stressFullN;
  std::vector<std::array<double, 9>> stretchOld;
  std::vector<std::array<double, 9>> stretchNew;
  std::vector<std::array<double, 9>> defgradOld;
  std::vector<std::array<double, 9>> defgradNew;
  std::vector<std::vector<double>> stateOld;
  std::vector<double> internalEnergyOld;
  std::vector<double> inelasticEnergyOld;
};

struct VumatBlockResult {
  std::vector<std::array<double, 9>> stressFullNew;
  std::vector<std::vector<double>> stateNew;
  std::vector<double> temperatureNew;
  std::vector<double> internalEnergyNew;
  std::vector<double> inelasticEnergyNew;
};

} // namespace vumat
