#include "ExplicitTimeIntegrator.hpp"
#include "RigidWallContact.hpp"
#include "SimpleJson.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
  try {
    const std::string deck = argc > 1 ? argv[1]
        : "examples/rigid_wall_contact.json";
    const auto root = vumat::json::loadFile(deck);
    const auto* particle = vumat::json::find(root, "particle");
    const auto* time = vumat::json::find(root, "time_integration");
    const auto* contactObject = vumat::json::find(root, "contact");
    const auto* output = vumat::json::find(root, "output");
    if (!particle || !time || !contactObject || !output)
      throw std::runtime_error("Contact regression deck is incomplete.");
    const double x0 = vumat::json::getDouble(*particle, "initialPosition");
    const double v0 = vumat::json::getDouble(*particle, "initialVelocity");
    const double volume = vumat::json::getDouble(*particle, "volume");
    const double density = vumat::json::getDouble(*particle, "density");
    const double dt = vumat::json::getDouble(*time, "dt");
    const int steps = vumat::json::getInt(*time, "steps");
    const std::string directory = vumat::json::getString(*output, "directory");

    vumat::demo::RigidWallContactConfig contactConfig;
    contactConfig.enabled = vumat::json::getBool(*contactObject, "enabled");
    contactConfig.point = {vumat::json::getDouble(*contactObject, "planePosition"), 0.0, 0.0};
    contactConfig.normal = {1.0, 0.0, 0.0};
    contactConfig.penaltyStiffness =
        vumat::json::getDouble(*contactObject, "penaltyStiffness");
    contactConfig.frictionCoefficient =
        vumat::json::getDouble(*contactObject, "frictionCoefficient");

    vumat::pd::ParticleBlock block;
    block.referencePosition = {{x0, 0.0, 0.0}};
    block.displacement = {{0.0, 0.0, 0.0}};
    block.volume = {volume};
    block.neighbors = {{}};
    vumat::pd::ExplicitTimeIntegrator integrator(density);
    vumat::pd::ExplicitDynamicState state;
    integrator.initialize(block, state);
    state.velocity[0][0] = v0;
    vumat::pd::RigidWallContact contact(contactConfig);

    std::filesystem::create_directories(directory);
    std::ofstream csv(directory + "/timeseries.csv");
    csv << "step,time,position,velocity,contact_force,wall_impulse,penetration,finite\n";
    double impulse = 0.0, previousForce = 0.0, maxPenetration = 0.0;
    bool finite = true, contacted = false, rebounded = false;
    for (int step = 1; step <= steps; ++step) {
      integrator.predictDisplacement(block, state, dt);
      const auto result = contact.evaluate(block, state);
      integrator.correctVelocity(state, result.forceDensity, dt);
      impulse += 0.5 * dt * (previousForce + result.forceOnBody[0]);
      previousForce = result.forceOnBody[0];
      maxPenetration = std::max(maxPenetration, result.maxPenetration);
      contacted = contacted || result.activeParticles > 0;
      const double position = block.referencePosition[0][0] + block.displacement[0][0];
      finite = finite && std::isfinite(position) && std::isfinite(state.velocity[0][0]) &&
          std::isfinite(result.forceOnBody[0]);
      csv << step << ',' << std::setprecision(16) << step * dt << ',' << position << ','
          << state.velocity[0][0] << ',' << result.forceOnBody[0] << ',' << impulse << ','
          << result.maxPenetration << ',' << (finite ? 1 : 0) << '\n';
      if (contacted && position > 0.0 && state.velocity[0][0] > 0.0) {
        rebounded = true;
        break;
      }
    }
    const double initialMomentum = density * volume * v0;
    const double finalMomentum = density * volume * state.velocity[0][0];
    const double momentumError = std::abs((finalMomentum - initialMomentum) - impulse) /
        std::max(std::abs(impulse), 1.0e-12);
    const auto activeCheck = contact.evaluate(block, state);
    contactConfig.enabled = false;
    const auto disabledCheck = vumat::pd::RigidWallContact(contactConfig).evaluate(block, state);
    const bool disabledZero = disabledCheck.forceOnBody[0] == 0.0 &&
        disabledCheck.forceOnBody[1] == 0.0 && disabledCheck.forceOnBody[2] == 0.0;
    const bool pass = finite && contacted && rebounded && disabledZero &&
        maxPenetration <= vumat::json::getDouble(root, "maximumPenetration") &&
        momentumError <= vumat::json::getDouble(root, "momentumToleranceRelative") &&
        activeCheck.normalReaction == 0.0;
    std::ofstream metadata(directory + "/metadata.json");
    metadata << "{\n  \"status\": \"" << (pass ? "pass" : "fail") << "\",\n"
        << "  \"finite\": " << (finite ? "true" : "false") << ",\n"
        << "  \"contacted\": " << (contacted ? "true" : "false") << ",\n"
        << "  \"rebounded\": " << (rebounded ? "true" : "false") << ",\n"
        << "  \"disabledContactForceZero\": " << (disabledZero ? "true" : "false") << ",\n"
        << "  \"maximumPenetration\": " << maxPenetration << ",\n"
        << "  \"momentumImpulseRelativeError\": " << momentumError << "\n}\n";
    std::cout << "rigid-wall contact regression: penetration=" << maxPenetration
        << " momentum_error=" << momentumError << " pass=" << (pass ? "true" : "false") << '\n';
    return pass ? 0 : 2;
  } catch (const std::exception& error) {
    std::cerr << "rigid-wall contact regression failed: " << error.what() << '\n';
    return 1;
  }
}
