#pragma once
#include "../math.hpp"
#include <algorithm>
#include <array>
namespace rf::gfx::detail {
// Project the tangent rays of a view-space sphere. Off-axis perspective
// expansion and a rounding margin preserve lighting at viewport/tile edges.
inline bool lightBounds(V3 p,float radius,float nearPlane,float px,float py,
                        std::array<float,4>& bounds) {
 float z=-p.z;
 if(z+radius<nearPlane)return false;
 bounds={-1,-1,1,1};
 if(z-radius<=nearPlane)return true;
 auto interval=[&](float c,float scale){
  double denominator=double(z)*z-double(radius)*radius;
  double center=double(c)*z/denominator;
  double extent=radius*std::sqrt(double(c)*c+denominator)/denominator;
  return std::array<float,2>{float((center-extent)*scale)-1e-5f,
                             float((center+extent)*scale)+1e-5f};
 };
 auto x=interval(p.x,px),y=interval(p.y,py);
 if(x[1]<-1||x[0]>1||y[1]<-1||y[0]>1)return false;
 bounds={x[0],y[0],x[1],y[1]};return true;
}
}
