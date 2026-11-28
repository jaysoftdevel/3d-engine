#pragma once
#include <string>
#include "lighting_shaders.hpp"
namespace rf::gfx::shader {
inline const std::string hybridLightingFragment=std::string(R"GLSL(#version 330 core
#define BLEND_LOCAL_PROBES
in vec2 texcoord;out vec4 outputColor;
uniform sampler2D source,depthMap,normalMap,surfaceMap,aoMap,albedoMap,brdfMap,directRaw,directFiltered;
uniform int denoiseDirect,physicalLighting,customLighting;
uniform samplerCube environmentMap,irradianceMap;
uniform mat4 view,invView;uniform vec4 projectionXY;uniform vec2 depthTerms;
uniform int aoEnabled,giSamples,reflectionSteps,iblEnabled,giMode,lightingPass;
uniform vec3 skyColor,groundColor;uniform float environmentRotation,ambientIntensity;
float viewZ(float d){return -depthTerms.y/(d*2.-1.+depthTerms.x);}
vec3 position(vec2 uv,float d){float z=viewZ(d);vec2 n=uv*2.-1.;return vec3(-z*(n.x+projectionXY.z)/projectionXY.x,-z*(n.y+projectionXY.w)/projectionXY.y,z);}
vec2 screenUV(vec3 q){return vec2(q.x*projectionXY.x+q.z*projectionXY.z,q.y*projectionXY.y+q.z*projectionXY.w)/-q.z*.5+.5;}
vec3 environmentDirection(vec3 d){float c=cos(-environmentRotation),s=sin(-environmentRotation);return vec3(c*d.x+s*d.z,d.y,-s*d.x+c*d.z);}
)GLSL")+lighting::probes+lighting::ambient+R"GLSL(
vec3 radianceAt(vec2 uv,float lod){vec3 value=textureLod(source,uv,lod).rgb;if(denoiseDirect!=0)value+=textureLod(directFiltered,uv,lod).rgb-textureLod(directRaw,uv,lod).rgb;return max(value,vec3(0));}
vec4 traceReflection(vec3 origin,vec3 ray,float rough){
 vec3 q=origin;float previous=1.,travel=0.;
 for(int i=0;i<96;i++){if(i>=reflectionSteps)break;
  float stepLength=max(.048,-origin.z*.0023)*(1.+float(i)*.12);q+=ray*stepLength;travel+=stepLength;
  if(q.z>=-.08)break;vec2 uv=screenUV(q);
  if(any(lessThan(uv,vec2(.002)))||any(greaterThan(uv,vec2(.998))))break;
  float d=texture(depthMap,uv).r;if(d>=.999999){previous=1.;continue;}
  float delta=viewZ(d)-q.z,thickness=.037+travel*.009-q.z*.002;
  if(delta>0.&&delta<thickness+stepLength&&previous<=0.&&i>1){
   vec3 lo=q-ray*stepLength,hi=q;
   for(int k=0;k<5;k++){vec3 mid=(lo+hi)*.5;vec2 m=screenUV(mid);
    if(viewZ(texture(depthMap,m).r)>mid.z)hi=mid;else lo=mid;}
   uv=screenUV(hi);float gap=viewZ(texture(depthMap,uv).r)-hi.z;
   vec3 hn=normalize(mat3(view)*(texture(normalMap,uv).xyz*2.-1.));
   float edge=smoothstep(.003,.075,min(min(uv.x,uv.y),min(1.-uv.x,1.-uv.y)));
   float confidence=edge*(1.-smoothstep(thickness,thickness*1.9,abs(gap)))*
                    smoothstep(-.05,.19,dot(hn,-ray))*smoothstep(.16,.55,travel);
   if(confidence>.01){
    float footprint=rough*rough*travel*float(textureSize(source,0).y)*.16/max(-hi.z,.8);
    float lod=clamp(log2(1.+footprint),0.,7.);
    return vec4(radianceAt(uv,lod),confidence);
   }
  }previous=delta;
 }return vec4(0);
}
vec3 reflectionFallback(vec3 world,vec3 ray,float rough,vec2 weights){
 vec3 radiance=backgroundSpecular(ray,rough);
 return probeSpecular(radiance,world,ray,rough*probeMaxLod,weights);
}
void main(){
 vec3 base=radianceAt(texcoord,0.);
 ivec2 pixel=min(ivec2(texcoord*vec2(textureSize(depthMap,0))),textureSize(depthMap,0)-1);
 float depth=texelFetch(depthMap,pixel,0).r;
 if(depth>=.999999){outputColor=lightingPass==0?vec4(base,1):vec4(0);return;}
 vec4 normalSample=texelFetch(normalMap,pixel,0);
 vec3 p=position(texcoord,depth),nw=normalize(normalSample.xyz*2.-1.),n=normalize(mat3(view)*nw),v=normalize(-p);
 vec3 world=(invView*vec4(p,1)).xyz;vec4 surface=texelFetch(surfaceMap,pixel,0),albedo=texelFetch(albedoMap,pixel,0);
 float rough=surface.x,metal=surface.y,ao=aoEnabled!=0?texture(aoMap,texcoord).r:1.;
 float coat=albedo.a,coatRough=clamp(normalSample.a,.06,1.);
 if(lightingPass==0){
  vec3 vw=normalize(mat3(invView)*v);
  vec3 primary=indirectLighting(albedo.rgb,nw,vw,rough,metal,world,surface.z,coat,coatRough,false);
  vec3 ambient=indirectLighting(albedo.rgb,nw,vw,rough,metal,world,surface.z,coat,coatRough,true);
  vec3 result=customLighting!=0?base*(.4+.6*ao):max(base-primary,vec3(0))+ambient*ao;
  outputColor=vec4(clamp(result,vec3(0),vec3(60000)),1);return;
 }
 if(lightingPass==1){vec3 bounce=vec3(0);float total=0.;
  for(int i=0;i<32;i++){if(i>=giSamples)break;float a=float(i)*2.399963,scale=sqrt((float(i)+.5)/float(giSamples));
   vec2 uv=texcoord+vec2(cos(a),sin(a))*.14*scale/max(1.,-p.z*.045);
   if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))continue;
   float d=texture(depthMap,uv).r;if(d>=.999999)continue;vec3 q=position(uv,d),delta=q-p;float distance=length(delta);
   if(distance<.12||distance>7.)continue;vec3 direction=delta/distance;
   vec3 qn=normalize(mat3(view)*(texture(normalMap,uv).xyz*2.-1.));
   float edge=min(min(uv.x,uv.y),min(1.-uv.x,1.-uv.y));
   float weight=max(dot(n,direction),0.)*max(dot(qn,-direction),0.)*exp(-distance*.4)*smoothstep(0.,.06,edge);
   bounce+=min(radianceAt(uv,1.),vec3(12))*weight;total+=weight;
  }
  outputColor=vec4(bounce/max(total,1.)*albedo.rgb*(1.-metal)*.32,0);return;
 }
 vec3 result=vec3(0);
 float response=mix(rough,min(rough,coatRough),coat);
 if(reflectionSteps>0){
  float nv=clamp(dot(n,v),.001,1.);
  float specAO=clamp(pow(nv+ao,exp2(-16.*rough-1.))-1.+ao,0.,1.);
  // Both screen-space and probe contributions are multiplied by specAO.
  if(specAO==0.){outputColor=vec4(0,0,0,response);return;}
  vec3 reflected=normalize(reflect(-v,n)),worldRay=normalize(mat3(invView)*reflected);
  vec2 pw=probeWeights(world,true);
  vec3 origin=p+n*clamp(-p.z*.0025,.028,.095);
  vec3 radiance=reflectionFallback(world,worldRay,rough,pw);
  vec4 mainHit=rough<.80?traceReflection(origin,reflected,rough):vec4(0);
  radiance=mix(radiance,mainHit.rgb,mainHit.a*(1.-smoothstep(.49,.80,rough)));
  vec3 f0=mix(vec3(.04),albedo.rgb,metal);vec2 lut=iblEnabled!=0?mix(vec2(1,0),texture(brdfMap,vec2(nv,rough)).rg,iblWeight):vec2(1,0);
  float coatF=.04+.96*pow(1.-nv,5.);
  f0=mix(f0,max(vec3(1.-rough),f0),pow(1.-nv,5.)*(1.-iblWeight));
  result+=radiance*(f0*lut.x+lut.y)*specAO*(1.-coat*coatF);
  if(coat>.012){
   vec3 coatRadiance=reflectionFallback(world,worldRay,coatRough,pw);
   vec4 hit=mainHit;
   if(abs(coatRough-rough)>.085 && coatRough<.72)hit=traceReflection(origin,reflected,coatRough);
   coatRadiance=mix(coatRadiance,hit.rgb,hit.a*(1.-smoothstep(.42,.76,coatRough)));
   result+=coatRadiance*coatF*coat*specAO;
  }
 }
 outputColor=vec4(clamp(result,vec3(0),vec3(60000)),response);
})GLSL";
}
