#include "internal.hpp"
#include "frontier/detail/light_bounds.hpp"
#include <bit>
#include <numeric>
namespace rf::gfx {
void Renderer::Impl::bindLocalShadows(unsigned program){
 for(unsigned j=0;j<localShadows.size();j++){
  auto &s=localShadows[j];auto suffix="["+std::to_string(j)+"]";
  integer(program,("localShadowLights"+suffix).c_str(),s.ready?s.light:-1);
  matrix(program,("localShadowMatrices"+suffix).c_str(),s.matrix);
  auto &l=s.spec;gl.Uniform4f(uniform(program,"localShadowRanges"+suffix),.05f,l.radius,l.sourceRadiusMetres,std::tan(l.outerCone));
 }
}
void Renderer::Impl::updateLocalShadows(){
 for(auto &s:localShadows)s.light=-1;
 if(!shadowFbo||compactScene)return;
 uint64_t stamp=1469598103934665603ull;
 auto add=[&](uint64_t v){stamp=(stamp^v)*1099511628211ull;};
 for(auto &draw:prepared){auto &m=draw.source->material;if(!m.castShadow||m.alphaMode==AlphaMode::Blend||m.alphaMode==AlphaMode::Additive)continue;
  add(draw.source->mesh.value);add(draw.mesh->revision);add(unsigned(m.alphaMode));add(m.albedoTexture.value);add(std::bit_cast<unsigned>(m.alphaCutoff));add(std::bit_cast<unsigned>(m.albedo.a));
  for(float f:draw.instance.model.m)add(std::bit_cast<unsigned>(f));
  if(m.windStrength!=0)add(std::bit_cast<unsigned>(environment.atmosphere.time));
 }
 if(terrainRequested){add(terrainSpec.seed);add(terrainSpec.revision);for(auto &[key,tile]:tiles){add(unsigned(key.first));add(unsigned(key.second));add(tile.mesh.vao);}}
 for(auto &s:scatterCommands){if(!s.material.castShadow)continue;add(s.id);add(s.revision);add(s.count);add(s.seed);add(s.mesh.value);for(float f:{s.center.x,s.center.y,s.center.z,s.halfExtent.x,s.halfExtent.y,s.halfExtent.z,s.minScale,s.maxScale,s.heightOffset})add(std::bit_cast<unsigned>(f));if(s.material.windStrength!=0)add(std::bit_cast<unsigned>(environment.atmosphere.time));}
 std::vector<size_t> selected;
 for(size_t j=0;j<lights.size();j++){
  auto &l=lights[j];std::array<float,4> bounds;
  if(l.castShadow&&l.intensity>0&&detail::lightBounds(transformPoint(view,l.position),l.radius,camera.nearPlane,projection.m[0],projection.m[5],bounds))selected.push_back(j);
 }
 auto weight=[&](size_t j){auto &l=lights[j];return l.intensity*(.2126f*l.color.r+.7152f*l.color.g+.0722f*l.color.b)/(1+dot(l.position-camera.position,l.position-camera.position));};
 std::stable_sort(selected.begin(),selected.end(),[&](size_t a,size_t b){float x=weight(a),y=weight(b);return x==y?lights[a].shadowId<lights[b].shadowId:x>y;});
 if(selected.size()>4)selected.resize(4);
 for(auto index:selected){auto &l=lights[index];auto slot=std::find_if(localShadows.begin(),localShadows.end(),[&](auto&s){return s.id==l.shadowId;});
  if(slot==localShadows.end())slot=std::find_if(localShadows.begin(),localShadows.end(),[&](auto&s){return std::none_of(selected.begin(),selected.end(),[&](size_t j){return lights[j].shadowId==s.id;});});
  bool changed=slot->id!=l.shadowId||slot->revision!=l.shadowRevision||distance(slot->spec.position,l.position)>.00001f||distance(slot->spec.direction,l.direction)>.00001f||slot->spec.outerCone!=l.outerCone||slot->spec.radius!=l.radius;
  if(changed)slot->ready=false;
  slot->id=l.shadowId;slot->light=int(index);slot->revision=l.shadowRevision;slot->spec=l;
 }
 std::vector<unsigned> work;
 for(unsigned j=0;j<4;j++)if(localShadows[j].light>=0&&(!localShadows[j].ready||localShadows[j].sceneStamp!=stamp))work.push_back(j);
 std::stable_sort(work.begin(),work.end(),[&](unsigned a,unsigned b){auto&A=localShadows[a];auto&B=localShadows[b];return A.ready!=B.ready?!A.ready:A.lastFrame<B.lastFrame;});
 auto savedVP=lightVP;bool savedCount=countDraws;countDraws=false;localShadowPass=true;
 gl.BindFramebuffer(vl::GL_FRAMEBUFFER,shadowFbo);gl.Enable(0x0B71);gl.DepthMask(1);gl.Enable(0x0C11);gl.Enable(0x8037);gl.PolygonOffset(1.1f,1.5f);
 for(unsigned j=0;j<std::min(size_t(2),work.size());j++){
  auto index=work[j];auto&s=localShadows[index];auto&l=s.spec;V3 direction=normalized(l.direction);
  s.matrix=perspective(l.outerCone*2,1,.05f,l.radius)*lookAt(l.position,l.position+direction);
  int edge=shadowSize/2,x=int(index%2)*edge,y=shadowSize+int(index/2)*edge;
  gl.Scissor(x,y,edge,edge);gl.Viewport(x,y,edge,edge);gl.Clear(0x100);lightVP=s.matrix;
  drawCommands(shadowProgram,lightVP,true);if(terrainRequested)drawTerrain(true);drawScatter(true);
  s.ready=true;s.lastFrame=frameIndex;s.sceneStamp=stamp;stats.localShadowUpdates++;
 }
 gl.Disable(0x0C11);gl.Disable(0x8037);lightVP=savedVP;countDraws=savedCount;localShadowPass=false;
 for(auto &s:localShadows)if(s.light>=0&&s.ready)stats.localShadowMaps++;
}
}
