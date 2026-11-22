#include "showcase.hpp"
namespace sundog {
namespace {
float cameraFloor(V3 p){return Showcase::insideCave(p)?8.f:Showcase::height(p.x,p.z);}
std::optional<CameraHit> terrainHit(V3 from,V3 delta){
 constexpr float radius=.3f;
 auto height=[&](V3 p){float floor=cameraFloor(p);for(auto offset:{V3{radius,0,0},V3{-radius,0,0},V3{0,0,radius},V3{0,0,-radius}})floor=std::max(floor,cameraFloor(p+offset));return floor;};
 int steps=std::max(1,int(std::ceil(rf::length(delta)/.2f)));
 for(int i=1;i<=steps;i++){float t=float(i)/steps;auto p=from+delta*t;
  if(p.y-radius>=height(p))continue;
  float a=float(i-1)/steps,b=t;for(int k=0;k<12;k++){float m=(a+b)*.5f;auto q=from+delta*m;if(q.y-radius<height(q))b=m;else a=m;}
  auto q=from+delta*a;float e=.1f;V3 n=rf::normalized(V3{height(q+V3{-e,0,0})-height(q+V3{e,0,0}),2*e,height(q+V3{0,0,-e})-height(q+V3{0,0,e})});
  return CameraHit{a,n};
 }return {};
}
}
void Showcase::buildCameraRoute(){
 auto append=[&](const g::Draw&draw){auto mesh=cameraMeshes.find(draw.mesh.value);if(mesh==cameraMeshes.end())return;
  cameraObstacles.add(mesh->second,draw.modelMatrix.value_or(rf::compose(draw.transform.position,{},draw.transform.scale)));};
 for(auto &draw:scenery)append(draw);for(auto &draw:caveScenery)append(draw);
 for(auto &instance:staticModels)for(auto &part:instance.model->parts){g::Draw d;d.mesh=part.mesh;d.modelMatrix=instance.root;append(d);}
 cameraObstacles.build();
 constexpr int hz=120,count=int(Duration)*hz;cameraRoute.resize(count+1);cameraRoute[0]=cameraAt(0).position;
 for(int j=1;j<=count;j++){
  auto wanted=cameraAt(j/float(hz)).position;V3 delta=wanted-cameraRoute[j-1];float distance=rf::length(delta);
  if(distance>.6f)wanted=cameraRoute[j-1]+delta*(.6f/distance);
  cameraRoute[j]=cameraObstacles.move(cameraRoute[j-1],wanted,.3f,terrainHit);
 }
 // Playback linearly interpolates this swept, fixed-rate path; no spline can
 // overshoot a safe sample into a wall or skim through a thin control panel.
}
g::Camera Showcase::viewAt(float seconds)const{
 auto camera=cameraAt(seconds);if(cameraRoute.empty())return camera;
 float time=std::fmod(std::max(0.f,seconds),Duration)*120;size_t i=std::min(size_t(time),cameraRoute.size()-2);float t=time-i;
 camera.position=rf::mix(cameraRoute[i],cameraRoute[i+1],t);return camera;
}
V3 Showcase::moveCamera(V3 from,V3 desired){
 if(cameraProbe.value==~0u){BodySettings s;s.motion=MotionType::Kinematic;s.sensor=true;s.position=from;cameraProbe=physics.sphere(s,.3f);}
 return cameraObstacles.move(from,desired,.3f,[&](V3 p,V3 delta){
  auto best=terrainHit(p,delta);physics.teleport(cameraProbe,p);auto hit=physics.sweep(cameraProbe,delta,{cameraProbe,false});
  if(hit&&(!best||hit->fraction<best->fraction))best=CameraHit{hit->fraction,hit->normal};return best;
 });
}
}
