#include "PdCorrespondenceSkeleton.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace vumat {
namespace pd {
namespace {

Tensor3 zeroTensor() { return {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a[0]+b[0], a[1]+b[1], a[2]+b[2]}; }
Vec3 subtract(const Vec3& a, const Vec3& b) { return {a[0]-b[0], a[1]-b[1], a[2]-b[2]}; }
double dot(const Vec3& a, const Vec3& b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
double norm(const Vec3& a) { return std::sqrt(dot(a,a)); }
double horizonTolerance(const std::vector<Vec3>& positions, double horizon) {
  double scale=horizon;
  for(const auto& x:positions) for(double v:x) {
    if(!std::isfinite(v)) throw std::runtime_error("Nonfinite PD coordinate.");
    scale=std::max(scale,std::abs(v));
  }
  return 64.0*std::numeric_limits<double>::epsilon()*scale;
}
double& at(Tensor3& a, int r, int c) { return a[static_cast<size_t>(3*r+c)]; }
double at(const Tensor3& a, int r, int c) { return a[static_cast<size_t>(3*r+c)]; }

Tensor3 multiply(const Tensor3& a, const Tensor3& b) {
  Tensor3 out = zeroTensor();
  for (int i=0;i<3;++i) for(int j=0;j<3;++j) for(int k=0;k<3;++k)
    at(out,i,j) += at(a,i,k)*at(b,k,j);
  return out;
}
Vec3 multiply(const Tensor3& a, const Vec3& x) {
  return {at(a,0,0)*x[0]+at(a,0,1)*x[1]+at(a,0,2)*x[2],
          at(a,1,0)*x[0]+at(a,1,1)*x[1]+at(a,1,2)*x[2],
          at(a,2,0)*x[0]+at(a,2,1)*x[1]+at(a,2,2)*x[2]};
}
Tensor3 transpose(const Tensor3& a) {
  return {a[0],a[3],a[6], a[1],a[4],a[7], a[2],a[5],a[8]};
}
double determinant(const Tensor3& a) {
  return a[0]*(a[4]*a[8]-a[5]*a[7])-a[1]*(a[3]*a[8]-a[5]*a[6])+
         a[2]*(a[3]*a[7]-a[4]*a[6]);
}
Tensor3 inverse(const Tensor3& a, const char* context) {
  const double det=determinant(a);
  if(std::abs(det)<1.0e-14) throw std::runtime_error(std::string("Singular tensor in ")+context+".");
  return {(a[4]*a[8]-a[5]*a[7])/det, -(a[1]*a[8]-a[2]*a[7])/det,
          (a[1]*a[5]-a[2]*a[4])/det, -(a[3]*a[8]-a[5]*a[6])/det,
          (a[0]*a[8]-a[2]*a[6])/det, -(a[0]*a[5]-a[2]*a[3])/det,
          (a[3]*a[7]-a[4]*a[6])/det, -(a[0]*a[7]-a[1]*a[6])/det,
          (a[0]*a[4]-a[1]*a[3])/det};
}
void validateBlock(const ParticleBlock& block) {
  const size_t n=block.referencePosition.size();
  if(n==0 || block.displacement.size()!=n || block.volume.size()!=n || block.neighbors.size()!=n)
    throw std::runtime_error("Invalid or incomplete PD particle block.");
}
Tensor3 cauchyToFirstPiola(const Tensor3& stress, const Tensor3& f) {
  const double j=determinant(f);
  if(std::abs(j)<1.0e-14) throw std::runtime_error("Invalid deformation gradient determinant.");
  Tensor3 p=multiply(stress,transpose(inverse(f,"deformation gradient")));
  for(double& v:p) v*=j;
  return p;
}

} // namespace

std::vector<std::vector<int>> buildNeighborList(
    const std::vector<Vec3>& positions, double horizon) {
  if(horizon<=0.0) throw std::runtime_error("PD horizon must be positive.");
  using Cell = std::tuple<long long,long long,long long>;
  const double tolerance=horizonTolerance(positions,horizon);
  const double radius=horizon+tolerance;
  const double cellWidth=horizon+2.0*tolerance;
  std::map<Cell,std::vector<int>> cells;
  const auto cellOf=[cellWidth](const Vec3& x){
    return Cell{static_cast<long long>(std::floor(x[0]/cellWidth)),
                static_cast<long long>(std::floor(x[1]/cellWidth)),
                static_cast<long long>(std::floor(x[2]/cellWidth))};
  };
  for(size_t i=0;i<positions.size();++i)
    cells[cellOf(positions[i])].push_back(static_cast<int>(i));
  std::vector<std::vector<int>> neighbors(positions.size());
  for(size_t i=0;i<positions.size();++i){
    const auto [cx,cy,cz]=cellOf(positions[i]);
    for(long long dx=-1;dx<=1;++dx) for(long long dy=-1;dy<=1;++dy)
      for(long long dz=-1;dz<=1;++dz){
        const auto found=cells.find(Cell{cx+dx,cy+dy,cz+dz});
        if(found==cells.end()) continue;
        for(int ji:found->second){
          const size_t j=static_cast<size_t>(ji);
          if(i!=j && norm(subtract(positions[j],positions[i]))<=radius)
            neighbors[i].push_back(ji);
        }
      }
    std::sort(neighbors[i].begin(),neighbors[i].end());
  }
  return neighbors;
}

std::vector<std::vector<int>> buildNeighborListBruteForce(
    const std::vector<Vec3>& positions, double horizon) {
  if(horizon<=0.0) throw std::runtime_error("PD horizon must be positive.");
  const double radius=horizon+horizonTolerance(positions,horizon);
  std::vector<std::vector<int>> neighbors(positions.size());
  for(size_t i=0;i<positions.size();++i) for(size_t j=0;j<positions.size();++j)
    if(i!=j && norm(subtract(positions[j],positions[i]))<=radius)
      neighbors[i].push_back(static_cast<int>(j));
  return neighbors;
}

PdCorrespondenceSkeleton::PdCorrespondenceSkeleton(double horizon):horizon_(horizon) {
  if(horizon_<=0.0) throw std::runtime_error("PD correspondence skeleton requires horizon > 0.");
}

void PdCorrespondenceSkeleton::initialize(const ParticleBlock& block,
                                          CorrespondenceState& state) const {
  validateBlock(block);
  state.shapeTensorInverse.resize(block.referencePosition.size());
  #pragma omp parallel for schedule(static)
  for(size_t i=0;i<block.referencePosition.size();++i){
    Tensor3 shape=zeroTensor();
    for(int ji:block.neighbors[i]){
      if(ji<0 || static_cast<size_t>(ji)>=block.referencePosition.size())
        throw std::runtime_error("PD neighbor index is out of range.");
      const size_t j=static_cast<size_t>(ji);
      const Vec3 xi=subtract(block.referencePosition[j],block.referencePosition[i]);
      for(int a=0;a<3;++a) for(int b=0;b<3;++b)
        at(shape,a,b)+=block.volume[j]*xi[static_cast<size_t>(a)]*xi[static_cast<size_t>(b)];
    }
    state.shapeTensorInverse[i]=inverse(shape,"PD shape tensor");
  }
}

std::vector<Tensor3> PdCorrespondenceSkeleton::computeDeformationGradients(
    const ParticleBlock& block, const CorrespondenceState& state) const {
  validateBlock(block);
  if(state.shapeTensorInverse.size()!=block.referencePosition.size())
    throw std::runtime_error("PD correspondence state is not initialized.");
  std::vector<Tensor3> f(block.referencePosition.size(),zeroTensor());
  #pragma omp parallel for schedule(static)
  for(size_t i=0;i<block.referencePosition.size();++i){
    Tensor3 numerator=zeroTensor();
    const Vec3 yi=add(block.referencePosition[i],block.displacement[i]);
    for(int ji:block.neighbors[i]){
      const size_t j=static_cast<size_t>(ji);
      const Vec3 xi=subtract(block.referencePosition[j],block.referencePosition[i]);
      const Vec3 yij=subtract(add(block.referencePosition[j],block.displacement[j]),yi);
      for(int a=0;a<3;++a) for(int b=0;b<3;++b)
        at(numerator,a,b)+=block.volume[j]*yij[static_cast<size_t>(a)]*xi[static_cast<size_t>(b)];
    }
    f[i]=multiply(numerator,state.shapeTensorInverse[i]);
  }
  return f;
}

std::vector<Vec3> PdCorrespondenceSkeleton::computeForceDensity(
    const ParticleBlock& block, const CorrespondenceState& state,
    const std::vector<Tensor3>& f, const std::vector<Tensor3>& stress) const {
  const size_t n=block.referencePosition.size();
  if(f.size()!=n || stress.size()!=n)
    throw std::runtime_error("PD force-state inputs do not match particle count.");
  std::vector<Tensor3> forceMap(n);
  #pragma omp parallel for schedule(static)
  for(size_t i=0;i<n;++i) forceMap[i]=multiply(cauchyToFirstPiola(stress[i],f[i]),state.shapeTensorInverse[i]);
  std::vector<Vec3> force(n,{0.0,0.0,0.0});
  #pragma omp parallel for schedule(static)
  for(size_t i=0;i<n;++i){
    for(size_t k=0;k<block.neighbors[i].size();++k){
      const size_t j=static_cast<size_t>(block.neighbors[i][k]);
      const Vec3 xi=subtract(block.referencePosition[j],block.referencePosition[i]);
      const Vec3 ti=multiply(forceMap[i],xi);
      const Vec3 negativeXi{-xi[0],-xi[1],-xi[2]};
      const Vec3 tj=multiply(forceMap[j],negativeXi);
      for(int d=0;d<3;++d) force[i][static_cast<size_t>(d)]+=
        (ti[static_cast<size_t>(d)]-tj[static_cast<size_t>(d)])*block.volume[j];
    }
  }
  return force;
}

} // namespace pd
} // namespace vumat
