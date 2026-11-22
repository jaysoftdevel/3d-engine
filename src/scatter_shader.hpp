#pragma once
namespace rf::gfx::lighting {
inline const char* scatterVertex=R"GLSL(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;layout(location=2)in vec4 color;layout(location=3)in vec2 uv;
invariant gl_Position;uniform mat4 vp,previousVP,view,previousView;
uniform sampler2D scatterHeight;uniform vec3 scatterCenter,scatterExtent,eye;
uniform vec4 scatterScale,scatterColor;uniform int scatterSeed;
uniform vec4 exclusionLo[2],exclusionHi[2];uniform float windStrength,windTime,viewDistance;
out vec3 N,P,L;out vec4 tint,currentClip,previousClip;out vec2 texcoord,linearDepths;
uint scatterHash(uint x){x^=x>>16u;x*=2146121005u;x^=x>>15u;x*=2221713035u;return x^(x>>16u);}
float random(uint n){return float(scatterHash(n)>>8u)/16777216.;}
void main(){uint id=uint(gl_InstanceID)*5u+uint(scatterSeed);vec2 location=vec2(random(id),random(id+1u));
 vec3 root=scatterCenter+vec3((location.x-.5)*2.*scatterExtent.x,0,(location.y-.5)*2.*scatterExtent.z);
 root.y=texture(scatterHeight,location).r;
 vec2 delta=1./vec2(textureSize(scatterHeight,0)-1);
 float dx=texture(scatterHeight,location+vec2(delta.x,0)).r-texture(scatterHeight,location-vec2(delta.x,0)).r;
 float dz=texture(scatterHeight,location+vec2(0,delta.y)).r-texture(scatterHeight,location-vec2(0,delta.y)).r;
 vec3 up=normalize(vec3(-dx/(4.*scatterExtent.x*delta.x),1,-dz/(4.*scatterExtent.z*delta.y)));
 bool reject=up.y<scatterScale.z||length(root-eye)>viewDistance;
 for(int i=0;i<2;i++)if(exclusionLo[i].w>.5&&all(greaterThanEqual(root,exclusionLo[i].xyz))&&all(lessThanEqual(root,exclusionHi[i].xyz)))reject=true;
 float scale=mix(scatterScale.x,scatterScale.y,random(id+2u)),angle=random(id+3u)*6.283185;
 vec3 t=normalize(cross(vec3(0,0,1),up)),b=cross(up,t);mat3 basis=mat3(t*cos(angle)+b*sin(angle),up,-t*sin(angle)+b*cos(angle));
 root.y+=scale*scatterScale.w;vec3 world=root+basis*position*scale,old=world;
 if(windStrength!=0.){world.xz+=vec2(sin(dot(root.xz,vec2(.37,.21))+windTime*1.9),cos(dot(root.xz,vec2(.19,.28))+windTime*1.3))*windStrength*uv.y*uv.y;
 old.xz+=vec2(sin(dot(root.xz,vec2(.37,.21))+(windTime-.01667)*1.9),cos(dot(root.xz,vec2(.19,.28))+(windTime-.01667)*1.3))*windStrength*uv.y*uv.y;}
 P=world;L=position;N=normalize(basis*normal);texcoord=uv;tint=color*scatterColor*(.82+.3*floor(random(id+4u)*8.)/7.);
 currentClip=vp*vec4(world,1);previousClip=previousVP*vec4(old,1);linearDepths=vec2(-(previousView*vec4(old,1)).z,-(view*vec4(world,1)).z);
 gl_Position=reject?vec4(2,2,2,1):currentClip;
})GLSL";
}
