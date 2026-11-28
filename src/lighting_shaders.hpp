#pragma once
#include <string>
#include "sky_shader.hpp"
namespace rf::gfx::lighting {
inline const char* probes=R"PROBE(
uniform samplerCube localProbe,localIrradiance;
uniform vec3 probePosition,probeExtent;
uniform float probeBlend,probeMaxLod,probeReady;
uniform int hybridReflection;
#ifdef BLEND_LOCAL_PROBES
uniform samplerCube secondProbe,secondIrradiance;
uniform vec3 secondPosition,secondExtent;
uniform float secondBlend,secondReady;
#endif
vec3 boxDirection(vec3 p,vec3 r,vec3 center,vec3 extent){
 vec3 safeR=mix(mix(vec3(-.00001),vec3(.00001),greaterThanEqual(r,vec3(0))),r,greaterThan(abs(r),vec3(.00001)));
 vec3 a=(center-extent-p)/safeR,b=(center+extent-p)/safeR;
 vec3 farPoint=max(a,b);float t=min(farPoint.x,min(farPoint.y,farPoint.z));
 return p+r*max(t,0.)-center;
}
float probeCoverage(vec3 p,vec3 center,vec3 extent,float fade,float ready){
 vec3 margin=extent-abs(p-center);
 float edge=min(margin.x,min(margin.y,margin.z));
 return ready*smoothstep(0.,max(fade,.001),edge);
}
vec2 probeWeights(vec3 p,bool blend){
 float a=probeCoverage(p,probePosition,probeExtent,probeBlend,probeReady),b=0.;
#ifdef BLEND_LOCAL_PROBES
 if(blend)b=probeCoverage(p,secondPosition,secondExtent,secondBlend,secondReady);
#endif
 float coverage=max(a,b);
 vec2 rank=vec2(a/(1.+dot(p-probePosition,p-probePosition)*.00035),b);
#ifdef BLEND_LOCAL_PROBES
 if(b>0.)rank.y/=1.+dot(p-secondPosition,p-secondPosition)*.00035;
#endif
 return rank*(coverage/max(rank.x+rank.y,.00001));
}
vec3 probeDiffuse(vec3 fallback,vec3 n,vec2 w){
 vec3 value=fallback*(1.-w.x-w.y);
 if(w.x>0.)value+=texture(localIrradiance,n).rgb*w.x;
#ifdef BLEND_LOCAL_PROBES
 if(w.y>0.)value+=texture(secondIrradiance,n).rgb*w.y;
#endif
 return value;
}
vec3 probeSpecular(vec3 fallback,vec3 p,vec3 r,float lod,vec2 w){
 vec3 value=fallback*(1.-w.x-w.y);
 if(w.x>0.)value+=textureLod(localProbe,boxDirection(p,r,probePosition,probeExtent),lod).rgb*w.x;
#ifdef BLEND_LOCAL_PROBES
 if(w.y>0.)value+=textureLod(secondProbe,boxDirection(p,r,secondPosition,secondExtent),lod).rgb*w.y;
#endif
 return value;
}
)PROBE";
// Shared by forward shading and the AO composite: occlusion attenuates only
// indirect illumination, leaving direct lights and emissive surfaces intact.
inline const char* ambient=R"AMBIENT(
uniform float iblWeight;
vec3 skyRadiance(vec3 ray,float roughness){
 // The horizon contributes even before the first local cubemap has been captured.
 float sky=smoothstep(-.19,.31,ray.y+roughness*.10);
 return mix(groundColor*.23,skyColor*mix(.40,.67,clamp(ray.y,0.,1.)),sky)*ambientIntensity;
}
vec3 backgroundSpecular(vec3 ray,float rough){
 vec3 color=skyRadiance(ray,rough);
 if(iblEnabled!=0)color=mix(color,textureLod(environmentMap,environmentDirection(ray),rough*7.).rgb*ambientIntensity,iblWeight);
 return color;
}
vec3 indirectLighting(vec3 base,vec3 n,vec3 v,float r,float metal,vec3 p,float orm,
                      float coat,float coatRoughness,bool blend){
 if(giMode==0)return base*(physicalLighting!=0?0.:.055);
 vec3 hemisphere=mix(groundColor,skyColor,n.y*.5+.5)*ambientIntensity;
 vec3 value=base*hemisphere*(1.-metal*.55);
 vec2 pw=probeWeights(p,blend);
 float nv=clamp(dot(n,v),.001,1.);vec3 f0=mix(vec3(.04),base,metal);
 float coatF=.04+.96*pow(1.-nv,5.);
 if(iblEnabled!=0||pw.x+pw.y>0.){
  vec3 fresnel=f0+(max(vec3(1.-r),f0)-f0)*pow(1.-nv,5.);
  vec3 diffuse=iblEnabled!=0?texture(irradianceMap,environmentDirection(n)).rgb*ambientIntensity*iblWeight:vec3(0);
  value=probeDiffuse(diffuse,n,pw)*base*(1.-metal)*(1.-fresnel);
  value+=base*hemisphere*(1.-metal*.55)*(1.-pw.x-pw.y)*(1.-iblWeight);
 }
 if(hybridReflection==0){
  vec3 ray=reflect(-v,n);
  vec3 radiance=backgroundSpecular(ray,r);
  radiance=probeSpecular(radiance,p,ray,r*probeMaxLod,pw);
  vec2 lut=iblEnabled!=0?mix(vec2(1,0),texture(brdfMap,vec2(nv,r)).rg,iblWeight):vec2(1,0);
  value+=radiance*(f0*lut.x+lut.y)*(1.-coat*coatF);
  if(coat>0.){
   vec3 top=backgroundSpecular(ray,coatRoughness);
   value+=probeSpecular(top,p,ray,coatRoughness*probeMaxLod,pw)*coat*coatF;
  }
 }
 return value*orm;
}
)AMBIENT";
inline const char* shadows=R"SHADOW(
uniform mat4 shadowMatrices[4];uniform vec4 shadowSplits;
uniform int cascadeCount;uniform sampler2D shadowMap;uniform float sunAngularRadius;
vec3 cascadeUV(vec3 p,int c){vec4 q=shadowMatrices[c]*vec4(p,1);return q.xyz/q.w*.5+.5;}
float cascadeShadow(vec3 n,int c,vec3 uv){
 if(any(lessThan(uv,vec3(.001)))||any(greaterThan(uv,vec3(.999))))return 1.;
 vec2 tile=vec2(float(c%2)*.5,float(c/2)*.25),extent=vec2(.5,.25),size=vec2(textureSize(shadowPcf,0));
 mat4 matrix=shadowMatrices[c];
 vec3 x=vec3(matrix[0].x,matrix[1].x,matrix[2].x),y=vec3(matrix[0].y,matrix[1].y,matrix[2].y),
      z=vec3(matrix[0].z,matrix[1].z,matrix[2].z);
 float texel=2./(length(x)*size.x*extent.x),depthScale=length(z)*.5;
 float nl=clamp(dot(n,sun),0.,1.),shift=min(.065,texel*(.20+.38*(1.-nl)));
 uv+=vec3(dot(x,n),dot(y,n),dot(z,n))*(.5*shift);
 float reference=uv.z-max(.0018,texel*.09)*depthScale;
 // Adjust each tap's comparison for the slope of the receiver in shadow-map space.
 vec2 slope=clamp(vec2(dot(normalize(x),n),dot(normalize(y),n))/max(nl,.23),vec2(-2.),vec2(2.))*texel*depthScale;
 vec2 center=tile+uv.xy*extent;float radius=shadowQuality<=1?.6:1.25;
 if(shadowQuality>=3){float total=0.,count=0.;for(int i=0;i<8;i++){
  float angle=float(i)*2.399963;vec2 o=vec2(cos(angle),sin(angle))*2.65;
  float d=texture(shadowMap,clamp(center+o/size,tile+2./size,tile+extent-2./size)).r;
  float gap=reference+dot(slope,o)-d;
  if(gap>texel*depthScale*.22){total+=gap;count+=1.;}}
  if(count>0.)radius=clamp(1.+total/count/(depthScale*texel)*tan(sunAngularRadius),1.,6.);
 }
 float result=0.;int taps=shadowQuality<=1?4:shadowQuality==2?8:16;
 for(int i=0;i<16;i++){if(i>=taps)break;float a=float(i)*2.399963;
  vec2 o=vec2(cos(a),sin(a))*sqrt((float(i)+.5)/float(taps))*radius;
  result+=texture(shadowPcf,vec3(clamp(center+o/size,tile+1./size,tile+extent-1./size),reference+dot(slope,o)));}
 return result/float(taps);
}
float shadow(vec3 n){
 if(shadowQuality==0||receiveShadow==0||cascadeCount==0)return 1.;
 float z=-(view*vec4(P,1)).z;int c=0;
 if(z>shadowSplits.x)c=1;if(z>shadowSplits.y)c=2;if(z>shadowSplits.z)c=3;
 c=min(c,cascadeCount-1);vec3 uv=cascadeUV(P,c);
 while(c<cascadeCount-1&&(any(lessThan(uv,vec3(.002)))||any(greaterThan(uv,vec3(.998))))){c++;uv=cascadeUV(P,c);}
 float value=cascadeShadow(n,c,uv);
 if(c<cascadeCount-1){float start=c==0?0.:shadowSplits[c-1],end=shadowSplits[c];
  float blend=smoothstep(mix(start,end,.85),end,z);if(blend>0.)value=mix(value,cascadeShadow(n,c+1,cascadeUV(P,c+1)),blend);}
 else value=mix(value,1.,smoothstep(shadowSplits[c]*.88,shadowSplits[c],z));
 return value;
}
)SHADOW";
inline const char* direct=R"DIRECT(
uniform sampler2D contactDepth;uniform mat4 contactVP,contactInvVP;
uniform int contactShadows,directSamples,stochasticFrame;
uniform mat4 localShadowMatrices[4];uniform int localShadowLights[4];uniform vec4 localShadowRanges[4];
float localDepth(float d,vec4 range){return range.x*range.y/max(range.y-d*(range.y-range.x),.0001);}
float mappedLocalVisibility(int index,vec3 p,vec3 n){
 for(int j=0;j<4;j++){if(localShadowLights[j]!=index)continue;
  vec4 range=localShadowRanges[j],unbiased=localShadowMatrices[j]*vec4(p,1);
  vec2 size=vec2(textureSize(shadowMap,0));
  float texel=max(0.,4.*range.w*unbiased.w/size.x);
  vec4 clip=localShadowMatrices[j]*vec4(p+n*clamp(texel*.42,.002,.018),1);
  vec3 uv=clip.xyz/clip.w*.5+.5;
  if(clip.w<=0.||any(lessThan(uv,vec3(.001)))||any(greaterThan(uv,vec3(.999))))return 1.;
  vec2 extent=vec2(.5,.25),tile=vec2(float(j%2)*.5,.5+float(j/2)*.25),center=tile+uv.xy*extent;
  float reference=uv.z-max(.002,texel*.14)*range.x/max(clip.w*clip.w,.01),radius=.9;
  if(shadowQuality>=3){float sum=0.,count=0.;for(int k=0;k<4;k++){
   float a=float(k)*2.399963;vec2 sampleUV=clamp(center+vec2(cos(a),sin(a))*2.5/size,tile+1./size,tile+extent-1./size);
   float blocker=texture(shadowMap,sampleUV).r;if(blocker<reference){sum+=localDepth(blocker,range);count+=1.;}}
   if(count>0.){float receiver=localDepth(uv.z,range),blocker=sum/count;
    radius=clamp(range.z*(receiver-blocker)/max(blocker,.05)*(.5*size.x)/max(2.*range.w*receiver,.001),.75,5.);}
  }
  float visible=0.;int taps=shadowQuality<=2?4:8;for(int k=0;k<8;k++){if(k>=taps)break;
   float a=float(k)*2.399963;vec2 offset=vec2(cos(a),sin(a))*sqrt((float(k)+.5)/float(taps))*radius/size;
   visible+=texture(shadowPcf,vec3(clamp(center+offset,tile+1./size,tile+extent-1./size),reference));}
  return visible/float(taps);
 }return 1.;
}
float directHash(uvec2 p){uint v=p.x*1664525u+p.y*1013904223u+uint(stochasticFrame)*747796405u;v^=v>>16u;v*=2246822519u;return float(v>>8u)/16777216.;}
float contactVisibility(vec3 n,vec3 ray,float limit,int steps){
 if(contactShadows==0||limit<.08)return 1.;
 vec3 start=P+n*.026;float visibility=1.;
 for(int i=1;i<=12;i++){if(i>steps)break;
  float fraction=float(i)/float(steps+1),t=limit*fraction*(.48+.52*fraction);
  vec4 clip=contactVP*vec4(start+ray*t,1);
  if(clip.w<=0.)break;vec3 uv=clip.xyz/clip.w*.5+.5;
  if(any(lessThan(uv.xy,vec2(.002)))||any(greaterThan(uv.xy,vec2(.998))))break;
  float d=texture(contactDepth,uv.xy).r;if(d>=.999999)continue;
  vec4 actual=contactInvVP*vec4(uv.xy*2.-1.,d*2.-1.,1);
  float gap=clip.w-1./actual.w;
  float edge=smoothstep(.003,.035,min(min(uv.x,uv.y),min(1.-uv.x,1.-uv.y)));
  float thickness=.11+t*.024;
  float blocked=smoothstep(.021,.047,gap)*(1.-smoothstep(thickness,thickness*1.8,gap))*edge;
  visibility=min(visibility,1.-blocked*(1.-.64*fraction));
  if(visibility<.18)break;
 }return visibility;
}
vec3 evaluateLocal(int index,vec3 base,vec3 n,vec3 v,float r,float metal){
 vec4 a=texelFetch(lightData,index*4);
 vec3 delta=a.xyz-P;float dd=dot(delta,delta);
 if(dd>=a.w*a.w||dd<.000001||dot(n,delta)<=0.)return vec3(0);
 vec4 b=texelFetch(lightData,index*4+1),c=texelFetch(lightData,index*4+2),d=texelFetch(lightData,index*4+3);
 float dist=sqrt(dd),range=max(1.-pow(dist/a.w,4.),0.);
 float attenuation=d.z>.5?range*range/max(dd,d.w*d.w):range*range/(dd+1.);
 if(d.y>.5)attenuation*=smoothstep(c.w,d.x,dot(-delta/dist,normalize(c.xyz)));
 if(attenuation<.000001)return vec3(0);
 emitterRadius=d.w;emitterDistance=dist;
 vec3 reflected=reflect(-v,n),towardRay=reflected*max(dot(delta,reflected),0.)-delta;
 emitterSpecular=safeNormal(delta+towardRay*min(1.,d.w/max(length(towardRay),.0001)),delta/dist);
 float visibility=1.;
 if(receiveShadow!=0 && shadowQuality!=0){
  visibility=mappedLocalVisibility(index,P,geometricNormal);
  if(visibility>.08 && dot(geometricNormal,delta)>0.)
   visibility=min(visibility,contactVisibility(geometricNormal,delta/dist,min(dist,5.),shadowQuality>=3?12:7));
 }
 return layeredBrdf(base,n,v,delta/dist,r,metal)*b.rgb*b.w*attenuation*visibility;
}
vec3 localLighting(vec3 base,vec3 n,vec3 v,float r,float metal){
 if(localLightCount==0)return vec3(0);
 ivec2 tile=ivec2(gl_FragCoord.xy)/16;uvec2 range=texelFetch(lightGrid,tile.y*lightGridWidth+tile.x).rg;
 vec3 result=vec3(0);int budget=directSamples;
 if(budget==0||int(range.y)<=budget){for(uint i=0u;i<128u;i++){if(i>=range.y)break;
  result+=evaluateLocal(int(texelFetch(lightIndices,int(range.x+i)).r),base,n,v,r,metal);}return result;}
 float weights[128];float sum=0.;
 for(uint j=0u;j<128u;j++){if(j>=range.y)break;
  int index=int(texelFetch(lightIndices,int(range.x+j)).r)*4;
  vec4 a=texelFetch(lightData,index),b=texelFetch(lightData,index+1),c=texelFetch(lightData,index+2),d=texelFetch(lightData,index+3);
  vec3 delta=a.xyz-P;float dd=max(dot(delta,delta),.0001),dist=sqrt(dd),nl=max(dot(n,delta/dist),0.);
  float attenuation=pow(max(1.-pow(dist/a.w,4.),0.),2.)/(d.z>.5?max(dd,d.w*d.w):dd+1.);
  if(d.y>.5)attenuation*=smoothstep(c.w,d.x,dot(-delta/dist,normalize(c.xyz)));
  vec3 h=safeNormal(v+delta/dist,n);float nh=max(dot(n,h),0.),a2=pow(r,4.);
  float spec=a2/max(3.141593*pow(nh*nh*(a2-1.)+1.,2.),.000001);
  float weight=max(.00000001,dot(b.rgb,vec3(.2126,.7152,.0722))*b.w*attenuation*nl*(.05+(1.-metal)*.31831+spec*(.04+metal)));
  sum+=weight;weights[j]=sum;
 }
 float jitter=directHash(uvec2(gl_FragCoord.xy));
 for(int s=0;s<32;s++){if(s>=budget)break;float target=(float(s)+jitter)/float(budget)*sum;
  int lo=0,hi=int(range.y)-1;for(int k=0;k<7;k++){int mid=(lo+hi)/2;if(weights[mid]<target)lo=mid+1;else hi=mid;}
  int index=int(texelFetch(lightIndices,int(range.x)+lo).r);
  float prior=lo==0?0.:weights[lo-1],probability=max((weights[lo]-prior)/max(sum,.00000001),.000000001);
  result+=evaluateLocal(index,base,n,v,r,metal)/(probability*float(budget));
 }return result;
}
)DIRECT";
inline const char* layers=R"LAYER(
uniform float wetness,dust,sheen,anisotropy,wetnessVariation;
vec3 layerTangent,geometricNormal;
float coatAmount,coatRoughness;
float emitterRadius=0.,emitterDistance=1.;vec3 emitterSpecular=vec3(0,1,0);
void layerFrame(vec3 n){
 vec3 fallback=safeNormal(cross(abs(n.y)<.95?vec3(0,1,0):vec3(1,0,0),n),vec3(1,0,0));
 vec3 dp1=dFdx(P),dp2=dFdy(P);vec2 uv1=dFdx(texcoord),uv2=dFdy(texcoord);vec3 flow=dp2*uv1.x-dp1*uv2.x;
 layerTangent=safeNormal(flow-n*dot(n,flow),fallback);
}
vec3 layeredBrdf(vec3 base,vec3 n,vec3 v,vec3 l,float r,float metal){
 vec3 specularL=emitterRadius>0.?emitterSpecular:l,h=safeNormal(v+specularL,n);float nl=max(dot(n,l),0.),specularNl=max(dot(n,specularL),0.),nv=clamp(dot(n,v),.001,1.),nh=clamp(dot(n,h),0.,1.),vh=clamp(dot(v,h),0.,1.);
 float a=min(1.,max(r*r,.0036)+emitterRadius/max(2.*emitterDistance,.001)),a2=a*a,denom=nh*nh*(a2-1.)+1.;
 float D=a2/max(3.14159265*denom*denom,1.e-10);
 if(abs(anisotropy)>.001){vec3 t=layerTangent,b=cross(n,t);
  float aspect=sqrt(1.-.9*abs(anisotropy)),ax=a/aspect,ay=a*aspect;
  if(anisotropy<0.){float swap=ax;ax=ay;ay=swap;}
  float d=pow(dot(h,t)/ax,2.)+pow(dot(h,b)/ay,2.)+nh*nh;D=1./max(3.14159265*ax*ay*d*d,1.e-10);}
 float k=pow(r+1.,2.)/8.,G=nv/(nv*(1.-k)+k)*specularNl/max(specularNl*(1.-k)+k,.001);
 vec3 f0=mix(vec3(.04),base,metal),F=f0+(1.-f0)*pow(1.-vh,5.);
 float coatF=.04+.96*pow(1.-vh,5.);
 vec3 value=((1.-F)*(1.-metal)*base/3.14159265*nl+D*G*F/max(4.*nv,.001))*(1.-coatAmount*coatF);
 if(coatAmount>0.){
  float coatAlpha=min(1.,coatRoughness*coatRoughness+emitterRadius/max(2.*emitterDistance,.001));
  value+=brdf(vec3(.04),n,v,specularL,sqrt(coatAlpha),1.)*coatAmount;
 }
 value+=sheen*base*pow(1.-vh,5.)*nl*(1.-metal)*.5;
 return value;
}
void materialLayers(inout vec3 base,inout float r,inout float metal,vec3 n,float retention,float variance){
 float cover=dust*smoothstep(-.15,.85,n.y);base=mix(base,vec3(.25,.18,.105),cover);
 float film=wetness*mix(1.,retention,wetnessVariation);
 r=mix(r,.9,cover);metal*=1.-cover;base*=1.-film*.31;
 r=mix(r,mix(.40,.22,retention),film*.60);
 coatAmount=1.-(1.-clearcoat)*(1.-film);
 coatRoughness=clamp(sqrt(pow(mix(clearcoatRoughness,max(.062,clearcoatRoughness*.64),film),2.)+variance*.18),.06,1.);
}
)LAYER";
inline const std::string probeSky=std::string(R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform mat4 invViewProjection;uniform vec3 eye;
)GLSL")+shader::skyDome+R"GLSL(
void main(){vec4 p=invViewProjection*vec4(texcoord*2.-1.,1,1);vec3 d=normalize(p.xyz/p.w-eye);
 gl_FragDepth=1.;color=vec4(skyDomeColor(d,skyColor),1);})GLSL";
inline const char* probeFilter=R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform samplerCube source;uniform int face,irradiance;uniform float roughness;
float radical(uint bits){bits=(bits<<16u)|(bits>>16u);bits=((bits&0x55555555u)<<1u)|((bits&0xAAAAAAAAu)>>1u);bits=((bits&0x33333333u)<<2u)|((bits&0xCCCCCCCCu)>>2u);bits=((bits&0x0F0F0F0Fu)<<4u)|((bits&0xF0F0F0F0u)>>4u);bits=((bits&0x00FF00FFu)<<8u)|((bits&0xFF00FF00u)>>8u);return float(bits)*2.3283064365386963e-10;}
vec3 direction(vec2 uv){vec2 p=uv*2.-1.;if(face==0)return normalize(vec3(1,-p.y,-p.x));if(face==1)return normalize(vec3(-1,-p.y,p.x));if(face==2)return normalize(vec3(p.x,1,p.y));if(face==3)return normalize(vec3(p.x,-1,-p.y));if(face==4)return normalize(vec3(p.x,-p.y,1));return normalize(vec3(-p.x,-p.y,-1));}
void main(){vec3 n=direction(texcoord),up=abs(n.y)<.99?vec3(0,1,0):vec3(1,0,0),t=normalize(cross(up,n)),b=cross(n,t),sum=vec3(0);float weights=0.;
 for(uint i=0u;i<32u;i++){float x=float(i)/32.,y=radical(i),a=6.283185*x;
  float ct=irradiance!=0?sqrt(1.-y):sqrt((1.-y)/(1.+(pow(max(roughness,.001),4.)-1.)*y));
  vec3 h=normalize(t*cos(a)*sqrt(1.-ct*ct)+b*sin(a)*sqrt(1.-ct*ct)+n*ct),l=irradiance!=0?h:reflect(-n,h);
  float w=irradiance!=0?1.:max(dot(n,l),0.);sum+=textureLod(source,l,roughness*3.).rgb*w;weights+=w;}
 color=vec4(sum/max(weights,.001),1);})GLSL";
}
