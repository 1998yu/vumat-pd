#include "MaterialConfig.hpp"
#include "VumatMaterialAdapter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Tensor=std::array<double,9>;
Tensor identity(){return {1,0,0,0,1,0,0,0,1};}
double det(const Tensor&a){return a[0]*(a[4]*a[8]-a[5]*a[7])-a[1]*(a[3]*a[8]-a[5]*a[6])+a[2]*(a[3]*a[7]-a[4]*a[6]);}
Tensor transpose(const Tensor&a){return {a[0],a[3],a[6],a[1],a[4],a[7],a[2],a[5],a[8]};}
Tensor mul(const Tensor&a,const Tensor&b){Tensor o{};for(int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)o[3*i+j]+=a[3*i+k]*b[3*k+j];return o;}
Tensor expected(const Tensor&f,double mu,double bulk){
  const double j=det(f),fac=mu*std::pow(j,-5.0/3.0);const Tensor b=mul(f,transpose(f));const double i1=b[0]+b[4]+b[8];Tensor s{};
  for(int i=0;i<3;++i)for(int k=0;k<3;++k)s[3*i+k]=fac*(b[3*i+k]-(i==k?i1/3.0:0.0))+(i==k?bulk*(j-1.0):0.0);return s;
}
double maxError(const Tensor&a,const Tensor&b){double e=0;for(std::size_t i=0;i<9;++i)e=std::max(e,std::abs(a[i]-b[i]));return e;}
std::string exeDir(const char*a){const std::filesystem::path p(a?a:"");return p.has_parent_path()?p.parent_path().string():".";}
struct Test{std::string name;Tensor f;};
}

int main(int argc,char**argv){
  try{
    const std::string configPath=argc>1?argv[1]:"configs/neohookean.json";
    const std::string outDir=argc>2?argv[2]:"demos/output/hyperelastic/material-point-check";
    std::filesystem::create_directories(outDir);const auto cfg=vumat::loadMaterialConfigFromJson(configPath,exeDir(argv[0]));
    if(cfg.props.size()<2)throw std::runtime_error("Neo-Hookean validation requires mu and bulk modulus.");
    const double q=std::sqrt(0.5);const Tensor r={q,-q,0,q,q,0,0,0,1};const Tensor stretch={1.2,0,0,0,0.9,0,0,0,1.0};
    const std::vector<Test> tests={{"uniaxial_tension",{1.2,0,0,0,1,0,0,0,1}},
      {"uniaxial_compression",{0.8,0,0,0,1,0,0,0,1}},
      {"simple_shear",{1,0.3,0,0,1,0,0,0,1}},
      {"xz_shear",{1,0,0.2,0,1,0,0,0,1}},
      {"yz_shear",{1,0,0,0,1,0.25,0,0,1}}, {"rigid_rotation",r},{"rotated_finite_stretch",mul(r,stretch)}};
    vumat::VumatMaterialAdapter material(cfg);std::ofstream csv(outDir+"/material_point_validation.csv");
    csv<<"case,max_stress_error_MPa,J,strain_energy_density_MPa,max_abs_stress_MPa,pass\n";
    double worst=0.0;bool passed=true;
    for(const auto&t:tests){vumat::VumatPointInput in;in.dt=0.01;in.stepTime=0.01;in.totalTime=0.01;in.charLength=1.0;in.temperature=293.0;in.defgradOld=identity();in.defgradNew=t.f;in.stateOld={1.0,0.0,0.0,0.0};
      const auto result=material.updatePoint(in);const Tensor exact=expected(t.f,cfg.props[0],cfg.props[1]);const double error=maxError(result.stressFullNew,exact);double maxStress=0.0;for(double v:result.stressFullNew)maxStress=std::max(maxStress,std::abs(v));
      const bool ok=error<1.0e-9&&std::isfinite(result.stateNew[1]);worst=std::max(worst,error);passed=passed&&ok;csv<<t.name<<','<<std::setprecision(16)<<error<<','<<det(t.f)<<','<<result.stateNew[1]<<','<<maxStress<<','<<(ok?1:0)<<'\n';}
    std::ofstream meta(outDir+"/metadata.json");meta<<"{\n  \"status\": \""<<(passed?"passed":"failed")<<"\",\n  \"plugin\": \""<<std::filesystem::path(cfg.pluginPath).generic_string()<<"\",\n  \"tests\": [\"uniaxial tension\", \"uniaxial compression\", \"xy shear\", \"xz shear\", \"yz shear\", \"rigid rotation\", \"rotated finite stretch\"],\n  \"worstStressError_MPa\": "<<std::setprecision(16)<<worst<<",\n  \"stressMeasure\": \"spatial Cauchy stress returned by this finite-strain VUMAT\",\n  \"tensorOrder\": \"11,22,33,23,13,12; F:11,22,33,12,23,31,21,32,13\"\n}\n";
    std::cout<<"Neo-Hookean VUMAT validation "<<(passed?"passed":"failed")<<"; worst error="<<worst<<" MPa\n";return passed?0:2;
  }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
