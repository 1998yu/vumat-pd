#pragma once

#include "PdTypes.hpp"
#include "SimpleDamageModel.hpp"

#include <vector>

namespace vumat {
namespace pd {

struct PdDamageState {
  std::vector<std::vector<double>> bondDamageN;
  std::vector<double> damageIndexN;
  std::vector<SimpleDamageState> pointDamageN;
};

struct PdDamageResult {
  std::vector<std::vector<double>> bondDamage;
  std::vector<double> damageIndex;
  std::vector<Tensor3> effectiveStress;
};

class PdSimpleDamage {
public:
  explicit PdSimpleDamage(SimpleDamageConfig config);
  void initialize(const ParticleBlock& block, PdDamageState& state) const;
  PdDamageResult update(const ParticleBlock& block,
                        const std::vector<Tensor3>& strain,
                        const std::vector<Tensor3>& rawStress,
                        const std::vector<std::vector<double>>& vumatState,
                        PdDamageState& state) const;

private:
  SimpleDamageConfig config_;
  SimpleDamageModel pointModel_;
};

} // namespace pd
} // namespace vumat
