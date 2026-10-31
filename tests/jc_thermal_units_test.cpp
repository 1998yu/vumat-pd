#include "MaterialConfig.hpp"
#include "VumatMaterialAdapter.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

vumat::VumatPointResult plasticStep(vumat::VumatMaterialConfig cfg) {
  vumat::VumatMaterialAdapter adapter(cfg);
  vumat::VumatPointInput in;
  in.dt = 0.000025;
  in.temperature = 293;
  in.stateOld.assign(6, 0);
  auto initialized = adapter.updatePoint(in);
  in.stateOld = initialized.stateNew;
  in.stressFullN = initialized.stressFullNew;
  in.strainRateFull[0] = 400; // 0.01 strain increment at the same reference temperature.
  return adapter.updatePoint(in);
}
int main(int argc, char **argv) {
  try {
    if (argc != 2)
      return 2;
    const auto legacy = vumat::loadMaterialConfigFromJson("configs/johnson_cook.json", argv[1]);
    const auto scaled =
        vumat::loadMaterialConfigFromJson("configs/johnson_cook_heat_units.json", argv[1]);
    const auto a = plasticStep(legacy), b = plasticStep(scaled);
    // Heating is calculated after this first return mapping. Identical stress
    // isolates unit conversion from subsequent thermal-softening feedback.
    for (int i = 0; i < 9; ++i)
      if (std::abs(a.stressFullNew[i] - b.stressFullNew[i]) > 1e-10)
        throw std::runtime_error("First-step mechanics unexpectedly differs");
    const double da = a.temperatureNew - 293, db = b.temperatureNew - 293;
    if (da <= 0 || std::abs(db / (1e6 * da) - 1) > 1e-6)
      throw std::runtime_error("MPa to J/m3 temperature conversion mismatch");
    std::cout << "PASS isolated thermal conversion: legacy dT=" << da << " K; scaled dT=" << db
              << " K; identical first-step stress\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
