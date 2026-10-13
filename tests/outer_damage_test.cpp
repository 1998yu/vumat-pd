#include "PdSimpleDamage.hpp"
#include "SimpleDamageModel.hpp"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
bool close(double a, double b) { return std::abs(a - b) < 1e-12; }
}

int main() {
  try {
    std::array<double, 9> strain{}, stress{};
    stress.fill(20.0);
    const std::vector<double> history{0.0, 0.15};
    vumat::SimpleDamageConfig config;
    vumat::SimpleDamageModel disabled(config);
    const auto intact = disabled.update(strain, history, {});
    require(intact.damage == 0.0 && disabled.degradeStress(stress, intact) == stress,
            "Disabled outer damage must preserve constitutive stress.");

    config.mode = "eqps_threshold";
    config.initiation = 0.10;
    config.failure = 0.20;
    config.deletionEnabled = true;
    vumat::SimpleDamageModel plastic(config);
    const auto half = plastic.update(strain, history, {});
    require(close(half.damage, 0.5), "Plastic threshold must give half degradation.");
    const auto degraded = plastic.degradeStress(stress, half);
    require(close(degraded[0], 10.0) && stress[0] == 20.0 && history[1] == 0.15,
            "Outer degradation must not overwrite constitutive stress or history.");
    const auto unloading = plastic.update(strain, {0.0, 0.05}, half);
    require(close(unloading.damage, half.damage), "Point damage must not heal.");
    const auto failed = plastic.update(strain, {0.0, 0.30}, unloading);
    require(failed.deleted && plastic.degradeStress(stress, failed)[0] == 0.0,
            "Failure state must suppress the effective stress.");

    config.mode = "max_principal_strain";
    vumat::SimpleDamageModel principal(config);
    strain[1] = strain[3] = 0.15;
    require(close(principal.update(strain, {}, {}).damage, 0.5),
            "Principal-strain mode must evaluate the full symmetric tensor.");
    config.failure = config.initiation;
    bool rejected = false;
    try { vumat::SimpleDamageModel(config).update(strain, {}, {}); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "Invalid threshold range must be rejected.");

    vumat::pd::ParticleBlock block;
    block.referencePosition = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
    block.displacement = {{0.0, 0.0, 0.0}, {0.10, 0.0, 0.0}};
    block.volume = {1.0, 2.0};
    block.neighbors = {{1}, {0}};
    std::vector<vumat::pd::Tensor3> strains(2), stresses(2);
    stresses[0].fill(20.0); stresses[1].fill(30.0);
    std::vector<std::vector<double>> states(2);
    config.mode = "critical_stretch";
    config.initiation = 0.05;
    config.failure = 0.0;
    vumat::pd::PdSimpleDamage bonds(config);
    vumat::pd::PdDamageState bondState;
    bonds.initialize(block, bondState);
    const auto broken = bonds.update(block, strains, stresses, states, bondState);
    require(broken.bondDamage[0][0] == 1.0 && broken.bondDamage[1][0] == 1.0 &&
            broken.damageIndex[0] == 1.0 && broken.damageIndex[1] == 1.0,
            "Critical tensile stretch must break both directed bonds.");
    require(broken.effectiveStress == stresses && bondState.pointDamageN[0].deleted,
            "Bond failure must retain material stress and record the failure state.");
    block.displacement[1] = {0.0, 0.0, 0.0};
    require(bonds.update(block, strains, stresses, states, bondState).bondDamage == broken.bondDamage,
            "Broken bonds must not heal on unloading.");
    config.failure = 0.02;
    vumat::pd::PdSimpleDamage compression(config);
    compression.initialize(block, bondState);
    block.displacement[1] = {-0.03, 0.0, 0.0};
    require(compression.update(block, strains, stresses, states, bondState).bondDamage[0][0] == 1.0,
            "Configured compression threshold must be honored.");
    std::cout << "PASS: optional outer damage modes, irreversibility and material-state separation.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
