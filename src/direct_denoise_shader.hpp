#pragma once
namespace rf::gfx::lighting {
inline const char* directDenoise=R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source,history,depthMap,normalMap,motionMap,surfaceMap,albedoMap;
uniform vec2 pixel,depthTerms;uniform int valid;
float depth(vec2 uv){return depthTerms.y/(texture(depthMap,uv).r*2.-1.+depthTerms.x);}
void main(){
 vec3 n=texture(normalMap,texcoord).xyz*2.-1.,albedo=texture(albedoMap,texcoord).rgb;
 float center=depth(texcoord);vec3 total=vec3(0),minimum=vec3(1e20),maximum=vec3(0);float weights=0.;
 for(int y=-2;y<=2;y++)for(int x=-2;x<=2;x++){
  vec2 uv=texcoord+vec2(x,y)*pixel;vec3 nn=texture(normalMap,uv).xyz*2.-1.,a=texture(albedoMap,uv).rgb;
  float weight=exp(-float(x*x+y*y)*.22-abs(depth(uv)-center)/max(.015,center*.007));
  weight*=pow(max(dot(n,nn),0.),32.)*exp(-length(a-albedo)*16.);
  vec3 value=texture(source,uv).rgb;total+=value*weight;weights+=weight;
  if(abs(x)<=1&&abs(y)<=1){minimum=min(minimum,value);maximum=max(maximum,value);}
 }
 vec3 current=total/max(weights,.00001);vec4 motion=texture(motionMap,texcoord);vec2 oldUV=texcoord-motion.xy;
 vec4 old=texture(history,oldUV);bool accept=valid!=0&&all(greaterThan(oldUV,vec2(0)))&&all(lessThan(oldUV,vec2(1)))&&abs(old.a-motion.z)<max(.03,abs(motion.z)*.01)&&texture(surfaceMap,texcoord).a<.5;
 float blend=accept?.84*exp(-length(motion.xy)*20.):0.;
 color=vec4(mix(current,clamp(old.rgb,minimum,maximum),blend),motion.w);
})GLSL";
}
