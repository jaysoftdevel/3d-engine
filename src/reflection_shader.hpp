#pragma once
#include <string>
#include "lighting_shaders.hpp"
namespace rf::gfx::shader {
inline const std::string hybridLightingFragment=std::string(R"GLSL(#version 330 core
in vec2 texcoord;out vec4 outputColor;
uniform sampler2D source,depthMap,normalMap,surfaceMap,aoMap,albedoMap,brdfMap,directRaw,directFiltered;
uniform int denoiseDirect,physicalLighting,customLighting;
uniform samplerCube environmentMap,irradianceMap;
uniform mat4 view,invView;uniform vec4 projectionXY;uniform vec2 depthTerms;
uniform int aoEnabled,giSamples,reflectionSteps,iblEnabled,giMode;
uniform vec3 skyColor,groundColor;uniform float environmentRotation,ambientIntensity;
float viewZ(float d){return -depthTerms.y/(d*2.-1.+depthTerms.x);}
vec3 position(vec2 uv,float d){float z=viewZ(d);vec2 n=uv*2.-1.;return vec3(-z*(n.x+projectionXY.z)/projectionXY.x,-z*(n.y+projectionXY.w)/projectionXY.y,z);}
vec2 screenUV(vec3 q){return vec2(q.x*projectionXY.x+q.z*projectionXY.z,q.y*projectionXY.y+q.z*projectionXY.w)/-q.z*.5+.5;}
vec3 environmentDirection(vec3 d){float c=cos(-environmentRotation),s=sin(-environmentRotation);return vec3(c*d.x+s*d.z,d.y,-s*d.x+c*d.z);}
)GLSL")+lighting::probes+lighting::ambient+R"GLSL(
vec3 radianceAt(vec2 uv,float lod){vec3 value=textureLod(source,uv,lod).rgb;if(denoiseDirect!=0)value+=textureLod(directFiltered,uv,lod).rgb-textureLod(directRaw,uv,lod).rgb;return max(value,vec3(0));}
vec4 traceReflection(vec3 origin,vec3 ray,float rough){
 vec3 q=origin;float previous=-1.,travel=0.;
 for(int i=0;i<96;i++){if(i>=reflectionSteps)break;
  float stepLength=max(.055,-origin.z*.0015)*(1.+float(i)*.09);q+=ray*stepLength;travel+=stepLength;
  if(q.z>=-.08)break;vec2 uv=screenUV(q);
  if(any(lessThan(uv,vec2(.002)))||any(greaterThan(uv,vec2(.998))))break;
  float d=texture(depthMap,uv).r;if(d>=.999999){previous=-1.;continue;}
  float delta=viewZ(d)-q.z,thickness=.06+travel*.012;
  if(delta>0.&&delta<thickness+stepLength&&previous<=thickness&&i>2){
   vec3 lo=q-ray*stepLength,hi=q;
   for(int k=0;k<6;k++){vec3 mid=(lo+hi)*.5;vec2 m=screenUV(mid);
    if(viewZ(texture(depthMap,m).r)>mid.z)hi=mid;else lo=mid;}
   vec3 hit=(lo+hi)*.5;uv=screenUV(hit);float gap=abs(viewZ(texture(depthMap,uv).r)-hit.z);
   vec3 hn=normalize(mat3(view)*(texture(normalMap,uv).xyz*2.-1.));
   float edge=clamp(min(min(uv.x,uv.y),min(1.-uv.x,1.-uv.y))*15.,0.,1.);
   float confidence=edge*(1.-smoothstep(thickness,thickness*2.,gap))*smoothstep(-.05,.2,dot(hn,-ray));
   float lod=clamp(log2(1.+rough*rough*travel*8.),0.,7.);
   return vec4(radianceAt(uv,lod),confidence);
  }previous=delta;
 }return vec4(0);
}
void main(){
 vec3 base=radianceAt(texcoord,0.);float depth=texture(depthMap,texcoord).r;
 if(depth>=.999999){outputColor=vec4(base,1);return;}
 vec3 p=position(texcoord,depth),nw=normalize(texture(normalMap,texcoord).xyz*2.-1.),n=normalize(mat3(view)*nw),v=normalize(-p);
 vec3 world=(invView*vec4(p,1)).xyz;vec4 surface=texture(surfaceMap,texcoord),albedo=texture(albedoMap,texcoord);
 float rough=surface.x,metal=surface.y,ao=aoEnabled!=0?texture(aoMap,texcoord).r:1.;
 vec3 ambient=indirectLighting(albedo.rgb,nw,normalize((invView*vec4(v,0)).xyz),rough,metal,world,surface.z);
 vec3 result=customLighting!=0?base*(.4+.6*ao):max(base-ambient,vec3(0))+ambient*ao;
 if(giSamples>0){vec3 bounce=vec3(0);float total=0.;
  for(int i=0;i<32;i++){if(i>=giSamples)break;float a=float(i)*2.399963,scale=sqrt((float(i)+.5)/float(giSamples));
   vec2 uv=texcoord+vec2(cos(a),sin(a))*.14*scale/max(1.,-p.z*.045);
   if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))continue;
   float d=texture(depthMap,uv).r;if(d>=.999999)continue;vec3 q=position(uv,d),delta=q-p;float distance=length(delta);
   if(distance<.12||distance>7.)continue;vec3 direction=delta/distance;
   vec3 qn=normalize(mat3(view)*(texture(normalMap,uv).xyz*2.-1.));
   float weight=max(dot(n,direction),0.)*max(dot(qn,-direction),0.)*exp(-distance*.4);
   bounce+=min(radianceAt(uv,1.),vec3(12))*weight;total+=weight;
  }
  result+=bounce/max(total,1.)*albedo.rgb*(1.-metal)*.32;
 }
 if(reflectionSteps>0){
  float nv=clamp(dot(n,v),.001,1.);
  float specAO=clamp(pow(nv+ao,exp2(-16.*rough-1.))-1.+ao,0.,1.);
  // Both screen-space and probe contributions are multiplied by specAO.
  if(specAO==0.){outputColor=vec4(clamp(result,vec3(0),vec3(60000)),1);return;}
  vec3 reflected=normalize(reflect(-v,n)),worldRay=normalize(mat3(invView)*reflected);
  float pw=probeWeight(world);
  vec3 radiance=iblEnabled!=0?textureLod(environmentMap,environmentDirection(worldRay),rough*7.).rgb*ambientIntensity:skyColor*.15;
  if(pw>0.)radiance=mix(radiance,textureLod(localProbe,boxDirection(world,worldRay),rough*probeMaxLod).rgb,pw);
  vec3 sum=vec3(0);int rays=rough>.6?1:reflectionSteps>=64?3:1;
  vec3 t=normalize(cross(abs(reflected.y)<.95?vec3(0,1,0):vec3(1,0,0),reflected)),b=cross(reflected,t);
  for(int j=0;j<3;j++){if(j>=rays)break;float a=float(j)*2.399963;
   vec3 ray=normalize(reflected+(t*cos(a)+b*sin(a))*rough*rough*.35*(rays>1?1.:0.));
   vec4 hit=rough<.82?traceReflection(p+n*.065,ray,rough):vec4(0);
   sum+=mix(radiance,hit.rgb,hit.a);
  }
  radiance=sum/float(rays);
  vec3 f0=mix(vec3(.04),albedo.rgb,metal);vec2 lut=iblEnabled!=0?texture(brdfMap,vec2(nv,rough)).rg:vec2(1,0);
  float coat=albedo.a,coatF=.04+.96*pow(1.-nv,5.);
  result+=radiance*(f0*lut.x+lut.y)*specAO*(1.-coat*coatF);
  if(coat>0.){
   vec3 coatRadiance=iblEnabled!=0?textureLod(environmentMap,environmentDirection(worldRay),.7).rgb*ambientIntensity:radiance;
   if(pw>0.)coatRadiance=mix(coatRadiance,textureLod(localProbe,boxDirection(world,worldRay),.7).rgb,pw);
   vec4 hit=traceReflection(p+n*.065,reflected,.1);coatRadiance=mix(coatRadiance,hit.rgb,hit.a);
   result+=coatRadiance*coatF*coat*specAO;
  }
 }
 outputColor=vec4(clamp(result,vec3(0),vec3(60000)),1);
})GLSL";
}
