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

using Vec2 = std::array<double, 2>;
using Tensor = std::array<double, 9>;

struct Element {
  std::array<int, 4> node{};
};

struct GaussState {
  Tensor strain{};
  Tensor stress{};
  std::vector<double> state = std::vector<double>(6, 0.0);
  double temperature = 293.0;
  double internalEnergy = 0.0;
  double inelasticEnergy = 0.0;
};

struct Kinematics {
  std::array<std::array<double, 2>, 4> gradient{};
  double detJ = 0.0;
};

double ramp(double time, double total, double amplitude) {
  if (time >= total) return amplitude;
  constexpr double pi = 3.14159265358979323846;
  return 0.5 * amplitude * (1.0 - std::cos(pi * time / total));
}

double rampVelocity(double time, double total, double amplitude) {
  if (time >= total) return 0.0;
  constexpr double pi = 3.14159265358979323846;
  return 0.5 * amplitude * pi / total * std::sin(pi * time / total);
}

double rampAcceleration(double time, double total, double amplitude) {
  if (time > total) return 0.0;
  constexpr double pi = 3.14159265358979323846;
  return 0.5 * amplitude * pi * pi / (total * total) * std::cos(pi * time / total);
}

Kinematics kinematics(const std::array<Vec2, 4>& x, double xi, double eta) {
  const std::array<double, 4> dxi = {
      -0.25 * (1.0 - eta), 0.25 * (1.0 - eta),
       0.25 * (1.0 + eta),-0.25 * (1.0 + eta)};
  const std::array<double, 4> deta = {
      -0.25 * (1.0 - xi),-0.25 * (1.0 + xi),
       0.25 * (1.0 + xi), 0.25 * (1.0 - xi)};
  double j00=0.0,j01=0.0,j10=0.0,j11=0.0;
  for(int a=0;a<4;++a) {
    j00 += dxi[a]*x[a][0]; j01 += deta[a]*x[a][0];
    j10 += dxi[a]*x[a][1]; j11 += deta[a]*x[a][1];
  }
  const double det=j00*j11-j01*j10;
  if(det<=0.0) throw std::runtime_error("Invalid FEM element Jacobian.");
  Kinematics result; result.detJ=det;
  for(int a=0;a<4;++a) {
    result.gradient[a][0]=( j11*dxi[a]-j10*deta[a])/det;
    result.gradient[a][1]=(-j01*dxi[a]+j00*deta[a])/det;
  }
  return result;
}

Tensor strainAt(const std::array<Vec2,4>& u, const Kinematics& k) {
  Tensor value{};
  for(int a=0;a<4;++a) {
    value[0] += k.gradient[a][0]*u[a][0];
    value[4] += k.gradient[a][1]*u[a][1];
    const double exy=0.5*(k.gradient[a][1]*u[a][0]+k.gradient[a][0]*u[a][1]);
    value[1] += exy; value[3] += exy;
  }
  return value;
}

bool finiteTensor(const Tensor& value) {
  return std::all_of(value.begin(),value.end(),[](double x){return std::isfinite(x);});
}

} // namespace

int main(int argc, char** argv) {
  try {
    if(argc!=7) {
      std::cerr << "usage: jc_2d_fem <jc_config> <plugin_dir> <spacing_mm> <dt_ms> <steps> <output_dir>\n";
      return 2;
    }
    const std::string materialConfig=argv[1];
    const std::string pluginDirectory=argv[2];
    const double spacing=std::stod(argv[3]);
    const double dt=std::stod(argv[4]);
    const int steps=std::stoi(argv[5]);
    const std::filesystem::path output=argv[6];
    constexpr double length=100.0,height=20.0,thickness=1.0,density=0.0078,amplitude=2.0,total=0.12;
    const int nx=static_cast<int>(std::lround(length/spacing));
    const int ny=static_cast<int>(std::lround(height/spacing));
    if(std::abs(nx*spacing-length)>1e-10||std::abs(ny*spacing-height)>1e-10)
      throw std::runtime_error("Spacing must divide the 100 x 20 mm geometry.");
    if(std::abs(dt*steps-total)>1e-10)
      throw std::runtime_error("dt * steps must equal 0.12 ms.");

    const auto nodeId=[nx](int i,int j){return i+(nx+1)*j;};
    std::vector<Vec2> x(static_cast<size_t>((nx+1)*(ny+1)));
    for(int j=0;j<=ny;++j) for(int i=0;i<=nx;++i)
      x[static_cast<size_t>(nodeId(i,j))]={i*spacing,j*spacing};
    std::vector<Element> elements;
    for(int j=0;j<ny;++j) for(int i=0;i<nx;++i)
      elements.push_back({{nodeId(i,j),nodeId(i+1,j),nodeId(i+1,j+1),nodeId(i,j+1)}});

    const double g=1.0/std::sqrt(3.0);
    const std::array<std::array<double,2>,4> gps={{{-g,-g},{g,-g},{g,g},{-g,g}}};
    std::vector<std::array<Kinematics,4>> shape(elements.size());
    std::vector<GaussState> gpState(elements.size()*4);
    std::vector<double> mass(x.size(),0.0);
    for(size_t e=0;e<elements.size();++e) {
      std::array<Vec2,4> xe{};
      for(int a=0;a<4;++a) xe[a]=x[static_cast<size_t>(elements[e].node[a])];
      for(int q=0;q<4;++q) shape[e][q]=kinematics(xe,gps[q][0],gps[q][1]);
      const double elementMass=density*spacing*spacing*thickness;
      for(int a=0;a<4;++a) mass[static_cast<size_t>(elements[e].node[a])]+=0.25*elementMass;
    }

    auto config=vumat::loadMaterialConfigFromJson(materialConfig, pluginDirectory);
    vumat::VumatMaterialAdapter adapter(config);
    std::vector<Vec2> u(x.size()),v(x.size()),acc(x.size()),force(x.size());
    std::vector<char> left(x.size(),0),right(x.size(),0);
    for(int j=0;j<=ny;++j) for(int i=0;i<=nx;++i) {
      const int n=nodeId(i,j);
      if(i<=1) left[static_cast<size_t>(n)]=1;
      if(i>=nx-1) right[static_cast<size_t>(n)]=1;
    }
    // Match RegionalConstraintSystem: the closest support point to the global
    // centre lies on the inner of the two constrained x layers.
    const int anchor=nodeId(1,ny/2);
    std::filesystem::create_directories(output);
    std::ofstream series(output/"timeseries.csv");
    series << "step,time_ms,load_displacement_mm,reaction_force_N,engineering_strain,"
              "nominal_stress_MPa,average_axial_strain,average_axial_stress_MPa,max_eqps,finite\n";
    series << std::setprecision(16);

    bool allFinite=true;
    for(int step=1;step<=steps;++step) {
      const double time=step*dt;
      for(size_t n=0;n<x.size();++n) for(int d=0;d<2;++d)
        u[n][d]+=v[n][d]*dt+0.5*acc[n][d]*dt*dt;
      const double prescribed=ramp(time,total,amplitude);
      const double prescribedV=rampVelocity(time,total,amplitude);
      const double prescribedA=rampAcceleration(time,total,amplitude);
      for(size_t n=0;n<x.size();++n) {
        if(left[n]) {u[n][0]=0.0;v[n][0]=0.0;acc[n][0]=0.0;}
        if(right[n]) {u[n][0]=prescribed;v[n][0]=prescribedV;acc[n][0]=prescribedA;}
      }
      u[static_cast<size_t>(anchor)][1]=0.0;
      v[static_cast<size_t>(anchor)][1]=0.0;
      acc[static_cast<size_t>(anchor)][1]=0.0;

      vumat::VumatBlockInput input;
      const size_t pointCount=elements.size()*4;
      input.dt=dt; input.stepTime=(step-1)*dt; input.totalTime=time;
      input.charLength.assign(pointCount,spacing);
      input.temperature.resize(pointCount);
      input.strainRateFull.resize(pointCount);
      input.strainFullN.resize(pointCount);
      input.stressFullN.resize(pointCount);
      input.defgradOld.resize(pointCount);
      input.defgradNew.resize(pointCount);
      input.stateOld.resize(pointCount);
      input.internalEnergyOld.resize(pointCount);
      input.inelasticEnergyOld.resize(pointCount);
      for(size_t e=0;e<elements.size();++e) {
        std::array<Vec2,4> ue{};
        for(int a=0;a<4;++a) ue[a]=u[static_cast<size_t>(elements[e].node[a])];
        for(int q=0;q<4;++q) {
          const size_t p=4*e+static_cast<size_t>(q);
          const Tensor newStrain=strainAt(ue,shape[e][q]);
          Tensor rate{};
          for(size_t c=0;c<9;++c) rate[c]=(newStrain[c]-gpState[p].strain[c])/dt;
          input.temperature[p]=gpState[p].temperature;
          input.strainRateFull[p]=rate;
          input.strainFullN[p]=gpState[p].strain;
          input.stressFullN[p]=gpState[p].stress;
          input.defgradOld[p]={1.0+gpState[p].strain[0],2.0*gpState[p].strain[1],0.0,
                               0.0,1.0+gpState[p].strain[4],0.0,0.0,0.0,1.0};
          input.defgradNew[p]={1.0+newStrain[0],2.0*newStrain[1],0.0,
                               0.0,1.0+newStrain[4],0.0,0.0,0.0,1.0};
          input.stateOld[p]=gpState[p].state;
          input.internalEnergyOld[p]=gpState[p].internalEnergy;
          input.inelasticEnergyOld[p]=gpState[p].inelasticEnergy;
          gpState[p].strain=newStrain;
        }
      }
      const auto result=adapter.updateBlock(input);
      std::fill(force.begin(),force.end(),Vec2{});
      double averageS=0.0,averageE=0.0,maxEqps=0.0; size_t averageCount=0;
      for(size_t e=0;e<elements.size();++e) {
        const int ix=static_cast<int>(e%static_cast<size_t>(nx));
        for(int q=0;q<4;++q) {
          const size_t p=4*e+static_cast<size_t>(q);
          gpState[p].stress=result.stressFullNew[p];
          gpState[p].state=result.stateNew[p];
          gpState[p].temperature=result.temperatureNew[p];
          gpState[p].internalEnergy=result.internalEnergyNew[p];
          gpState[p].inelasticEnergy=result.inelasticEnergyNew[p];
          allFinite=allFinite&&finiteTensor(gpState[p].stress)&&
              std::all_of(gpState[p].state.begin(),gpState[p].state.end(),[](double z){return std::isfinite(z);});
          const double sxx=gpState[p].stress[0],syy=gpState[p].stress[4],sxy=gpState[p].stress[1];
          const double weight=shape[e][q].detJ*thickness;
          for(int a=0;a<4;++a) {
            const int n=elements[e].node[a];
            force[static_cast<size_t>(n)][0]+=(shape[e][q].gradient[a][0]*sxx+
                                               shape[e][q].gradient[a][1]*sxy)*weight;
            force[static_cast<size_t>(n)][1]+=(shape[e][q].gradient[a][0]*sxy+
                                               shape[e][q].gradient[a][1]*syy)*weight;
          }
          maxEqps=std::max(maxEqps,gpState[p].state[1]);
          if(ix>=2&&ix<nx-2) {averageS+=sxx;averageE+=gpState[p].strain[0];++averageCount;}
        }
      }
      double reaction=0.0;
      std::vector<Vec2> newAcc(x.size());
      for(size_t n=0;n<x.size();++n) {
        newAcc[n][0]=-force[n][0]/mass[n]; newAcc[n][1]=-force[n][1]/mass[n];
        if(right[n]) reaction+=force[n][0]+mass[n]*prescribedA;
      }
      for(size_t n=0;n<x.size();++n) for(int d=0;d<2;++d)
        v[n][d]+=0.5*(acc[n][d]+newAcc[n][d])*dt;
      acc=std::move(newAcc);
      for(size_t n=0;n<x.size();++n) {
        if(left[n]) {u[n][0]=0.0;v[n][0]=0.0;acc[n][0]=0.0;}
        if(right[n]) {u[n][0]=prescribed;v[n][0]=prescribedV;acc[n][0]=prescribedA;}
      }
      u[static_cast<size_t>(anchor)][1]=0.0;v[static_cast<size_t>(anchor)][1]=0.0;acc[static_cast<size_t>(anchor)][1]=0.0;
      if(step%(steps/240)==0||step==1||step==steps)
        series << step << ',' << time << ',' << prescribed << ',' << reaction << ','
               << prescribed/(length-spacing) << ',' << reaction/(height*thickness) << ','
               << averageE/averageCount << ',' << averageS/averageCount << ',' << maxEqps << ','
               << (allFinite?1:0) << '\n';
    }

    // Export final Gauss-point fields for mesh-independent FEM-PD contour plots.
    std::ofstream fields(output/"final_gauss_points.csv");
    fields << "element,gauss,x0_mm,y0_mm,x_mm,y_mm,s11_MPa,eqps\n";
    fields << std::setprecision(16);
    for(size_t e=0;e<elements.size();++e) {
      const int ix=static_cast<int>(e%static_cast<size_t>(nx));
      const int iy=static_cast<int>(e/static_cast<size_t>(nx));
      std::array<Vec2,4> ue{};
      for(int a=0;a<4;++a) ue[a]=u[static_cast<size_t>(elements[e].node[a])];
      for(int q=0;q<4;++q) {
        const double xi=gps[q][0], eta=gps[q][1];
        const std::array<double,4> N = {
            0.25*(1.0-xi)*(1.0-eta), 0.25*(1.0+xi)*(1.0-eta),
            0.25*(1.0+xi)*(1.0+eta), 0.25*(1.0-xi)*(1.0+eta)};
        Vec2 point{0.0,0.0};
        for(int a=0;a<4;++a) {
          point[0] += N[a]*(x[static_cast<size_t>(elements[e].node[a])][0]+ue[a][0]);
          point[1] += N[a]*(x[static_cast<size_t>(elements[e].node[a])][1]+ue[a][1]);
        }
        const auto& state=gpState[4*e+static_cast<size_t>(q)];
        fields << e << ',' << q << ','
               << (ix+(xi+1.0)*0.5)*spacing << ','
               << (iy+(eta+1.0)*0.5)*spacing << ','
               << point[0] << ',' << point[1] << ','
               << state.stress[0] << ',' << state.state[1] << '\n';
      }
    }

    std::ofstream nodes(output/"final_nodes.csv");
    nodes << "node,x0_mm,y0_mm,ux_mm,uy_mm,x_mm,y_mm\n" << std::setprecision(16);
    for(size_t n=0;n<x.size();++n)
      nodes << n << ',' << x[n][0] << ',' << x[n][1] << ',' << u[n][0] << ',' << u[n][1]
            << ',' << x[n][0]+u[n][0] << ',' << x[n][1]+u[n][1] << '\n';
    std::cout << "JC FEM completed: elements=" << elements.size() << " gauss_points="
              << elements.size()*4 << " finite=" << (allFinite?"true":"false") << '\n';
    return allFinite?0:1;
  } catch(const std::exception& error) {
    std::cerr << "JC FEM error: " << error.what() << '\n';
    return 1;
  }
}
