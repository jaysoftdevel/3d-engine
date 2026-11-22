#pragma once
namespace rf::gfx::cinematic {
inline const char *skyVertex = R"GLSL(#version 330 core
out vec2 texcoord;void main(){texcoord=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(texcoord*2.-1.,1,1);})GLSL";
inline const char *sky = R"GLSL(#version 330 core
in vec2 texcoord;
uniform sampler2D skyEquirect; uniform sampler3D noiseVolume;uniform float environmentRotation;
uniform mat4 invProjection,invView,previousVP,starOrientation;
uniform float starIntensity;uniform int starSeed,airlessSky;
uniform vec3 skyColor,eye,sun,sunColor,wind;
uniform float farDistance,skyTime,coverage,cloudDensity,cloudBase,cloudThickness,turbidity;
uniform int enabled,atmosphereEnabled,cloudSteps;
layout(location=0)out vec4 color;layout(location=1)out vec4 normal;
layout(location=2)out vec4 surface;layout(location=3)out vec4 motion;
float density(vec3 p){float h=(p.y-cloudBase)/cloudThickness;float shape=textureLod(noiseVolume,p*.00048+wind*skyTime*.00022,0.).r;float detail=textureLod(noiseVolume,p*.0017-wind*skyTime*.0001,0.).g;float f=shape*.8+detail*.2;float edge=smoothstep(0.,.18,h)*(1.-smoothstep(.62,1.,h));return max(0.,f-(1.-coverage)) * edge * cloudDensity * .13;}
uint starHash(uvec3 p){p=p*1664525u+1013904223u;p.x+=p.y*p.z;p.y+=p.z*p.x;p.z+=p.x*p.y;p^=p>>16u;return p.x^p.y^p.z^uint(starSeed);}
vec3 stars(vec3 direction){
 if(starIntensity<=0.)return vec3(0);
 vec3 ray=normalize(mat3(starOrientation)*direction),p=ray*210.;ivec3 cell=ivec3(floor(p));
 float footprint=max(.035,length(fwidth(p))*.44);vec3 result=vec3(0);
 for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
  ivec3 c=cell+ivec3(x,y,z);uint h=starHash(uvec3(c));if((h&1023u)>12u)continue;
  vec3 random=vec3(float((h>>10u)&127u),float((h>>17u)&127u),float((h>>24u)&127u))/127.;
  vec3 candidate=vec3(c)+random;if(abs(length(candidate)-210.)>.45)continue;
  vec3 star=normalize(candidate)*210.;float distance=length(p-star);
  float radius=.060+.085*random.x*random.x,width=sqrt(radius*radius+footprint*footprint);
  float energy=radius*radius/(width*width)*exp(-distance*distance/(width*width));
  vec3 tint=mix(vec3(.70,.82,1.),vec3(1.,.87,.67),random.y);
  result+=tint*energy*(2.+7.*pow(random.z,8.));
 }
 return result*starIntensity*smoothstep(-.022,.008,ray.y);
}
vec3 atmosphere(vec3 d){float altitude=max(d.y,0.);float haze=pow(1.-altitude,4.);vec3 zenith=vec3(.045,.14,.29),horizon=vec3(.58,.58,.49);float daylight=smoothstep(-.16,.10,sun.y);vec3 result=mix(vec3(.0004,.0007,.0018),mix(zenith,horizon,haze),daylight);float angle=dot(d,sun);result+=sunColor*pow(max(angle,0.),12.)*.18*turbidity*daylight;float sunDisc=smoothstep(cos(.009),cos(.004),angle);result+=sunColor*sunDisc*7.;result=mix(result,vec3(.24,.20,.15)*daylight,(1.-smoothstep(-.12,0.,d.y)));return result;}
void main(){vec4 p=invProjection*vec4(texcoord*2.-1.,1,1);vec3 d=normalize(mat3(invView)*(p.xyz/p.w));vec4 old=previousVP*vec4(d,0);vec2 previousUV=abs(old.w)>.0001?old.xy/old.w*.5+.5:texcoord;float C=cos(-environmentRotation),S=sin(-environmentRotation);vec3 rotated=vec3(C*d.x+S*d.z,d.y,-S*d.x+C*d.z);vec2 envUV=vec2(atan(rotated.z,rotated.x)/6.283185+.5,acos(clamp(rotated.y,-1.,1.))/3.14159265);vec3 background=enabled!=0?textureLod(skyEquirect,envUV,0.).rgb:skyColor*.4;
if(atmosphereEnabled!=0 && enabled==0)background=atmosphere(d);
background+=stars(d);
if(airlessSky!=0){float disc=smoothstep(cos(.0050),cos(.0043),dot(d,sun));background+=sunColor*disc*12.;}
if(atmosphereEnabled!=0){if(cloudSteps>0&&d.y>.012){float entry=max(0.,(cloudBase-eye.y)/d.y),exitPoint=(cloudBase+cloudThickness-eye.y)/d.y;float stepSize=min((exitPoint-entry)/float(cloudSteps),120.);float transmittance=1.;vec3 scattered=vec3(0);float forward=.15+1.4*pow(max(dot(d,sun),0.),10.);for(int i=0;i<32;i++){if(i>=cloudSteps||transmittance<.015)break;vec3 point=eye+d*(entry+(float(i)+.5)*stepSize);float amount=density(point);if(amount>.0001){float extinction=exp(-amount*stepSize);float lightDepth=0.;for(int j=1;j<=3;j++)lightDepth+=density(point+sun*float(j)*18.)*18.;vec3 illumination=vec3(.22,.28,.34)+sunColor*exp(-lightDepth*1.6)*(1.05+forward);scattered+=transmittance*(1.-extinction)*illumination;transmittance*=extinction;}}float horizonFade=smoothstep(.012,.08,d.y);background=mix(background,background*transmittance+scattered,horizonFade);}}
color=vec4(clamp(background,vec3(0),vec3(60000)),1);normal=vec4(.5,1,.5,1);surface=vec4(1,0,1,0);motion=vec4(texcoord-previousUV,farDistance,farDistance);}
)GLSL";
inline const char *particleVertex = R"GLSL(#version 330 core
layout(location=0)in vec4 centerSize;layout(location=1)in vec4 velocityStretch;
layout(location=2)in vec4 rgba;layout(location=3)in vec4 properties;
uniform mat4 vp,view;uniform vec3 right,up;
out vec2 uv;out vec4 tint;out vec4 options;out float linearDepth;out vec3 world;
void main(){vec2 corners[6]=vec2[](vec2(-1,-1),vec2(1,-1),vec2(1,1),vec2(-1,-1),vec2(1,1),vec2(-1,1));uv=corners[gl_VertexID];float a=properties.x;vec2 q=mat2(cos(a),-sin(a),sin(a),cos(a))*uv;vec3 along=right,across=up;float extent=1.;if(velocityStretch.w>0.){vec2 v=vec2(dot(velocityStretch.xyz,right),dot(velocityStretch.xyz,up));if(length(v)>.01){v=normalize(v);along=right*v.x+up*v.y;across=right*(-v.y)+up*v.x;extent+=length(velocityStretch.xyz)*velocityStretch.w;}}world=centerSize.xyz+(along*q.x*extent+across*q.y)*centerSize.w;gl_Position=vp*vec4(world,1);linearDepth=-(view*vec4(world,1)).z;tint=rgba;options=properties;}
)GLSL";
inline const char *particleFragment = R"GLSL(#version 330 core
in vec2 uv;in vec4 tint,options;in float linearDepth;in vec3 world;
uniform sampler2D depthMap;uniform sampler3D noiseVolume;uniform vec2 viewport;
uniform vec2 depthTerms;uniform vec3 eye,sun,sunColor,skyColor;uniform float sunIntensity;
layout(location=0)out vec4 color;layout(location=1)out vec4 reactive;
void main(){float radius=length(uv);if(radius>1.)discard;vec2 screen=gl_FragCoord.xy/viewport;float scene=depthTerms.y/(texture(depthMap,screen).r*2.-1.+depthTerms.x);float fade=clamp((scene-linearDepth)/max(options.y,.001),0.,1.)*tint.a;if(fade<.0014)discard;int style=int(options.w+.5);float alpha=exp(-radius*radius*3.2)*(1.-smoothstep(.72,1.,radius));if(style<2){vec3 q=vec3(uv*1.15+world.xz*.017,options.z*.19);float noise=texture(noiseVolume,q*.46).g*.6+texture(noiseVolume,q*1.4).r*.4;alpha*=smoothstep(.16,.76,noise);if(style==1)alpha*=1.4;}if(style==2)alpha=(1.-smoothstep(.05,1.,abs(uv.y)))*(1.-smoothstep(.35,1.,abs(uv.x)));if(style==3)alpha=(1.-smoothstep(.35,1.,radius));
alpha*=fade;if(alpha<.002)discard;vec3 radiance=tint.rgb;if(style<2){float phase=.12+.4*pow(max(dot(normalize(world-eye),sun),0.),8.);radiance*=skyColor*.8+sunColor*sunIntensity*phase;}color=vec4(radiance*alpha,style==2?0.:alpha);reactive=vec4(0,0,0,1);}
)GLSL";
inline const char *imageVertex = R"GLSL(#version 330 core
uniform vec4 rectangle;out vec2 uv;void main(){vec2 p[6]=vec2[](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));uv=p[gl_VertexID];vec2 position=rectangle.xy+uv*rectangle.zw;gl_Position=vec4(position.x/640.-1.,1.-position.y/360.,0,1);}
)GLSL";
inline const char *imageFragment = R"GLSL(#version 330 core
in vec2 uv;out vec4 color;uniform sampler2D source;uniform vec4 tint;void main(){color=texture(source,uv)*tint;}
)GLSL";
} // namespace rf::gfx::cinematic
