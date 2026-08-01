#include "CorotationalKinematics.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

using vumat::pd::Tensor3;

Tensor3 identity() {
  return {1.0,0.0,0.0, 0.0,1.0,0.0, 0.0,0.0,1.0};
}

double maxDifference(const Tensor3& a, const Tensor3& b) {
  double result=0.0;
  for(size_t i=0;i<a.size();++i)
    result=std::max(result,std::abs(a[i]-b[i]));
  return result;
}

void requireNear(double actual, double expected, double tolerance,
                 const char* message) {
  if(std::abs(actual-expected)>tolerance)
    throw std::runtime_error(message);
}

void testRigidRotation() {
  const double angle=1.0e-7;
  const Tensor3 fN=identity();
  const Tensor3 fNP1={std::cos(angle),-std::sin(angle),0.0,
                      std::sin(angle), std::cos(angle),0.0,
                      0.0,             0.0,            1.0};
  const auto update=vumat::pd::updateCorotationalKinematics(
      fN,fNP1,identity(),identity(),1.0);
  Tensor3 zero{};
  if(maxDifference(update.unrotatedRateOfDeformation,zero)>1.0e-13)
    throw std::runtime_error(
        "Rigid rotation generated a non-zero material strain rate.");
  if(maxDifference(update.leftStretchNP1,identity())>1.0e-13)
    throw std::runtime_error(
        "Rigid rotation changed the left stretch tensor.");
}

void testFiniteExtensionAndStrainMeasure() {
  const Tensor3 fNP1={1.01,0.0,0.0, 0.0,1.0,0.0, 0.0,0.0,1.0};
  const auto update=vumat::pd::updateCorotationalKinematics(
      identity(),fNP1,identity(),identity(),0.01);
  requireNear(update.unrotatedRateOfDeformation[0],1.0,1.0e-12,
              "Incorrect unrotated axial deformation rate.");
  requireNear(update.leftStretchNP1[0],1.01,1.0e-12,
              "Incorrect updated left stretch.");
  const Tensor3 strain=vumat::pd::greenLagrangeStrain(fNP1);
  requireNear(strain[0],0.01005,1.0e-12,
              "Incorrect Green-Lagrange axial strain.");
}

void testStressRotation() {
  const Tensor3 rotation={0.0,-1.0,0.0, 1.0,0.0,0.0, 0.0,0.0,1.0};
  const Tensor3 materialStress={10.0,0.0,0.0, 0.0,2.0,0.0, 0.0,0.0,1.0};
  const Tensor3 spatial=vumat::pd::rotateCorotationalStressToSpatial(
      materialStress,rotation);
  requireNear(spatial[0],2.0,1.0e-12,
              "Stress rotation did not map yy stress onto spatial xx.");
  requireNear(spatial[4],10.0,1.0e-12,
              "Stress rotation did not map xx stress onto spatial yy.");
}

} // namespace

int main() {
  try {
    testRigidRotation();
    testFiniteExtensionAndStrainMeasure();
    testStressRotation();
    std::cout << "Corotational kinematics tests passed.\n";
    return 0;
  } catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
