#pragma once

#include "PdTypes.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace vumat::demo {

// Coordinates are in the reference configuration, along the loading axis.
// NaN bounds select the inner edges of the two constrained end bands.
inline std::pair<double, double> observationBounds(const pd::ParticleBlock &block, int axis,
                                                   double bandWidth, double requestedMin,
                                                   double requestedMax) {
  if (axis < 0 || axis > 2 || block.referencePosition.empty())
    throw std::runtime_error("Observation window requires nodes and an axis in [0,2].");
  double lo = block.referencePosition.front()[axis], hi = lo;
  for (const auto &x : block.referencePosition) {
    lo = std::min(lo, x[axis]);
    hi = std::max(hi, x[axis]);
  }
  const double a = std::isnan(requestedMin) ? lo + bandWidth : requestedMin;
  const double b = std::isnan(requestedMax) ? hi - bandWidth : requestedMax;
  if (!std::isfinite(a) || !std::isfinite(b) || a < lo || b > hi || a >= b)
    throw std::runtime_error(
        "Observation window must lie within the body and have positive length.");
  return {a, b};
}

// Trapezoidal weights on a grid-aligned subdomain; do not halve exterior
// nodal volumes twice.
inline std::vector<double> observationWeights(const pd::ParticleBlock &block, int axis, double a,
                                              double b, double tolerance) {
  double lo = block.referencePosition.front()[axis], hi = lo;
  for (const auto &x : block.referencePosition) {
    lo = std::min(lo, x[axis]);
    hi = std::max(hi, x[axis]);
  }
  bool foundA = false, foundB = false;
  std::vector<double> weights(block.volume.size(), 0.0);
  for (size_t i = 0; i < weights.size(); ++i) {
    const double x = block.referencePosition[i][axis];
    const bool atA = std::abs(x - a) < tolerance, atB = std::abs(x - b) < tolerance;
    foundA = foundA || atA;
    foundB = foundB || atB;
    if (x < a - tolerance || x > b + tolerance)
      continue;
    // Exterior planes already carry half-volume weights in Geometry.cpp.
    const bool interiorEnd = (atA && a > lo + tolerance) || (atB && b < hi - tolerance);
    weights[i] = block.volume[i] * (interiorEnd ? 0.5 : 1.0);
  }
  if (!foundA || !foundB)
    throw std::runtime_error("Observation bounds must coincide with reference nodal planes.");
  return weights;
}

} // namespace vumat::demo
