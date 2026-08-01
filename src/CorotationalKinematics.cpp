#include "CorotationalKinematics.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vumat {
namespace pd {
namespace {

Tensor3 identity() {
  return {1.0, 0.0, 0.0,
          0.0, 1.0, 0.0,
          0.0, 0.0, 1.0};
}

double& at(Tensor3& a, int r, int c) { return a[static_cast<size_t>(3*r+c)]; }
double at(const Tensor3& a, int r, int c) { return a[static_cast<size_t>(3*r+c)]; }

Tensor3 transpose(const Tensor3& a) {
  return {a[0],a[3],a[6], a[1],a[4],a[7], a[2],a[5],a[8]};
}

Tensor3 multiply(const Tensor3& a, const Tensor3& b) {
  Tensor3 out{};
  for(int i=0;i<3;++i) for(int j=0;j<3;++j) for(int k=0;k<3;++k)
    at(out,i,j)+=at(a,i,k)*at(b,k,j);
  return out;
}

double determinant(const Tensor3& a) {
  return a[0]*(a[4]*a[8]-a[5]*a[7])
      - a[1]*(a[3]*a[8]-a[5]*a[6])
      + a[2]*(a[3]*a[7]-a[4]*a[6]);
}

Tensor3 inverse(const Tensor3& a) {
  const double det=determinant(a);
  if(std::abs(det)<1.0e-14)
    throw std::runtime_error("Corotational update encountered a singular tensor.");
  return {(a[4]*a[8]-a[5]*a[7])/det, (a[2]*a[7]-a[1]*a[8])/det,
          (a[1]*a[5]-a[2]*a[4])/det, (a[5]*a[6]-a[3]*a[8])/det,
          (a[0]*a[8]-a[2]*a[6])/det, (a[2]*a[3]-a[0]*a[5])/det,
          (a[3]*a[7]-a[4]*a[6])/det, (a[1]*a[6]-a[0]*a[7])/det,
          (a[0]*a[4]-a[1]*a[3])/det};
}

Tensor3 addScaled(const Tensor3& a, const Tensor3& b, double scale) {
  Tensor3 out=a;
  for(size_t i=0;i<9;++i) out[i]+=scale*b[i];
  return out;
}

} // namespace

CorotationalUpdate updateCorotationalKinematics(
    const Tensor3& fN, const Tensor3& fNP1, const Tensor3& vN,
    const Tensor3& rN, double dt) {
  if(dt<=0.0) throw std::runtime_error("Corotational update requires dt > 0.");

  CorotationalUpdate out;
  Tensor3 fDot{};
  for(size_t i=0;i<9;++i) fDot[i]=(fNP1[i]-fN[i])/dt;
  out.velocityGradient=multiply(fDot,inverse(fN));

  Tensor3 d{},w{};
  const Tensor3 lt=transpose(out.velocityGradient);
  for(size_t i=0;i<9;++i) {
    d[i]=0.5*(out.velocityGradient[i]+lt[i]);
    w[i]=0.5*(out.velocityGradient[i]-lt[i]);
  }

  // Flanagan--Taylor spin correction, matching Peridigm correspondence.cxx.
  const double zX=-vN[2]*d[3]-vN[5]*d[4]-vN[8]*d[5]
                  +vN[1]*d[6]+vN[4]*d[7]+vN[7]*d[8];
  const double zY= vN[2]*d[0]+vN[5]*d[1]+vN[8]*d[2]
                  -vN[0]*d[6]-vN[3]*d[7]-vN[6]*d[8];
  const double zZ=-vN[1]*d[0]-vN[4]*d[1]-vN[7]*d[2]
                  +vN[0]*d[3]+vN[3]*d[4]+vN[6]*d[5];
  const double wX=0.5*(w[7]-w[5]);
  const double wY=0.5*(w[2]-w[6]);
  const double wZ=0.5*(w[3]-w[1]);
  const double trV=vN[0]+vN[4]+vN[8];
  Tensor3 trIminusV=identity();
  for(double& x:trIminusV) x*=trV;
  for(size_t i=0;i<9;++i) trIminusV[i]-=vN[i];
  const Tensor3 correction=inverse(trIminusV);
  const double omegaX=wX+correction[0]*zX+correction[1]*zY+correction[2]*zZ;
  const double omegaY=wY+correction[3]*zX+correction[4]*zY+correction[5]*zZ;
  const double omegaZ=wZ+correction[6]*zX+correction[7]*zY+correction[8]*zZ;
  const Tensor3 omega={0.0,-omegaZ,omegaY,
                       omegaZ,0.0,-omegaX,
                       -omegaY,omegaX,0.0};

  const double omegaSq=omegaX*omegaX+omegaY*omegaY+omegaZ*omegaZ;
  Tensor3 q=identity();
  if(omegaSq>1.0e-30) {
    const double magnitude=std::sqrt(omegaSq);
    const double s1=std::sin(dt*magnitude)/magnitude;
    const double s2=-(1.0-std::cos(dt*magnitude))/omegaSq;
    const Tensor3 omega2=multiply(omega,omega);
    for(size_t i=0;i<9;++i) q[i]+=s1*omega[i]+s2*omega2[i];
  }
  out.rotationNP1=multiply(q,rN);

  const Tensor3 rateOfStretchA=multiply(out.velocityGradient,vN);
  const Tensor3 rateOfStretchB=multiply(vN,omega);
  Tensor3 rateOfStretch{};
  for(size_t i=0;i<9;++i) rateOfStretch[i]=rateOfStretchA[i]-rateOfStretchB[i];
  out.leftStretchNP1=addScaled(vN,rateOfStretch,dt);

  out.unrotatedRateOfDeformation=
      multiply(transpose(out.rotationNP1),multiply(d,out.rotationNP1));
  // Abaqus relSpinInc is the spin remaining after removal of the
  // corotational material-frame spin, expressed in that material frame.
  Tensor3 relativeSpinSpatial{};
  for(size_t i=0;i<9;++i) relativeSpinSpatial[i]=w[i]-omega[i];
  out.relativeSpinIncrement=multiply(
      transpose(out.rotationNP1),multiply(relativeSpinSpatial,out.rotationNP1));
  for(double& x:out.relativeSpinIncrement) x*=dt;
  return out;
}

Tensor3 rotateCorotationalStressToSpatial(const Tensor3& stress,
                                          const Tensor3& rotation) {
  return multiply(rotation,multiply(stress,transpose(rotation)));
}

Tensor3 greenLagrangeStrain(const Tensor3& f) {
  Tensor3 result=multiply(transpose(f),f);
  result[0]-=1.0; result[4]-=1.0; result[8]-=1.0;
  for(double& x:result) x*=0.5;
  return result;
}

} // namespace pd
} // namespace vumat
