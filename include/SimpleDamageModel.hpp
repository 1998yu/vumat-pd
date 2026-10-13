#pragma once

#include <array>
#include <string>
#include <vector>

namespace vumat {

struct SimpleDamageConfig {
  // Point modes: none, eqps_threshold, max_principal_strain.
  // PD correspondence mode: critical_stretch, where initiation is tensile
  // critical stretch and failure optionally stores positive compression stretch.
  std::string mode = "none";
  double initiation = 0.0;
  double failure = 1.0;
  int eqpsStateIndex = 1; // zero-based; JC eqps is state variable 2.
  bool deletionEnabled = false;
};

struct SimpleDamageState {
  double damage = 0.0;
  bool deleted = false;
};

class SimpleDamageModel {
public:
  explicit SimpleDamageModel(SimpleDamageConfig config);

  SimpleDamageState update(const std::array<double, 9>& strainFull,
                           const std::vector<double>& stateNew,
                           SimpleDamageState previous) const;

  std::array<double, 9> degradeStress(const std::array<double, 9>& stress,
                                      const SimpleDamageState& state) const;

private:
  SimpleDamageConfig config_;
};

} // namespace vumat
