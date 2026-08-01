#include "PdSimpleDamage.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace vumat {
namespace pd {
namespace {
Vec3 add(const Vec3& a,const Vec3& b){return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
Vec3 sub(const Vec3& a,const Vec3& b){return {a[0]-b[0],a[1]-b[1],a[2]-b[2]};}
double norm(const Vec3& a){return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);}

std::vector<double> damageIndex(const ParticleBlock& block,
                                const std::vector<std::vector<double>>& bonds){
  std::vector<double> result(block.referencePosition.size(),0.0);
  for(size_t i=0;i<result.size();++i){
    double damaged=0.0,total=0.0;
    for(size_t k=0;k<block.neighbors[i].size();++k){
      const size_t j=static_cast<size_t>(block.neighbors[i][k]);
      damaged+=bonds[i][k]*block.volume[j]; total+=block.volume[j];
    }
    result[i]=total>0.0?damaged/total:0.0;
  }
  return result;
}
} // namespace

PdSimpleDamage::PdSimpleDamage(SimpleDamageConfig config)
    :config_(std::move(config)),pointModel_(config_){}

void PdSimpleDamage::initialize(const ParticleBlock& block,PdDamageState& state) const{
  const size_t n=block.referencePosition.size();
  state.bondDamageN.resize(n);
  for(size_t i=0;i<n;++i) state.bondDamageN[i].assign(block.neighbors[i].size(),0.0);
  state.damageIndexN.assign(n,0.0);
  state.pointDamageN.assign(n,SimpleDamageState{});
}

PdDamageResult PdSimpleDamage::update(const ParticleBlock& block,
    const std::vector<Tensor3>& strain,const std::vector<Tensor3>& rawStress,
    const std::vector<std::vector<double>>& vumatState,PdDamageState& state) const{
  const size_t n=block.referencePosition.size();
  if(strain.size()!=n||rawStress.size()!=n||vumatState.size()!=n||state.bondDamageN.size()!=n)
    throw std::runtime_error("Simple PD damage inputs do not match particle count.");
  PdDamageResult out;
  out.bondDamage=state.bondDamageN;
  out.effectiveStress=rawStress;
  if(config_.mode=="critical_stretch"){
    if(config_.initiation<=0.0) throw std::runtime_error("critical_stretch requires initiation > 0.");
    const bool useCompression=config_.failure>0.0&&std::isfinite(config_.failure);
    for(size_t i=0;i<n;++i){
      const Vec3 yi=add(block.referencePosition[i],block.displacement[i]);
      for(size_t k=0;k<block.neighbors[i].size();++k){
        const size_t j=static_cast<size_t>(block.neighbors[i][k]);
        const Vec3 xi=sub(block.referencePosition[j],block.referencePosition[i]);
        const double initial=norm(xi);
        const double stretch=(norm(sub(add(block.referencePosition[j],block.displacement[j]),yi))-initial)/initial;
        if(stretch>config_.initiation||(useCompression&&stretch < -config_.failure))
          out.bondDamage[i][k]=1.0;
      }
    }
    out.damageIndex=damageIndex(block,out.bondDamage);
    for(size_t i=0;i<n;++i){
      state.pointDamageN[i].damage=out.damageIndex[i];
      state.pointDamageN[i].deleted=config_.deletionEnabled&&out.damageIndex[i]>=1.0;
    }
  } else {
    out.damageIndex.resize(n);
    for(size_t i=0;i<n;++i){
      state.pointDamageN[i]=pointModel_.update(strain[i],vumatState[i],state.pointDamageN[i]);
      out.damageIndex[i]=state.pointDamageN[i].damage;
      out.effectiveStress[i]=pointModel_.degradeStress(rawStress[i],state.pointDamageN[i]);
    }
  }
  state.bondDamageN=out.bondDamage;
  state.damageIndexN=out.damageIndex;
  return out;
}

} // namespace pd
} // namespace vumat
