#pragma once
#include <frontier/detail/light_bounds.hpp>
#include <cmath>
template<class Check> void lightBoundsTests(Check check){
 using namespace rf;using namespace rf::gfx::detail;
 std::array<float,4> bounds;
 check(!lightBounds({40,0,-10},1,.08f,1.2f,1.8f,bounds),"offscreen lights do not occupy edge tiles");
 check(!lightBounds({0,0,3},1,.08f,1.2f,1.8f,bounds),"lights behind the near plane are rejected");
 check(lightBounds({.2f,0,-.5f},1,.08f,1.2f,1.8f,bounds)&&bounds[0]==-1&&bounds[2]==1,
       "near-plane light intersections conservatively cover the viewport");
 bool conservative=true;
 for(V3 p:{V3{4,1,-6},V3{-4,-1,-6},V3{0,0,-10}}){
  bool intersects=lightBounds(p,2,.08f,1.2f,1.8f,bounds);
  for(int i=0;i<=32;i++)for(int j=0;j<64;j++){
   float a=i*pi/32,b=j*2*pi/64;
   auto v=p+V3{2*std::sin(a)*std::cos(b),2*std::cos(a),2*std::sin(a)*std::sin(b)};
   float x=v.x/-v.z*1.2f,y=v.y/-v.z*1.8f;
   if(x>=-1&&x<=1&&y>=-1&&y<=1)
    conservative&=intersects&&x>=bounds[0]-1e-5f&&x<=bounds[2]+1e-5f&&y>=bounds[1]-1e-5f&&y<=bounds[3]+1e-5f;
  }
 }
 check(conservative,"off-axis sphere samples remain inside light tile bounds");
}
