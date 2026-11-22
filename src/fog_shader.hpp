#pragma once
namespace rf::gfx::lighting {
inline const char* volumeFog=R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D depthMap,shadowMap;
uniform mat4 invVP,shadowMatrices[4];uniform vec3 eye,sun,sunColor,ambientColor;
uniform float density,falloff,intensity,time;uniform int steps,shadows,volumeCount,lightCount;
uniform vec4 volumePosition[8],volumeExtent[8],volumeColor[8],volumeEmission[8],volumeVelocity[8];
uniform vec4 fogLightPosition[12],fogLightColor[12],fogLightDirection[12],fogLightCone[12];
uniform mat4 localShadowMatrices[4];uniform int fogLightShadow[12];
float lampVisibility(int j,vec3 p){int slot=fogLightShadow[j];if(slot<0)return 1.;
 vec4 q=localShadowMatrices[slot]*vec4(p,1);vec3 uv=q.xyz/q.w*.5+.5;
 if(q.w<=0.||any(lessThan(uv,vec3(.001)))||any(greaterThan(uv,vec3(.999))))return 1.;
 vec2 tile=vec2(float(slot%2)*.5,.5+float(slot/2)*.25);return uv.z-.00025<=texture(shadowMap,tile+uv.xy*vec2(.5,.25)).r?1.:0.;
}
float hash(ivec3 p){uvec3 q=uvec3(p)*1664525u+1013904223u;q.x+=q.y*q.z;q.y+=q.z*q.x;q.z+=q.x*q.y;q^=q>>16u;return float((q.x^q.y^q.z)>>8u)/16777216.;}
float field(vec3 p){ivec3 c=ivec3(floor(p));vec3 f=fract(p);f=f*f*(3.-2.*f);float a=mix(hash(c),hash(c+ivec3(1,0,0)),f.x),b=mix(hash(c+ivec3(0,1,0)),hash(c+ivec3(1,1,0)),f.x),d=mix(hash(c+ivec3(0,0,1)),hash(c+ivec3(1,0,1)),f.x),e=mix(hash(c+ivec3(0,1,1)),hash(c+ivec3(1,1,1)),f.x);return mix(mix(a,b,f.y),mix(d,e,f.y),f.z);}
float phase(float cosine,float g){return (1.-g*g)/(12.56637*pow(max(.01,1.+g*g-2.*g*cosine),1.5));}
float visibility(vec3 p){if(shadows==0)return 1.;for(int c=0;c<4;c++){vec4 q=shadowMatrices[c]*vec4(p,1);vec3 uv=q.xyz/q.w*.5+.5;
 if(all(greaterThan(uv,vec3(.003)))&&all(lessThan(uv,vec3(.997)))){vec2 tile=vec2(float(c%2)*.5,float(c/2)*.25);return uv.z-.0004<=texture(shadowMap,tile+uv.xy*vec2(.5,.25)).r?1.:.03;}}return 1.;}
void main(){float depth=texture(depthMap,texcoord).r;vec4 q=invVP*vec4(texcoord*2.-1.,depth*2.-1.,1);
 vec3 delta=q.xyz/q.w-eye;float distance=min(length(delta),160.),stepSize=distance/float(steps);vec3 ray=normalize(delta),sum=vec3(0);float trans=1.;
 for(int i=0;i<64;i++){if(i>=steps||trans<.004)break;vec3 p=eye+ray*(float(i)+.5)*stepSize;
  float sigma=density*exp(clamp(-p.y*falloff,-8.,4.));vec3 tint=ambientColor*sigma,emission=vec3(0);float g=.25,weightedG=g*sigma;
  for(int j=0;j<8;j++){if(j>=volumeCount)break;vec3 offset=abs((p-volumePosition[j].xyz)/volumeExtent[j].xyz);
   float edge=clamp(min(1.-offset.x,min(1.-offset.y,1.-offset.z))*4.,0.,1.);if(edge<=0.)continue;
   vec3 samplePosition=(p-volumeVelocity[j].xyz*time)*volumeEmission[j].w;
   float turbulent=.3+.9*field(samplePosition)+.3*field(samplePosition*2.13+vec3(29,7,43));
   float amount=volumeExtent[j].w*edge*turbulent;sigma+=amount;tint+=volumeColor[j].rgb*amount;
   emission+=volumeEmission[j].rgb*amount;weightedG+=volumeColor[j].w*amount;
  }
  if(sigma<=.0000001)continue;vec3 albedo=tint/sigma;g=clamp(weightedG/sigma,-.85,.85);
  vec3 incoming=ambientColor+sunColor*intensity*phase(dot(ray,normalize(sun)),g)*visibility(p);
  for(int j=0;j<12;j++){if(j>=lightCount)break;vec4 a=fogLightPosition[j],b=fogLightColor[j],c=fogLightDirection[j],d=fogLightCone[j];vec3 l=a.xyz-p;float dd=dot(l,l);
   if(dd>a.w*a.w||dd<.001)continue;float dist=sqrt(dd),range=max(1.-pow(dist/a.w,4.),0.);
   float attenuation=range*range/(dd+(d.z>.5?max(d.w*d.w,.01):1.));
   if(d.y>.5)attenuation*=smoothstep(c.w,d.x,dot(-l/dist,normalize(c.xyz)));
   incoming+=b.rgb*b.w*attenuation*phase(dot(ray,l/dist),g)*lampVisibility(j,p);
  }
  float attenuation=exp(-sigma*stepSize);sum+=trans*(1.-attenuation)*(albedo*incoming+emission/sigma);trans*=attenuation;
 }color=vec4(clamp(sum,vec3(0),vec3(60000)),trans);
})GLSL";
inline const char* fogComposite=R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source,scattering,depthMap;uniform vec2 depthTerms;
float linearDepth(vec2 uv){float d=texture(depthMap,uv).r;return depthTerms.y/(d*2.-1.+depthTerms.x);}
void main(){vec2 pixel=1./vec2(textureSize(scattering,0));float center=linearDepth(texcoord);vec4 total=vec4(0);float weights=0.;
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){vec2 uv=texcoord+vec2(x,y)*pixel;
  float difference=abs(linearDepth(uv)-center),weight=exp(-float(x*x+y*y)*.65-difference/max(.05,center*.015));
  total+=texture(scattering,uv)*weight;weights+=weight;}
 vec4 fog=total/max(weights,.00001);color=vec4(clamp(texture(source,texcoord).rgb*fog.a+fog.rgb,vec3(0),vec3(60000)),1);
})GLSL";
}
