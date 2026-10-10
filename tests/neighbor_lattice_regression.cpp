#include "PdCorrespondenceSkeleton.hpp"
#include <algorithm>
#include <iostream>
#include <vector>
int main() {
  for(double h:{2.0/3.0,1.0,2.0}) for(double shift:{0.0,-123.456,10000.125}) {
    const int n=9;
    std::vector<vumat::pd::Vec3> x;
    for(int i=0;i<n;++i)for(int j=0;j<n;++j)for(int k=0;k<n;++k)
      x.push_back({shift+i*h,shift+j*h,shift+k*h});
    auto fast=vumat::pd::buildNeighborList(x,3*h);
    auto slow=vumat::pd::buildNeighborListBruteForce(x,3*h);
    for(int i=0;i<n;++i)for(int j=0;j<n;++j)for(int k=0;k<n;++k) {
      std::vector<int> expected;
      for(int a=0;a<n;++a)for(int b=0;b<n;++b)for(int c=0;c<n;++c) {
        int d=(a-i)*(a-i)+(b-j)*(b-j)+(c-k)*(c-k);
        if(d>0&&d<=9)expected.push_back((a*n+b)*n+c);
      }
      int p=(i*n+j)*n+k;
      if(fast[p]!=expected||slow[p]!=expected) {
        std::cerr<<"FAIL h="<<h<<" shift="<<shift<<" index="<<p<<"\n";return 1;
      }
    }
  }
  std::cout<<"PASS 9 grids; both searches match independent integer-lattice neighborhoods at all nodes\n";
}
