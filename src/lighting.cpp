#include "internal.hpp"
#include "shaders.hpp"
#include "lighting_shaders.hpp"
#include "scatter_shader.hpp"
#include "direct_denoise_shader.hpp"
#include <stdexcept>
namespace rf::gfx {
using namespace vl;
namespace {
bool finite(V3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
Mat4 cubeView(V3 p,int face){
 const V3 directions[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
 const V3 ups[]={{0,-1,0},{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};
 V3 f=directions[face],s=normalized(cross(f,ups[face])),u=cross(s,f);Mat4 m=Mat4::identity();
 m.m[0]=s.x;m.m[4]=s.y;m.m[8]=s.z;m.m[1]=u.x;m.m[5]=u.y;m.m[9]=u.z;
 m.m[2]=-f.x;m.m[6]=-f.y;m.m[10]=-f.z;m.m[12]=-dot(s,p);m.m[13]=-dot(u,p);m.m[14]=dot(f,p);return m;
}
}
void Renderer::reflectionProbe(const ReflectionProbe&p){
 if(!p.id||!finite(p.position)||!finite(p.halfExtent)||p.halfExtent.x<=0||p.halfExtent.y<=0||p.halfExtent.z<=0||
    !std::isfinite(p.blendDistance)||p.blendDistance<0||p.refreshFrames>1000000)throw std::invalid_argument("Invalid reflection probe");
 auto&i=*impl;if(i.rendered)throw std::logic_error("Submit probes before rendering");
 const auto rank=[&](const ReflectionProbe& probe) {
     return std::pair{distance(probe.position,i.camera.position),probe.id};
 };
 for (auto& slot:i.requestedProbes) {
     if (slot && slot->id==p.id) { slot=p; return; }
 }
 auto candidate=std::optional{p};
 for (auto& slot:i.requestedProbes) {
     if (!slot || rank(*candidate)<rank(*slot)) std::swap(candidate,slot);
     if (!candidate) break;
 }
}
void Renderer::fogVolume(const FogVolume&v){
 if(!finite(v.position)||!finite(v.halfExtent)||!finite(v.velocity)||v.halfExtent.x<=0||v.halfExtent.y<=0||v.halfExtent.z<=0||
    !std::isfinite(v.density)||v.density<0||v.density>4||!std::isfinite(v.noiseScale)||v.noiseScale<=0||
    !std::isfinite(v.anisotropy)||std::abs(v.anisotropy)>.9f)throw std::invalid_argument("Invalid fog volume");
 for(float f:{v.albedo.r,v.albedo.g,v.albedo.b,v.emission.r,v.emission.g,v.emission.b})
  if(!std::isfinite(f)||f<0)throw std::invalid_argument("Invalid fog color");
 if(impl->rendered)throw std::logic_error("Submit fog before rendering");
 if(impl->fogVolumes.size()>=8)throw std::runtime_error("Fog volume budget exceeded");
 impl->fogVolumes.push_back(v);
}
void Renderer::Impl::initializeLighting(){
 directDenoiseProgram=program(shader::fullscreen,lighting::directDenoise);
 scatterProgram=program(lighting::scatterVertex,shader::sceneFragment);scatterMinimalProgram=program(lighting::scatterVertex,shader::minimalFragment);scatterShadowProgram=program(lighting::scatterVertex,shader::shadowFragment);
 probeSkyProgram=program(shader::fullscreen,lighting::probeSky);
 probeFilterProgram=program(shader::fullscreen,lighting::probeFilter);
}
void Renderer::Impl::releaseLighting(){
 for(auto&[id,p]:probeCache){(void)id;for(auto t:{p.raw,p.specular,p.irradiance,p.pendingSpecular,p.pendingIrradiance})if(t)gl.DeleteTextures(1,&t);
  if(p.fbo)gl.DeleteFramebuffers(1,&p.fbo);if(p.depth)gl.DeleteRenderbuffers(1,&p.depth);}
 for(auto &[id,s]:scatterCache){(void)id;if(s.height)gl.DeleteTextures(1,&s.height);}scatterCache.clear();
 gl.DeleteProgram(directDenoiseProgram);gl.DeleteProgram(scatterProgram);gl.DeleteProgram(scatterMinimalProgram);gl.DeleteProgram(scatterShadowProgram);
 probeCache.clear();gl.DeleteProgram(probeSkyProgram);gl.DeleteProgram(probeFilterProgram);
}
void Renderer::Impl::bindProbe(unsigned p){
 const auto shade=[&](const ProbeCache* probe,const char* position,const char* extent,const char* blend,
                      const char* readiness,const char* specular,const char* irradiance,int specUnit,int irrUnit) {
     const bool ready=probe && probe->ready && !probeCapture;
     const auto spec=ready?probe->spec:ReflectionProbe{};
     scalar(p,readiness,ready?std::min(1.f,float(frameIndex-probe->firstPublish+1)/14.f):0.f);
     vector(p,position,spec.position);
     vector(p,extent,spec.halfExtent);
     scalar(p,blend,spec.blendDistance);
     bind(p,specular,ready?probe->specular:0,specUnit,0x8513);
     bind(p,irradiance,ready?probe->irradiance:0,irrUnit,0x8513);
 };
 shade(activeProbes[0],"probePosition","probeExtent","probeBlend","probeReady","localProbe","localIrradiance",14,5);
 if (p==lightingProgram)
     shade(activeProbes[1],"secondPosition","secondExtent","secondBlend","secondReady","secondProbe","secondIrradiance",12,15);
 scalar(p,"probeMaxLod",7);
}
void Renderer::Impl::drawShadows(){
 cascadeCount=0;if(!shadowFbo)return;
 cascadeCount=4;stats.shadowCascades=4;
 float far=environment.shadowDistance>0?std::clamp(environment.shadowDistance,16.f,250.f):std::min(settings().viewDistance*.7f,180.f);
 const float fractions[]={.1f,.25f,.5f,1};float begin=camera.nearPlane;
 V3 f=normalized(camera.direction),sun=normalized(environment.sunDirection);if(length(sun)<.1f)sun={.3f,.8f,.2f};
 const auto previousMatrices = shadowMatrices;
 const bool sunLit = environment.sunIntensity > 0;
 for(int c=0;c<4;c++){
  float end=far*fractions[c];shadowSplits[c]=end;
  V3 center=camera.position+f*((begin+end)*.5f);
  float h=end*std::tan(camera.fovDegrees*pi/360),w=h*float(outputWidth)/outputHeight;
  float extent=std::ceil(std::sqrt(h*h+w*w+std::pow((end-begin)*.5f,2.f))*16)/16;
  auto lv=lookAt(center+sun*(extent+far),center);auto origin=transformPoint(lv,{});
  float texel=2*extent/std::max(1,shadowSize/2);
  lv.m[12]+=std::round(origin.x/texel)*texel-origin.x;lv.m[13]+=std::round(origin.y/texel)*texel-origin.y;
  shadowMatrices[c]=orthographic(-extent,extent,-extent,extent,.1f,2*(extent+far))*lv;
  begin=end*.88f;
 }
 lightVP=shadowMatrices.back();

 // The upper atlas belongs to the sun. Reuse it only when the exact projection
 // and every input to the mesh depth pass match. Streaming terrain and GPU
 // scatter have independent mesh lifetimes, so they always render here.
 const bool cacheable = !sunLit || (!terrainRequested && scatterCommands.empty());
 bool animated = false;
 sunShadowCasters.clear();
 if(cacheable && sunLit){
     for(size_t index:opaqueOrder){
         const auto &draw = prepared[index];
         const auto &material = draw.source->material;
         if(!material.castShadow)continue;
         const bool masked = material.alphaMode == AlphaMode::Mask;
         const unsigned maskTexture = masked && material.surface == SurfaceKind::Textured
             ? material.albedoTexture.value : 0u;
         animated |= material.windStrength != 0;
         sunShadowCasters.push_back({draw.source->mesh.value, draw.mesh->revision, maskTexture,
             material.alphaMode, masked ? material.alphaCutoff : 0.f,
             masked ? material.albedo.a : 0.f, material.windStrength, draw.instance.model.m});
     }
 }
 const std::array shadowView{camera.position.x, camera.position.y, camera.position.z,
     camera.fovDegrees, camera.nearPlane, settings().viewDistance,
     animated ? environment.atmosphere.time : 0.f};
 const bool sameMatrices = std::equal(shadowMatrices.begin(), shadowMatrices.end(),
     previousMatrices.begin(), [](const Mat4 &a, const Mat4 &b){return a.m == b.m;});
 if(cacheable && sunShadowsValid && sameMatrices && shadowView == previousSunShadowView &&
    sunShadowCasters == previousSunShadowCasters)return;

 sunShadowsValid = false;
 gl.BindFramebuffer(GL_FRAMEBUFFER,shadowFbo);
 gl.Enable(0x0C11);gl.Scissor(0,0,shadowSize,shadowSize);gl.Clear(0x100);gl.Disable(0x0C11);
 if(sunLit){
     gl.Enable(0x8037);gl.PolygonOffset(1.15f,1.6f);
     for(int c=0;c<4;c++){
         lightVP=shadowMatrices[c];
         gl.Viewport((c%2)*shadowSize/2,(c/2)*shadowSize/2,shadowSize/2,shadowSize/2);
         drawCommands(shadowProgram,lightVP,true);
         if(terrainRequested)drawTerrain(true);
         drawScatter(true);
     }
     gl.Disable(0x8037);
 }
 lightVP=shadowMatrices.back();
 if(cacheable){
     previousSunShadowView = shadowView;
     previousSunShadowCasters.swap(sunShadowCasters);
     sunShadowsValid = true;
 }
}
void Renderer::Impl::updateProbe(){
 activeProbes={};if(!requestedProbes[0]||compactScene||
   (settings().globalIllumination!=GlobalIllumination::Hybrid&&settings().globalIllumination!=GlobalIllumination::ProbeLite&&settings().reflections==Quality::Off))return;
 const auto sunDirection=normalized(environment.sunDirection);
 const bool imageLit=environment.image.value && environment.imageLightingWeight>.12f;
 const auto recordLighting=[&](ProbeCache& probe) {
     probe.sun=environment.sunIntensity;
     probe.storm=environment.storm;
     probe.imageWeight=environment.imageLightingWeight;
     probe.imageRotation=environment.imageRotation;
     probe.sunDirection=sunDirection;
     probe.environment=environment.image.value;
 };
 ProbeCache* work=nullptr;
 for (unsigned i=0;i<requestedProbes.size() && requestedProbes[i];++i) {
     const auto& spec=*requestedProbes[i];
     if (!probeCache.contains(spec.id) && probeCache.size()>=8) {
         auto oldest=probeCache.end();
         for (auto it=probeCache.begin();it!=probeCache.end();++it) {
             if (std::any_of(requestedProbes.begin(),requestedProbes.end(),
                 [&](const auto& request){return request && request->id==it->first;})) continue;
             if (oldest==probeCache.end() || it->second.lastRequested<oldest->second.lastRequested) oldest=it;
         }
         auto& evicted=oldest->second;
         for(auto t:{evicted.raw,evicted.specular,evicted.irradiance,evicted.pendingSpecular,evicted.pendingIrradiance})
             if(t)gl.DeleteTextures(1,&t);
         if(evicted.fbo)gl.DeleteFramebuffers(1,&evicted.fbo);
         if(evicted.depth)gl.DeleteRenderbuffers(1,&evicted.depth);
         probeCache.erase(oldest);
     }
     auto& cached=probeCache[spec.id];
     const bool changed=cached.spec.revision!=spec.revision || cached.environment!=environment.image.value ||
         distance(cached.spec.position,spec.position)>.001f;
     const bool lightingChanged=std::abs(cached.sun-environment.sunIntensity)>.52f ||
         std::abs(cached.storm-environment.storm)>.36f ||
         (environment.image.value && std::abs(cached.imageWeight-environment.imageLightingWeight)>.22f) ||
         (imageLit && std::cos(cached.imageRotation-environment.imageRotation)<.977f) ||
         (environment.sunIntensity>.1f && distance(cached.sunDirection,sunDirection)>.24f);
     cached.spec=spec;
     cached.lastRequested=frameIndex;
     if (changed || lightingChanged) {
         if (changed) cached.ready=false;
         cached.nextFace=cached.filterTask=0;
         cached.refreshing=true;
         recordLighting(cached);
     }
     const bool due=spec.refreshFrames && frameIndex-cached.lastFrame>=spec.refreshFrames;
     if (!cached.ready || due) cached.refreshing=true;
     if (cached.refreshing && (!work || (!work->nextFace && cached.nextFace))) work=&cached;
     activeProbes[i]=&cached;
     stats.activeReflectionProbes+=unsigned(cached.ready);
 }
 if (!work) return;
 auto& p=*work;
 const auto& spec=p.spec;
 constexpr int size=128;
 if(!p.raw){
  auto cube=[&](int edge,int levels){unsigned t;gl.GenTextures(1,&t);gl.BindTexture(0x8513,t);
   for(int mip=0;mip<levels;mip++)for(int face=0;face<6;face++)gl.TexImage2D(0x8515+face,mip,GL_RGBA16F,std::max(1,edge>>mip),std::max(1,edge>>mip),0,GL_RGBA,GL_FLOAT,nullptr);
   gl.TexParameteri(0x8513,GL_TEXTURE_MIN_FILTER,levels>1?0x2703:GL_LINEAR);gl.TexParameteri(0x8513,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   for(auto e:{0x2802,0x2803,0x8072})gl.TexParameteri(0x8513,e,GL_CLAMP_TO_EDGE);gl.TexParameteri(0x8513,0x813D,levels-1);return t;};
  p.raw=cube(size,8);p.specular=cube(size,8);p.irradiance=cube(16,1);p.pendingSpecular=cube(size,8);p.pendingIrradiance=cube(16,1);
  gl.GenFramebuffers(1,&p.fbo);gl.GenRenderbuffers(1,&p.depth);gl.BindRenderbuffer(0x8D41,p.depth);gl.RenderbufferStorage(0x8D41,0x81A6,size,size);
 }
 auto savedCamera=camera;auto savedView=view,savedProjection=projection,savedVP=vp,savedInv=invProjection;
 auto savedPreviousVP=previousVP,savedPreviousView=previousView;int savedWidth=width,savedHeight=height;
 bool savedCount=countDraws;countDraws=false;probeCapture=true;depthPrepassed=false;
 camera.position=spec.position;camera.fovDegrees=90;camera.nearPlane=.08f;width=height=size;
 projection=perspective(pi*.5f,1,.08f,settings().viewDistance);invProjection=inverse(projection);
 gl.BindFramebuffer(GL_FRAMEBUFFER,p.fbo);gl.FramebufferRenderbuffer(GL_FRAMEBUFFER,0x8D00,0x8D41,p.depth);gl.DrawBuffer(GL_COLOR_ATTACHMENT0);
 unsigned faceBudget=1;
 for(unsigned update=0;update<faceBudget&&p.nextFace<6;update++,p.nextFace++){
  int face=int(p.nextFace);view=cubeView(camera.position,face);vp=projection*view;previousVP=vp;previousView=view;
  camera.direction={-view.m[2],-view.m[6],-view.m[10]};uploadLights();
  gl.BindFramebuffer(GL_FRAMEBUFFER,p.fbo);gl.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,0x8515+face,p.raw,0);gl.Viewport(0,0,size,size);
  if(gl.CheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Reflection capture incomplete");
  gl.Disable(GL_BLEND);gl.Enable(0x0B71);gl.DepthFunc(0x0201);gl.DepthMask(1);gl.ClearColor(environment.skyColor.r,environment.skyColor.g,environment.skyColor.b,1);gl.Clear(GL_COLOR_BUFFER_BIT|0x100);
  drawCommands(sceneProgram,vp,false);if(terrainRequested)drawTerrain(false);drawScatter(false);
  gl.DepthFunc(0x0203);gl.DepthMask(0);gl.UseProgram(probeSkyProgram);matrix(probeSkyProgram,"invViewProjection",inverse(vp));vector(probeSkyProgram,"eye",camera.position);
  bindSky(probeSkyProgram);fullscreen(probeSkyProgram,p.fbo);gl.DepthMask(1);gl.DepthFunc(0x0201);stats.reflectionProbeFaces++;
 }
 if(p.nextFace==6&&stats.reflectionProbeFaces==0){
  gl.Disable(0x0B71);gl.BindTexture(0x8513,p.raw);if(p.filterTask==0)gl.GenerateMipmap(0x8513);
  gl.UseProgram(probeFilterProgram);bind(probeFilterProgram,"source",p.raw,0,0x8513);
  gl.FramebufferRenderbuffer(GL_FRAMEBUFFER,0x8D00,0x8D41,0);
  for(unsigned work=0;work<6&&p.filterTask<54;work++,p.filterTask++){
   unsigned task=p.filterTask;int mode=task>=48,mip=mode?0:int(task/6),face=mode?int(task-48):int(task%6);
   gl.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,0x8515+face,mode?p.pendingIrradiance:p.pendingSpecular,mip);
   integer(probeFilterProgram,"face",face);integer(probeFilterProgram,"irradiance",mode);scalar(probeFilterProgram,"roughness",float(mip)/7);
   int edge=mode?16:std::max(1,size>>mip);gl.Viewport(0,0,edge,edge);gl.BindVertexArray(fullscreenVao);gl.DrawArrays(GL_TRIANGLES,0,3);stats.reflectionProbeFilters++;
  }
  if(p.filterTask==54){
   std::swap(p.specular,p.pendingSpecular);std::swap(p.irradiance,p.pendingIrradiance);
   if(!p.ready){p.firstPublish=frameIndex;++stats.activeReflectionProbes;}
   p.ready=true;p.refreshing=false;p.nextFace=0;p.filterTask=0;p.lastFrame=frameIndex;
   recordLighting(p);
  }
 }
 camera=savedCamera;view=savedView;projection=savedProjection;vp=savedVP;invProjection=savedInv;previousVP=savedPreviousVP;previousView=savedPreviousView;
 width=savedWidth;height=savedHeight;countDraws=savedCount;probeCapture=false;uploadLights();
 gl.Viewport(0,0,width,height);
}
}
