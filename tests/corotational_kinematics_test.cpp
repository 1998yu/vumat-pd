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

void requireRotation(const Tensor3& r, double tolerance) {
  const auto eye=identity();
  Tensor3 rtR{};
  for(int i=0;i<3;++i) for(int j=0;j<3;++j) for(int k=0;k<3;++k)
    rtR[static_cast<size_t>(3*i+j)]+=r[static_cast<size_t>(3*k+i)]*r[static_cast<size_t>(3*k+j)];
  if(maxDifference(rtR,eye)>tolerance)
    throw std::runtime_error("Finite spin produced a nonorthogonal rotation.");
  const double det=r[0]*(r[4]*r[8]-r[5]*r[7])-r[1]*(r[3]*r[8]-r[5]*r[6])+r[2]*(r[3]*r[7]-r[4]*r[6]);
  requireNear(det,1.0,tolerance,"Finite spin changed the rotation determinant.");
  const auto rotated=vumat::pd::rotateCorotationalStressToSpatial(eye,r);
  if(maxDifference(rotated,eye)>tolerance)
    throw std::runtime_error("Rotation changed isotropic stress.");
}

void testFiniteSpinExponential() {
  // F_(n+1)=I+dt*W isolates a prescribed skew velocity gradient at V_n=I.
  // These checks test exp(dt*Omega), independently of time discretization
  // error in recovering a rigid body's velocity gradient from two F values.
  for(int axis=0;axis<3;++axis) for(double angle : {1.0e-7,0.01,0.1,0.7,-0.4}) {
    const int a=(axis+1)%3,b=(axis+2)%3;
    auto f=identity();
    f[static_cast<size_t>(3*a+b)]=-angle;
    f[static_cast<size_t>(3*b+a)]=angle;
    auto expected=identity();
    expected[static_cast<size_t>(3*a+a)]=std::cos(angle);
    expected[static_cast<size_t>(3*b+b)]=std::cos(angle);
    expected[static_cast<size_t>(3*a+b)]=-std::sin(angle);
    expected[static_cast<size_t>(3*b+a)]=std::sin(angle);
    const auto update=vumat::pd::updateCorotationalKinematics(identity(),f,identity(),identity(),1.0);
    if(maxDifference(update.rotationNP1,expected)>2.0e-14)
      throw std::runtime_error("Finite spin does not match its analytic matrix exponential.");
    requireRotation(update.rotationNP1,3.0e-14);
  }
}

void testAccumulatedRotation() {
  const double increment=0.01;
  auto rotation=identity();
  auto previousF=identity();
  constexpr int steps=1000;
  for(int step=1;step<=steps;++step) {
    const double angle=increment*step;
    const Tensor3 f={std::cos(angle),-std::sin(angle),0.0,
                     std::sin(angle),std::cos(angle),0.0,0.0,0.0,1.0};
    const auto update=vumat::pd::updateCorotationalKinematics(previousF,f,identity(),rotation,1.0);
    rotation=update.rotationNP1;
    requireRotation(rotation,2.0e-12);
    previousF=f;
  }
  // The backward F difference produces W_z=sin(increment), not increment.
  const double recoveredAngle=steps*std::sin(increment);
  const Tensor3 expected={std::cos(recoveredAngle),-std::sin(recoveredAngle),0.0,
                         std::sin(recoveredAngle),std::cos(recoveredAngle),0.0,0.0,0.0,1.0};
  if(maxDifference(rotation,expected)>2.0e-12)
    throw std::runtime_error("Accumulated spin differs from the recovered-gradient solution.");
}

} // namespace

int main() {
  try {
    testRigidRotation();
    testFiniteExtensionAndStrainMeasure();
    testStressRotation();
    testFiniteSpinExponential();
    testAccumulatedRotation();
    std::cout << "Corotational kinematics tests passed.\n";
    return 0;
  } catch(const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
