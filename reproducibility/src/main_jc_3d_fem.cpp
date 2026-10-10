#include "MaterialConfig.hpp"
#include "VumatPdCorrespondenceMaterial.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

using V=vumat::pd::Vec3;
using T=vumat::pd::Tensor3;
const T I{1,0,0,0,1,0,0,0,1};
const std::array<V,8> signs{{{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
                            {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}}};
struct Shape {std::array<double,8> N{};std::array<V,8> grad{};};
Shape shape(V q,double h) {
  Shape s;
  for(int a=0;a<8;++a) {
    const auto& b=signs[a];
    s.N[a]=.125*(1+b[0]*q[0])*(1+b[1]*q[1])*(1+b[2]*q[2]);
    for(int d=0;d<3;++d) {
      s.grad[a][d]=.25/h*b[d];
      for(int k=0;k<3;++k) if(k!=d) s.grad[a][d]*=1+b[k]*q[k];
    }
  }
  return s;
}
T piola(const T& s,const T& f) {
  // Cofactor(F)=det(F) F^{-T}; P=sigma Cofactor(F).
  const T c{f[4]*f[8]-f[5]*f[7],f[5]*f[6]-f[3]*f[8],f[3]*f[7]-f[4]*f[6],
            f[2]*f[7]-f[1]*f[8],f[0]*f[8]-f[2]*f[6],f[1]*f[6]-f[0]*f[7],
            f[1]*f[5]-f[2]*f[4],f[2]*f[3]-f[0]*f[5],f[0]*f[4]-f[1]*f[3]};
  const double J=f[0]*c[0]+f[1]*c[1]+f[2]*c[2];
  if(!(J>0) || !std::isfinite(J)) throw std::runtime_error("Invalid FEM Jacobian");
  T p{};for(int i=0;i<3;++i)for(int j=0;j<3;++j)for(int k=0;k<3;++k)p[3*i+j]+=s[3*i+k]*c[3*k+j];
  return p;
}
void selftest() {
  double volume=0;const double g=1/std::sqrt(3.0),h=2;
  for(const auto& b:signs) {
    const auto s=shape({b[0]*g,b[1]*g,b[2]*g},h);
    double nsum=std::accumulate(s.N.begin(),s.N.end(),0.0);
    if(std::abs(nsum-1)>1e-12) throw std::runtime_error("Partition failed");
    for(int d=0;d<3;++d) for(int k=0;k<3;++k) {
      double v=0;for(int a=0;a<8;++a)v+=.5*h*signs[a][k]*s.grad[a][d];
      if(std::abs(v-(d==k?1:0))>1e-12)throw std::runtime_error("Affine patch failed");
    }
    volume+=h*h*h/8;
  }
  const auto p=piola(I,{2,0,0,0,3,0,0,0,4});
  if(std::abs(volume-8)>1e-12||p[0]!=12||p[4]!=8||p[8]!=6) throw std::runtime_error("Piola failed");
  std::cout<<"H8 partition, affine patch, integrated volume, Piola transform: PASS\n";
}
int main(int argc,char**argv) {try {
  if(argc==2&&std::string(argv[1])=="--selftest"){selftest();return 0;}
  if(argc!=7)throw std::runtime_error("Usage: jc_3d_fem material.json plugin_dir h dt steps output");
  const double h=std::stod(argv[3]),dt=std::stod(argv[4]);const int steps=std::stoi(argv[5]);
  const bool symmetric=std::getenv("FEM_SYMMETRY")&&std::string(std::getenv("FEM_SYMMETRY"))=="1";
  const double width=symmetric?10:20,thickness=symmetric?2:4;
  const int nx=std::lround(100/h),ny=std::lround(width/h),nz=std::lround(thickness/h);
  if(std::abs(nx*h-100)>1e-9||std::abs(ny*h-width)>1e-9||std::abs(nz*h-thickness)>1e-9||std::abs(dt*steps-.12)>1e-10)
    throw std::runtime_error("Grid must exactly span 100 x 20 x 4 mm and time 0.12 ms");
  const auto id=[=](int i,int j,int k){return (i*(ny+1)+j)*(nz+1)+k;};
  std::vector<V>x((nx+1)*(ny+1)*(nz+1)),u(x.size()),vel(x.size()),acc(x.size()),force(x.size());
  std::vector<double>mass(x.size());
  for(int i=0;i<=nx;++i)for(int j=0;j<=ny;++j)for(int k=0;k<=nz;++k)x[id(i,j,k)]={i*h,j*h-(symmetric?0:10),k*h-(symmetric?0:2)};
  std::vector<std::array<int,8>>elem;
  for(int i=0;i<nx;++i)for(int j=0;j<ny;++j)for(int k=0;k<nz;++k) {
    elem.push_back({id(i,j,k),id(i+1,j,k),id(i+1,j+1,k),id(i,j+1,k),id(i,j,k+1),id(i+1,j,k+1),id(i+1,j+1,k+1),id(i,j+1,k+1)});
    for(int a:elem.back())mass[a]+=.0078*h*h*h/8;
  }
  const double totalMass=std::accumulate(mass.begin(),mass.end(),0.0);
  if(std::abs(totalMass-.0078*100*width*thickness)>1e-7)throw std::runtime_error("Mass check failed");
  const int band=std::lround(2/h),anchor=id(band,symmetric?0:ny/2,symmetric?0:nz/2),pin=id(band,ny,symmetric?0:nz/2);
  const double g=1/std::sqrt(3.0),weight=h*h*h/8;
  std::array<Shape,8> shapes;for(int q=0;q<8;++q)shapes[q]=shape({signs[q][0]*g,signs[q][1]*g,signs[q][2]*g},h);
  const auto constraint=[&](double time){
    const double pi=std::acos(-1.0),phase=pi*time/.12;
    const double disp=time>=.12?2:1-std::cos(phase);
    const double v=time>=.12?0:pi/.12*std::sin(phase);
    const double a=time>=.12?0:pi*pi/(.12*.12)*std::cos(phase);
    for(size_t n=0;n<x.size();++n) {
      if(x[n][0]<=2+1e-8){u[n][0]=vel[n][0]=acc[n][0]=0;}
      if(x[n][0]>=98-1e-8){u[n][0]=disp;vel[n][0]=v;acc[n][0]=a;}
      if(symmetric&&std::abs(x[n][1])<1e-8){u[n][1]=vel[n][1]=acc[n][1]=0;}
      if(symmetric&&std::abs(x[n][2])<1e-8){u[n][2]=vel[n][2]=acc[n][2]=0;}
    }
    u[anchor][1]=vel[anchor][1]=acc[anchor][1]=0;
    u[anchor][2]=vel[anchor][2]=acc[anchor][2]=0;
    u[pin][2]=vel[pin][2]=acc[pin][2]=0;
  };
  vumat::pd::VumatPdCorrespondenceConfig cfg;
  cfg.material=vumat::loadMaterialConfigFromJson(argv[1],argv[2]);cfg.charLength=h;
  vumat::pd::VumatPdCorrespondenceMaterial bridge(cfg);vumat::pd::MaterialState state;
  const size_t np=elem.size()*8;std::vector<T>F(np,I);bridge.initialize(F,state);
  std::vector<V>gp(np);
  for(size_t e=0;e<elem.size();++e)for(int q=0;q<8;++q)for(int a=0;a<8;++a)for(int d=0;d<3;++d)gp[8*e+q][d]+=shapes[q].N[a]*x[elem[e][a]][d];
  const std::filesystem::path out=argv[6];std::filesystem::create_directories(out);
  std::ofstream ts(out/"timeseries.csv");ts<<std::setprecision(16)<<"step,time_ms,load_displacement_mm,average_axial_strain,average_axial_stress_MPa,max_eqps,net_internal_force_N,finite\n";
  vumat::pd::MaterialUpdateResult result;constraint(0);
  const auto started=std::chrono::steady_clock::now();
  for(int step=1;step<=steps;++step){
    const double time=step*dt;
    for(size_t n=0;n<x.size();++n)for(int d=0;d<3;++d)u[n][d]+=dt*vel[n][d]+.5*dt*dt*acc[n][d];
    constraint(time);
    #pragma omp parallel for schedule(static)
    for(size_t e=0;e<elem.size();++e)for(int q=0;q<8;++q) {
      auto& f=F[8*e+q];f=I;
      for(int a=0;a<8;++a)for(int i=0;i<3;++i)for(int j=0;j<3;++j)f[3*i+j]+=u[elem[e][a]][i]*shapes[q].grad[a][j];
    }
    result=bridge.update(F,state,dt,time,time);
    std::fill(force.begin(),force.end(),V{});
    double meanS=0,meanE=0,vol=0,maxEQ=0;
    for(size_t e=0;e<elem.size();++e)for(int q=0;q<8;++q){
      const size_t p=8*e+q;const auto P=piola(result.rawStress[p],F[p]);
      for(int a=0;a<8;++a)for(int i=0;i<3;++i)for(int j=0;j<3;++j)force[elem[e][a]][i]+=P[3*i+j]*shapes[q].grad[a][j]*weight;
      if(gp[p][0]>2&&gp[p][0]<98){meanS+=result.rawStress[p][0]*weight;meanE+=result.strain[p][0]*weight;vol+=weight;}
      maxEQ=std::max(maxEQ,state.vumatStateN[p][1]);
    }
    V net{};
    for(size_t n=0;n<x.size();++n)for(int d=0;d<3;++d){
      net[d]+=force[n][d];const double an=-force[n][d]/mass[n];vel[n][d]+=.5*dt*(acc[n][d]+an);acc[n][d]=an;
      if(!std::isfinite(u[n][d])||!std::isfinite(vel[n][d]))throw std::runtime_error("Nonfinite FEM motion");
    }
    constraint(time);
    if(step%(steps/240)==0||step==1||step==steps){
      ts<<step<<','<<time<<','<<(time>=.12?2:1-std::cos(std::acos(-1.0)*time/.12))<<','<<meanE/vol<<','<<meanS/vol<<','<<maxEQ<<','<<std::sqrt(net[0]*net[0]+net[1]*net[1]+net[2]*net[2])<<",1\n";ts.flush();
    }
    if(step%240==0){std::cout<<"step="<<step<<"/"<<steps<<" stress="<<meanS/vol<<" elapsed_s="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<std::endl;}
  }
  std::ofstream nodes(out/"final_nodes.csv"),fields(out/"final_gauss_points.csv");
  nodes<<std::setprecision(16)<<"node,x0_mm,y0_mm,z0_mm,ux_mm,uy_mm,uz_mm\n";
  size_t outNode=0;
  for(size_t n=0;n<x.size();++n)for(int sy : {1,-1})for(int sz : {1,-1}) {
    if(!symmetric&&(sy<0||sz<0))continue;
    if(symmetric&&((sy<0&&std::abs(x[n][1])<1e-8)||(sz<0&&std::abs(x[n][2])<1e-8)))continue;
    nodes<<outNode++<<','<<x[n][0]<<','<<sy*x[n][1]<<','<<sz*x[n][2]<<','<<u[n][0]<<','<<sy*u[n][1]<<','<<sz*u[n][2]<<'\n';
  }
  fields<<std::setprecision(16)<<"element,gauss,x0_mm,y0_mm,z0_mm,s11_MPa,eqps,e11\n";
  for(size_t p=0;p<np;++p)for(int sy : {1,-1})for(int sz : {1,-1}) {
    if(!symmetric&&(sy<0||sz<0))continue;
    fields<<p/8<<','<<p%8<<','<<gp[p][0]<<','<<sy*gp[p][1]<<','<<sz*gp[p][2]<<','<<result.rawStress[p][0]<<','<<state.vumatStateN[p][1]<<','<<result.strain[p][0]<<'\n';
  }
  std::ofstream meta(out/"metadata.json");meta<<"{\"element_type\":\"H8 fully integrated\",\"mesh\":["<<nx<<','<<ny*(symmetric?2:1)<<','<<nz*(symmetric?2:1)<<"],\"spacing_mm\":"<<h<<",\"dt_ms\":"<<dt<<",\"steps\":"<<steps<<",\"volume_mm3\":8000,\"planeStrain\":false,\"symmetry_reconstruction\":"<<(symmetric?"true":"false")<<",\"finite\":true,\"mass\":"<<totalMass*(symmetric?4:1)<<"}\n";
  std::cout<<"FEM COMPLETED elements="<<elem.size()<<" gauss_points="<<np<<" volume=8000\n";
  return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
