#include "jsd/camera_collision.hpp"
#include <numeric>
#include <stdexcept>
namespace jsd {using namespace rf;
namespace {
V3 lower(V3 a,V3 b){return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)};}
V3 upper(V3 a,V3 b){return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)};}
float component(V3 p,int a){return a==0?p.x:a==1?p.y:p.z;}
bool finite(V3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool box(V3 p,V3 d,V3 lo,V3 hi,float radius,float limit){
 float first=0,last=limit;
 for(int axis=0;axis<3;axis++){
  float start=component(p,axis),delta=component(d,axis),a=component(lo,axis)-radius,b=component(hi,axis)+radius;
  if(std::abs(delta)<1e-10f){if(start<a||start>b)return false;continue;}
  float x=(a-start)/delta,y=(b-start)/delta;if(x>y)std::swap(x,y);first=std::max(first,x);last=std::min(last,y);if(first>last)return false;
 }return true;
}
V3 closest(V3 p,V3 a,V3 b,V3 c){
 V3 ab=b-a,ac=c-a,ap=p-a;float d1=dot(ab,ap),d2=dot(ac,ap);if(d1<=0&&d2<=0)return a;
 V3 bp=p-b;float d3=dot(ab,bp),d4=dot(ac,bp);if(d3>=0&&d4<=d3)return b;
 float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));
 V3 cp=p-c;float d5=dot(ab,cp),d6=dot(ac,cp);if(d6>=0&&d5<=d6)return c;
 float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));
 float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6)));
 float scale=1/(va+vb+vc);return a+ab*(vb*scale)+ac*(vc*scale);
}
}
void CameraCollision::add(std::span<const gfx::Vertex> v,const Mat4&m){
 if(!nodes_.empty())throw std::logic_error("Camera collision field is already built");
 if(v.size()%3)throw std::invalid_argument("Camera collision needs triangles");
 for(size_t j=0;j<v.size();j+=3){V3 a=transformPoint(m,v[j].p),b=transformPoint(m,v[j+1].p),c=transformPoint(m,v[j+2].p);
  if(!finite(a)||!finite(b)||!finite(c))throw std::invalid_argument("Nonfinite camera obstacle");
  if(length(cross(b-a,c-a))<1e-8f)continue;triangles_.push_back({a,b,c,lower(a,lower(b,c)),upper(a,upper(b,c))});}
}
unsigned CameraCollision::partition(unsigned first,unsigned count){
 unsigned id=nodes_.size();nodes_.push_back({});V3 lo=triangles_[first].lo,hi=triangles_[first].hi;
 for(unsigned j=first+1;j<first+count;j++){lo=lower(lo,triangles_[j].lo);hi=upper(hi,triangles_[j].hi);}
 nodes_[id].lo=lo;nodes_[id].hi=hi;
 if(count<=8){nodes_[id].first=first;nodes_[id].count=count;return id;}
 V3 size=hi-lo;int axis=size.x>size.y&&size.x>size.z?0:size.y>size.z?1:2;unsigned half=count/2;
 std::nth_element(triangles_.begin()+first,triangles_.begin()+first+half,triangles_.begin()+first+count,[&](auto&a,auto&b){return component(a.lo+a.hi,axis)<component(b.lo+b.hi,axis);});
 auto left=partition(first,half),right=partition(first+half,count-half);nodes_[id].left=left;nodes_[id].right=right;return id;
}
void CameraCollision::build(){if(!nodes_.empty())throw std::logic_error("Camera BVH already built");if(!triangles_.empty())partition(0,triangles_.size());}
std::optional<CameraHit> CameraCollision::cast(V3 p,V3 d,float radius)const{
 if(!finite(p)||!finite(d)||!std::isfinite(radius)||radius<=0)throw std::invalid_argument("Invalid camera sweep");
 if(nodes_.empty())return {};
 CameraHit best;bool found=false;float speed2=dot(d,d);
 auto accept=[&](float t,V3 n){if(t<-.000001f||t>best.fraction||t>1||length(n)<.001f)return;
  n=normalized(n);if(speed2>1e-12f&&dot(d,n)>=-1e-7f)return;best={std::max(0.f,t),n};found=true;};
 auto sphere=[&](V3 center){V3 q=p-center;float b=dot(q,d),c=dot(q,q)-radius*radius;
  float disc=b*b-speed2*c;if(speed2>1e-12f&&disc>=0){float t=(-b-std::sqrt(disc))/speed2;accept(t,p+d*t-center);}};
 std::array<unsigned,128> stack{};unsigned pending=1;
 while(pending){
  unsigned index=stack[--pending];const auto &node=nodes_[index];
  if(!box(p,d,node.lo,node.hi,radius,best.fraction))continue;
  if(!node.count){stack[pending++]=node.left;stack[pending++]=node.right;continue;}
  for(unsigned j=node.first;j<node.first+node.count;j++){
   auto&t=triangles_[j];V3 n=normalized(cross(t.b-t.a,t.c-t.a)),near=closest(p,t.a,t.b,t.c),away=p-near;
   if(dot(away,away)<radius*radius-1e-7f){V3 normal=length(away)>.00001f?normalized(away):n;
    if(speed2<1e-12f){best={0,normal};found=true;}else accept(0,normal);}
   float start=dot(p-t.a,n),velocity=dot(d,n);
   if(std::abs(velocity)>1e-10f)for(float side:{-1.f,1.f}){
    float time=(side*radius-start)/velocity;if(time<0||time>best.fraction)continue;
    V3 point=p+d*time-n*(side*radius),test=closest(point,t.a,t.b,t.c);
    if(dot(test-point,test-point)<1e-8f)accept(time,n*side);
   }
   for(auto edge:std::array<std::pair<V3,V3>,3>{{{t.a,t.b},{t.b,t.c},{t.c,t.a}}}){
    V3 e=edge.second-edge.first,m=p-edge.first;float ee=dot(e,e);if(ee<1e-12f)continue;
    float ed=dot(e,d),em=dot(e,m),a=speed2-ed*ed/ee,b=dot(m,d)-em*ed/ee,c=dot(m,m)-em*em/ee-radius*radius,disc=b*b-a*c;
    if(a>1e-12f&&disc>=0){float time=(-b-std::sqrt(disc))/a,u=(em+ed*time)/ee;if(u>=0&&u<=1)accept(time,p+d*time-(edge.first+e*u));}
   }
   sphere(t.a);sphere(t.b);sphere(t.c);
  }
 }
 return found?std::optional<CameraHit>(best):std::nullopt;
}
V3 CameraCollision::move(V3 from,V3 desired,float radius,const ExtraCast&extra)const{
 V3 remaining=desired-from;
 for(int pass=0;pass<6&&length(remaining)>1e-6f;pass++){
  auto hit=cast(from,remaining,radius);if(extra){auto next=extra(from,remaining);if(next&&(!hit||next->fraction<hit->fraction))hit=next;}
  if(!hit)return from+remaining;
  float t=std::max(0.f,hit->fraction-.003f/std::max(.003f,length(remaining)));from=from+remaining*t;
  remaining=remaining*(1-t);float inward=dot(remaining,hit->normal);if(inward<0)remaining=remaining-hit->normal*inward;else break;
 }return from;
}
}
