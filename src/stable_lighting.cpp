#include "internal.hpp"
#include "shaders.hpp"

namespace rf::gfx {
using namespace vl;

namespace {
constexpr char geometryFragment[]=R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;
uniform sampler2D normals,motion,depthMap;
void main(){
 ivec2 pixel=min(ivec2(texcoord*vec2(textureSize(depthMap,0))),textureSize(depthMap,0)-1);
 float depth=texelFetch(depthMap,pixel,0).r;
 color=depth>=.999999?vec4(0):vec4(texelFetch(normals,pixel,0).rgb,texelFetch(motion,pixel,0).w);
}
)GLSL";

constexpr char resolveFragment[]=R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;
uniform sampler2D current,history,geometry,prior,motion,surface;
uniform float historyWeight;
uniform int valid;
vec4 at(sampler2D tex,ivec2 p){
 return texelFetch(tex,clamp(p,ivec2(0),textureSize(tex,0)-1),0);
}
void main(){
 ivec2 pixel=ivec2(gl_FragCoord.xy);
 vec4 g=at(geometry,pixel);
 if(g.a<=0.){color=vec4(0);return;}
 vec4 sampleValue=at(current,pixel);
 vec3 n=g.rgb*2.-1.,value=sampleValue.rgb;
 vec3 low=value,high=value,sum=value;
 float count=1.;
 for(int i=0;i<4;++i){
  ivec2 offset=i<2?ivec2(i==0?1:-1,0):ivec2(0,i==2?1:-1);
  ivec2 uv=pixel+offset;
  vec4 other=at(geometry,uv);
  float dd=abs(other.a-g.a);
  if(other.a<=0.||dd>max(.09,g.a*.035)||dot(n,other.rgb*2.-1.)<.88)continue;
  vec4 c=at(current,uv);
  if(abs(c.a-sampleValue.a)>.14)continue;
  low=min(low,c.rgb);high=max(high,c.rgb);
  float weight=.28*exp(-dd/max(.037,g.a*.018));
  sum+=c.rgb*weight;count+=weight;
 }
 vec3 filtered=sum/count;
 vec4 mv=texture(motion,texcoord);
 vec2 prev=texcoord-mv.xy;
 if(valid!=0&&all(greaterThan(prev,vec2(0)))&&all(lessThan(prev,vec2(1)))){
  ivec2 pp=ivec2(prev*vec2(textureSize(prior,0)));
  vec4 last=at(prior,pp);
  vec4 held=at(history,pp);
  if(last.a>0.&&abs(last.a-mv.z)<max(.065,abs(mv.z)*.014)&&dot(n,last.rgb*2.-1.)>.90&&abs(held.a-sampleValue.a)<.13){
   float reactive=texture(surface,texcoord).a;
   vec3 pad=(high-low)*.12+max(high,vec3(.03))*.045;
   vec3 old=clamp(held.rgb,max(low-pad,vec3(0)),high+pad);
   float weight=historyWeight*exp(-length(mv.xy)*10.)*(1.-.77*reactive);
   filtered=mix(filtered,old,weight);
  }
 }
 color=vec4(filtered,sampleValue.a);
}
)GLSL";

constexpr char addFragment[]=R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;
uniform sampler2D reflections,bounce,geometry,normals,motion,depthMap,surface,albedo;
uniform int useReflection,useBounce;
void main(){
 if(texture(depthMap,texcoord).r>=.999999){color=vec4(0);return;}
 vec4 normal=texture(normals,texcoord);
 vec3 n=normal.rgb*2.-1.;
 float d=texture(motion,texcoord).w;
 float r=texture(surface,texcoord).r,rough=mix(r,min(r,normal.a),texture(albedo,texcoord).a);
 ivec2 dim=textureSize(geometry,0);
 vec2 p=texcoord*vec2(dim)-.5, fraction=fract(p);
 ivec2 origin=ivec2(floor(p));
 vec3 reflection=vec3(0),indirect=vec3(0);float reflectionWeight=0.,indirectWeight=0.;
 for(int y=0;y<=1;++y)for(int x=0;x<=1;++x){
  ivec2 q=clamp(origin+ivec2(x,y),ivec2(0),dim-1);
  vec4 g=texelFetch(geometry,q,0);
  if(g.a<=0.)continue;
  float dd=abs(d-g.a), facing=max(0.,dot(n,g.rgb*2.-1.));
  float w=(x==0?1.-fraction.x:fraction.x)*(y==0?1.-fraction.y:fraction.y);
  w*=pow(facing,16.)*exp(-dd/max(.04,d*.026));
  if(useReflection!=0){
   vec4 sampleValue=texelFetch(reflections,q,0);
   float materialWeight=w*(1.-smoothstep(.048,.23,abs(sampleValue.a-rough)));
   reflection+=sampleValue.rgb*materialWeight;
   reflectionWeight+=materialWeight;
  }
  if(useBounce!=0){indirect+=texelFetch(bounce,q,0).rgb*w;indirectWeight+=w;}
 }
 color=vec4(reflection/max(reflectionWeight,.06)+indirect/max(indirectWeight,.06),0.);
}
)GLSL";
}

void Renderer::Impl::initializeStableLighting() {
    lightingGeometryProgram=program(shader::fullscreen,geometryFragment);
    lightingResolveProgram=program(shader::fullscreen,resolveFragment);
    lightingAddProgram=program(shader::fullscreen,addFragment);
    for (auto& slot:passTimings)
        for (auto& stage:slot) gl.GenQueries(2,stage.query.data());
}
void Renderer::Impl::releaseStableLighting() {
    for (const auto program:{lightingGeometryProgram,lightingResolveProgram,lightingAddProgram})
        if (program) gl.DeleteProgram(program);
    for (const auto& slot:passTimings)
        for (const auto& stage:slot) gl.DeleteQueries(2,stage.query.data());
}
void Renderer::Impl::collectProfiles() {
    auto& slot=passTimings[queryIndex%passTimings.size()];
    const std::array values{&stats.reflectionMilliseconds,&stats.indirectMilliseconds,&stats.probeMilliseconds};
    for (unsigned i=0;i<slot.size();++i) {
        auto& stage=slot[i];
        if (!stage.pending) continue;
        int available=0;
        gl.GetQueryObjectiv(stage.query[1],0x8867,&available);
        if (!available) continue;
        std::array<unsigned long long,2> timestamps{};
        for (unsigned n=0;n<timestamps.size();++n)
            gl.GetQueryObjectui64v(stage.query[n],0x8866,&timestamps[n]);
        if (timestamps[1]>=timestamps[0]) *values[i]=double(timestamps[1]-timestamps[0])/1000000.;
        stage.pending=false;
    }
}
bool Renderer::Impl::beginProfile(ProfilePass pass) {
    if (!settings().gpuProfiling) return false;
    auto& stage=passTimings[queryIndex%passTimings.size()][unsigned(pass)];
    if (stage.pending) return false;
    gl.QueryCounter(stage.query[0],0x8E28);
    return true;
}
void Renderer::Impl::finishProfile(ProfilePass pass,bool started) {
    if (!started) return;
    auto& stage=passTimings[queryIndex%passTimings.size()][unsigned(pass)];
    gl.QueryCounter(stage.query[1],0x8E28);
    stage.pending=true;
}

void Renderer::Impl::allocateLightingHistory() {
    if (compactScene) return;
    const int w=std::max(1,width/2),h=std::max(1,height/2);
    const auto create=[&] { return target(GL_RGBA16F,GL_RGBA,w,h); };
    const auto allocate=[&](LightingHistory& state) {
        state.raw=create();
        for (auto& buffer:state.filtered) buffer=create();
    };
    const auto gi=settings().globalIllumination;
    if (settings().reflections!=Quality::Off) allocate(reflectionHistory);
    if ((gi==GlobalIllumination::Hybrid || gi==GlobalIllumination::ScreenSpace) &&
        settings().illuminationQuality!=Quality::Off) allocate(indirectHistory);
    if (reflectionHistory.raw.fbo || indirectHistory.raw.fbo)
        for (auto& buffer:lightingGeometry) buffer=create();
}
void Renderer::Impl::releaseLightingHistory() {
    for (auto* state:{&reflectionHistory,&indirectHistory}) {
        destroy(state->raw);
        for (auto& buffer:state->filtered) destroy(buffer);
    }
    for (auto& buffer:lightingGeometry) destroy(buffer);
    lightingHistoryIndex=0;
}
void Renderer::Impl::resolveLighting() {
    if (!lightingGeometry.front().fbo) return;
    const int w=std::max(1,width/2),h=std::max(1,height/2);
    const unsigned previous=lightingHistoryIndex,next=1-previous;
    auto p=lightingGeometryProgram;
    gl.UseProgram(p);
    sampler(p,"normals",sceneNormal,0);
    sampler(p,"motion",sceneMotion,1);
    sampler(p,"depthMap",sceneDepth,2);
    fullscreen(p,lightingGeometry[next].fbo,w,h);
    const auto resolve=[&](LightingHistory& state,int pass,float weight,ProfilePass profile) {
        if (!state.raw.fbo) return;
        const bool timed=beginProfile(profile);
        gl.UseProgram(lightingProgram);
        integer(lightingProgram,"lightingPass",pass);
        // The resolve draws bind different inputs to these texture units.
        sampler(lightingProgram,"source",sceneColor,0);
        sampler(lightingProgram,"depthMap",sceneDepth,1);
        sampler(lightingProgram,"normalMap",sceneNormal,2);
        sampler(lightingProgram,"surfaceMap",sceneSurface,3);
        sampler(lightingProgram,"aoMap",aoB.texture,4);
        fullscreen(lightingProgram,state.raw.fbo,w,h);
        const auto shader=lightingResolveProgram;
        gl.UseProgram(shader);
        sampler(shader,"current",state.raw.texture,0);
        sampler(shader,"history",state.filtered[previous].texture,1);
        sampler(shader,"geometry",lightingGeometry[next].texture,2);
        sampler(shader,"prior",lightingGeometry[previous].texture,3);
        sampler(shader,"motion",sceneMotion,4);
        sampler(shader,"surface",sceneSurface,5);
        scalar(shader,"historyWeight",weight);
        integer(shader,"valid",historyValid);
        fullscreen(shader,state.filtered[next].fbo,w,h);
        finishProfile(profile,timed);
    };
    resolve(indirectHistory,1,.82f,ProfilePass::Indirect);
    resolve(reflectionHistory,2,.56f,ProfilePass::Reflection);
    p=lightingAddProgram;
    gl.UseProgram(p);
    sampler(p,"reflections",reflectionHistory.filtered[next].texture,0);
    sampler(p,"bounce",indirectHistory.filtered[next].texture,1);
    sampler(p,"geometry",lightingGeometry[next].texture,2);
    sampler(p,"normals",sceneNormal,3);
    sampler(p,"motion",sceneMotion,4);
    sampler(p,"depthMap",sceneDepth,5);
    sampler(p,"surface",sceneSurface,6);
    sampler(p,"albedo",sceneAlbedo,7);
    integer(p,"useReflection",reflectionHistory.raw.fbo!=0);
    integer(p,"useBounce",indirectHistory.raw.fbo!=0);
    gl.Enable(GL_BLEND);
    gl.BlendFunc(1,1);
    fullscreen(p,lit.fbo);
    gl.Disable(GL_BLEND);
    lightingHistoryIndex=next;
}
}
