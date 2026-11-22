#include "internal.hpp"
#include <stdexcept>
namespace rf::gfx {
using namespace vl;
namespace {uint32_t scatterHash(uint32_t x){x^=x>>16;x*=2146121005u;x^=x>>15;x*=2221713035u;return x^(x>>16);}
float scatterRandom(uint32_t x){return float(scatterHash(x)>>8)*(1.f/16777216.f);}}
void Renderer::scatter(const ProceduralScatter&s){
 auto&i=*impl;
 if(!s.id||!i.meshes.contains(s.mesh.value)||!s.height||s.count>32768||!std::isfinite(s.minScale+s.maxScale+s.maxSlope+s.heightOffset)||
    s.minScale<=0||s.maxScale<s.minScale||s.maxSlope<0||s.halfExtent.x<=0||s.halfExtent.z<=0||s.seed>2147483647u)
  throw std::invalid_argument("Invalid procedural scatter");
 for(auto p:{s.center,s.halfExtent})if(!std::isfinite(p.x+p.y+p.z))throw std::invalid_argument("Invalid scatter extent");
 for(float f:{s.material.wetness,s.material.dust,s.material.sheen})if(!std::isfinite(f)||f<0||f>1)throw std::invalid_argument("Invalid scatter material layer");
 if(int(s.material.surface)<0||int(s.material.surface)>6||s.material.surfaceSeed>2147483647u||!std::isfinite(s.material.anisotropy)||std::abs(s.material.anisotropy)>.95f)throw std::invalid_argument("Invalid scatter material recipe");
 for(float f:{s.material.albedo.r,s.material.albedo.g,s.material.albedo.b,s.material.albedo.a,s.material.roughness,s.material.metallic,s.material.textureScale,s.material.normalScale,s.material.emissive.r,s.material.emissive.g,s.material.emissive.b})if(!std::isfinite(f))throw std::invalid_argument("Nonfinite scatter material");
 if(s.id>1000000000ull)throw std::invalid_argument("Scatter ID exceeds capture range");
 for(auto box:s.exclusions)for(auto v:{box.lo,box.hi})if(!std::isfinite(v.x+v.y+v.z))throw std::invalid_argument("Invalid scatter exclusion");
 if(i.rendered)throw std::logic_error("Submit scatter before rendering");
 if(i.scatterCommands.size()>=8)throw std::runtime_error("Scatter submission budget exceeded");
 if(i.observer&&i.observer->recordOnly){
  // Project exports bake placements; interactive runtime generation stays on the GPU.
  for(unsigned j=0;j<s.count;j++){unsigned id=j*5+s.seed;float u=scatterRandom(id),v=scatterRandom(id+1);
   V3 p=s.center+V3{(u-.5f)*2*s.halfExtent.x,0,(v-.5f)*2*s.halfExtent.z};p.y=s.height(p.x,p.z);
   bool excluded=false;for(auto box:s.exclusions)if(box.hi.x>box.lo.x&&p.x>=box.lo.x&&p.x<=box.hi.x&&p.z>=box.lo.z&&p.z<=box.hi.z&&p.y>=box.lo.y&&p.y<=box.hi.y)excluded=true;
   float e=.1f;V3 up=normalized(V3{(s.height(p.x-e,p.z)-s.height(p.x+e,p.z))/(2*e),1,(s.height(p.x,p.z-e)-s.height(p.x,p.z+e))/(2*e)});
   if(excluded||up.y<1/std::sqrt(1+s.maxSlope*s.maxSlope))continue;
   float scale=s.minScale+(s.maxScale-s.minScale)*scatterRandom(id+2),a=scatterRandom(id+3)*2*pi;p.y+=scale*s.heightOffset;
   V3 t=normalized(cross(V3{0,0,1},up)),b=cross(up,t),x=(t*std::cos(a)+b*std::sin(a))*scale,y=up*scale,z=(t*(-std::sin(a))+b*std::cos(a))*scale;
   Mat4 m=Mat4::identity();const float X[]={x.x,x.y,x.z},Y[]={y.x,y.y,y.z},Z[]={z.x,z.y,z.z};for(int r=0;r<3;r++){m.m[r]=X[r];m.m[4+r]=Y[r];m.m[8+r]=Z[r];}m.m[12]=p.x;m.m[13]=p.y;m.m[14]=p.z;
   Draw d;d.mesh=s.mesh;d.material=s.material;float tint=.82f+.3f*std::floor(scatterRandom(id+4)*8)/7;d.material.albedo.r*=tint;d.material.albedo.g*=tint;d.material.albedo.b*=tint;d.modelMatrix=m;d.objectId=0x4000000000000000ull+s.id*100000+j;submit(d);
  }return;
 }
 if(!i.scatterCache.contains(s.id)&&i.scatterCache.size()>=32)throw std::runtime_error("Scatter height cache budget exceeded");
 auto&cache=i.scatterCache[s.id];
 if(!cache.height||cache.revision!=s.revision||distance(cache.center,s.center)>.001f||distance(cache.extent,s.halfExtent)>.001f){
  constexpr int edge=129;cache.samples.resize(edge*edge);
  for(int z=0;z<edge;z++)for(int x=0;x<edge;x++){float h=s.height(s.center.x+(x/128.f-.5f)*2*s.halfExtent.x,s.center.z+(z/128.f-.5f)*2*s.halfExtent.z);
   if(!std::isfinite(h)||std::abs(h)>10000)throw std::invalid_argument("Invalid scatter height");cache.samples[z*edge+x]=h;}
  if(cache.height)i.gl.DeleteTextures(1,&cache.height);cache.height=i.texture(edge,edge,0x822E,0x1903,GL_FLOAT,cache.samples.data());cache.center=s.center;cache.extent=s.halfExtent;cache.revision=s.revision;
 }
 i.scatterCommands.push_back(s);
}
void Renderer::Impl::drawScatter(bool shadow,bool depthOnly){
 if(scatterCommands.empty())return;
 unsigned p=shadow||depthOnly?scatterShadowProgram:compactScene?scatterMinimalProgram:scatterProgram;
 common(p,shadow?lightVP:vp);
 for(auto&s:scatterCommands){
  if(shadow&&!s.material.castShadow)continue;
  if(depthOnly&&s.material.alphaMode!=AlphaMode::Opaque)continue;
  material(p,s.material);bind(p,"scatterHeight",scatterCache.at(s.id).height,16,GL_TEXTURE_2D);
  vector(p,"scatterCenter",s.center);vector(p,"scatterExtent",s.halfExtent);integer(p,"scatterSeed",int(s.seed));
  gl.Uniform4f(uniform(p,"scatterScale"),s.minScale,s.maxScale,1/std::sqrt(1+s.maxSlope*s.maxSlope),s.heightOffset);
  auto c=s.material.albedo;gl.Uniform4f(uniform(p,"scatterColor"),c.r,c.g,c.b,c.a);
  for(int j=0;j<2;j++){auto&box=s.exclusions[j];std::string index="["+std::to_string(j)+"]";
   gl.Uniform4f(uniform(p,("exclusionLo"+index).c_str()),box.lo.x,box.lo.y,box.lo.z,box.hi.x>box.lo.x?1.f:0.f);
   gl.Uniform4f(uniform(p,("exclusionHi"+index).c_str()),box.hi.x,box.hi.y,box.hi.z,0);
  }
  bool equal=depthPrepassed&&!shadow&&!depthOnly&&s.material.alphaMode==AlphaMode::Opaque;
  gl.DepthFunc(equal?0x0202:0x0201);gl.DepthMask(!equal);
  auto&m=meshes.at(s.mesh.value);gl.BindVertexArray(m.vao);
  for(unsigned a=4;a<=15;a++)gl.DisableVertexAttribArray(a);
  if(m.ibo)gl.DrawElementsInstanced(GL_TRIANGLES,m.count,0x1405,nullptr,s.count);else gl.DrawArraysInstanced(GL_TRIANGLES,0,m.count,s.count);
  for(unsigned a=4;a<=15;a++)gl.EnableVertexAttribArray(a);
  if(countDraws){stats.drawCalls++;stats.triangles+=m.count/3*s.count;if(!shadow&&!depthOnly)stats.proceduralInstances+=s.count;}
 }
 gl.DepthFunc(0x0201);gl.DepthMask(1);
}
}
