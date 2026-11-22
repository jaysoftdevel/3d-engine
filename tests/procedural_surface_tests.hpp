#pragma once
#include <frontier/renderer.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "process_id.hpp"
template<class Check> void proceduralSurfaceTests(rf::gfx::Renderer&renderer,Check check){
 using namespace rf;using namespace rf::gfx;
 auto path=std::filesystem::temp_directory_path()/("jsd-procedural-"+std::to_string(jsdTestProcessId())+".ppm");
 struct Remove {std::filesystem::path p;~Remove(){std::error_code e;std::filesystem::remove(p,e);}} remove{path};
 auto snapshot=[&](float x,unsigned seed,bool world){
  Camera camera;camera.position={x+1,3,-3};camera.direction=normalized(V3{-.1f,-.6f,1});
  renderer.begin(camera);Material m;m.surface=SurfaceKind::Soil;m.surfaceSeed=seed;m.surfaceWorld=world;m.albedo={.48f,.31f,.19f,1};
  renderer.primitive(Primitive::Box,{{x,-.25f,0},{8,.5f,8},0},m);
  renderer.end();renderer.check();renderer.screenshot(path);
  std::ifstream f(path,std::ios::binary);return std::string((std::istreambuf_iterator<char>(f)),{});
 };
 auto distance=[](const std::string&a,const std::string&b){
  if(a.size()!=b.size())return 1e9;
  double d=0;for(size_t i=0;i<a.size();i++)d+=std::abs(int(static_cast<unsigned char>(a[i]))-int(static_cast<unsigned char>(b[i])));
  return d/a.size();
 };
 for(bool full:{false,true}){
  auto s=GraphicsSettings::minimal();s.viewDistance=64;
  if(full)s.textureQuality=TextureQuality::Ultra;
  renderer.setSettings(s);
  auto a=snapshot(0,17,true),again=snapshot(0,17,true),far=snapshot(64,17,true),seed=snapshot(0,381,true);
  check(a==again,"procedural surfaces are deterministic between frames");
  check(distance(a,far)>.1&&distance(a,seed)>.1,"world positions and recipe seeds create distinct surfaces without a repeating tile");
  auto local=snapshot(0,17,false),moved=snapshot(64,17,false);
  check(distance(local,moved)<.05,"object-local surface detail stays attached while the object moves");
 }
}
