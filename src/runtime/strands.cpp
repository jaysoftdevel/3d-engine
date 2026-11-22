#include "jsd/strands.hpp"
#include <stdexcept>
namespace jsd {
namespace {bool finite(V3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}}
void StrandChain::reset(std::span<const V3> points){
 if(points.size()<2||points.size()>128)throw std::invalid_argument("Strand needs 2 to 128 points");
 std::vector<float> lengths;for(auto p:points)if(!finite(p))throw std::invalid_argument("Invalid strand position");
 for(size_t j=1;j<points.size();j++){float length=rf::distance(points[j],points[j-1]);if(length<.00001f)throw std::invalid_argument("Degenerate strand segment");lengths.push_back(length);}
 points_.assign(points.begin(),points.end());velocity_.assign(points.size(),{});rest_=std::move(lengths);
}
void StrandChain::advance(float seconds,V3 root,V3 acceleration,float damping,const PhysicsWorld*world,Body ignore,float radius){
 if(points_.empty()||!finite(root)||!finite(acceleration)||!std::isfinite(seconds+damping+radius)||seconds<=0||seconds>.25f||damping<0||radius<0)
  throw std::invalid_argument("Invalid strand simulation step");
 int steps=std::max(1,int(std::ceil(seconds*120)));float dt=seconds/steps;V3 oldRoot=points_[0];
 for(int tick=0;tick<steps;tick++){
  auto old=points_;V3 pin=oldRoot+(root-oldRoot)*(float(tick+1)/steps);points_[0]=pin;
  for(size_t j=1;j<points_.size();j++){velocity_[j]=(velocity_[j]+acceleration*dt)*std::exp(-damping*dt);points_[j]=points_[j]+velocity_[j]*dt;}
  for(int iteration=0;iteration<24;iteration++){
   points_[0]=pin;
   for(size_t j=1;j<points_.size();j++){
    V3 delta=points_[j]-points_[j-1];float length=rf::length(delta);if(length<.000001f)continue;
    V3 correction=delta*((length-rest_[j-1])/length);
    if(j==1)points_[j]=points_[j]-correction;
    else{points_[j-1]=points_[j-1]+correction*.5f;points_[j]=points_[j]-correction*.5f;}
   }
   if(world)for(size_t j=1;j<points_.size();j++){
    if(rf::distance(old[j],points_[j])<.000001f)continue;
    auto hit=world->rayCast(old[j],points_[j],{ignore,false});if(hit)points_[j]=hit->position+hit->normal*radius;
   }
  }
  points_[0]=pin;for(size_t j=1;j<points_.size();j++)velocity_[j]=(points_[j]-old[j])/dt;
 }
}
std::vector<gfx::Vertex> strandRibbon(std::span<const V3>points,V3 eye,float width,Color root,Color tip){
 if(points.size()<2||!finite(eye)||!std::isfinite(width)||width<=0)throw std::invalid_argument("Invalid strand ribbon");
 std::vector<gfx::Vertex> out;
 auto vertex=[&](size_t j,float side){
  V3 tangent=rf::normalized(points[std::min(j+1,points.size()-1)]-points[j?j-1:0]);
  V3 normal=rf::normalized(eye-points[j]),right=rf::normalized(rf::cross(tangent,normal));
  if(rf::length(right)<.1f)right=rf::normalized(rf::cross(tangent,std::abs(tangent.y)<.9f?V3{0,1,0}:V3{1,0,0}));
  float u=float(j)/(points.size()-1);Color color{rf::mix(root.r,tip.r,u),rf::mix(root.g,tip.g,u),rf::mix(root.b,tip.b,u),rf::mix(root.a,tip.a,u)};
  return gfx::Vertex{points[j]+right*(side*width*.5f*(1-u*.85f)),normal,color,side*.5f+.5f,u};
 };
 for(size_t j=1;j<points.size();j++){auto a=vertex(j-1,-1),b=vertex(j-1,1),c=vertex(j,1),d=vertex(j,-1);out.insert(out.end(),{a,b,c,a,c,d});}
 return out;
}
}
