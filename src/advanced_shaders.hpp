#pragma once
namespace rf::gfx::advanced {
inline const char *sky = R"GLSL(#version 330 core
in vec2 texcoord;uniform samplerCube skyMap;uniform mat4 invProjection,invView,previousVP;uniform vec3 skyColor;uniform float farDistance;uniform int enabled;layout(location=0)out vec4 color;layout(location=1)out vec4 normal;layout(location=2)out vec4 surface;layout(location=3)out vec4 motion;
void main(){vec4 p=invProjection*vec4(texcoord*2.-1.,1,1);vec3 d=normalize(mat3(invView)*(p.xyz/p.w));vec4 previous=previousVP*vec4(d,0);vec2 uv=previous.xy/previous.w*.5+.5;color=vec4(enabled!=0?textureLod(skyMap,d,0.).rgb:skyColor*.4,1);normal=vec4(.5,1,.5,1);surface=vec4(1,0,1,0);motion=vec4(texcoord-uv,farDistance,farDistance);})GLSL";
inline const char *motionBlur = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source,motionMap,depthMap,surfaceMap;uniform vec2 size;uniform int samples;uniform float strength;
void main(){vec2 velocity=texture(motionMap,texcoord).xy*strength;float pixels=length(velocity*size);velocity*=min(1.,24./max(pixels,.001));if(pixels<1.5||texture(surfaceMap,texcoord).a>.5){color=texture(source,texcoord);return;}vec3 total=vec3(0);float weights=0.,depth=texture(depthMap,texcoord).r;for(int i=0;i<17;i++){if(i>=samples)break;float t=float(i)/float(samples-1)-.5;vec2 uv=texcoord+velocity*t;float w=exp(-abs(texture(depthMap,uv).r-depth)*2000.);total+=texture(source,uv).rgb*w;weights+=w;}color=vec4(total/max(weights,.001),1);})GLSL";
inline const char *taa = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source,history,motionMap,surfaceMap;uniform vec2 pixel;uniform float weight;uniform int valid;
void main(){vec3 current=texture(source,texcoord).rgb;vec4 motion=texture(motionMap,texcoord);vec2 oldUV=texcoord-motion.xy;vec4 old=texture(history,oldUV);vec3 minimum=current,maximum=current;for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){vec3 value=texture(source,texcoord+vec2(x,y)*pixel).rgb;minimum=min(minimum,value);maximum=max(maximum,value);}vec3 clamped=clamp(old.rgb,minimum,maximum);float difference=abs(old.a-motion.z),confidence=valid!=0&&all(greaterThanEqual(oldUV,vec2(0)))&&all(lessThanEqual(oldUV,vec2(1)))&&difference<max(.12,abs(motion.z)*.02)?1.:0.;confidence*=1.-clamp(texture(surfaceMap,texcoord).a,0.,1.);float blend=weight*confidence*exp(-length(motion.xy/pixel)*.075);color=vec4(mix(current,clamped,blend),motion.w);})GLSL";
inline const char *upscale = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source;uniform float sharpening;uniform vec2 pixel;uniform int sameSize;
vec4 cubic(float v){float v2=v*v,v3=v2*v;return vec4(-.5*v+v2-.5*v3,1.-2.5*v2+1.5*v3,.5*v+2.*v2-1.5*v3,-.5*v2+.5*v3);}
vec3 sampleCubic(vec2 uv){vec2 size=vec2(textureSize(source,0)),p=uv*size-.5,base=floor(p),f=fract(p);vec4 cx=cubic(f.x),cy=cubic(f.y);vec3 c=vec3(0);for(int y=0;y<4;y++)for(int x=0;x<4;x++)c+=texture(source,(base+vec2(x-1,y-1)+.5)/size).rgb*cx[x]*cy[y];return c;}
void main(){vec3 center=sameSize!=0?texture(source,texcoord).rgb:sampleCubic(texcoord),blur=(texture(source,texcoord+vec2(pixel.x,0)).rgb+texture(source,texcoord-vec2(pixel.x,0)).rgb+texture(source,texcoord+vec2(0,pixel.y)).rgb+texture(source,texcoord-vec2(0,pixel.y)).rgb)*.25;color=vec4(clamp(center+(center-blur)*sharpening,0.,1.),1);})GLSL";
inline const char *bloom = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source,exposureMap;uniform float threshold,exposureEV;uniform int first,autoExposure;
void main(){vec2 pixel=1./vec2(textureSize(source,0));vec3 c=vec3(0);for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++)c+=texture(source,texcoord+vec2(x,y)*pixel).rgb/9.;
 if(first!=0){float exposure=exp2(exposureEV)*(autoExposure!=0?texture(exposureMap,vec2(.5)).r:1.);
  float limit=threshold/max(exposure,.00001),knee=max(limit*.5,.00001),bright=max(c.r,max(c.g,c.b));
  float soft=clamp(bright-limit+knee,0.,2.*knee);soft=soft*soft/(4.*knee+.00001);c*=max(soft,bright-limit)/max(bright,.001);
 }color=vec4(clamp(c,vec3(0),vec3(60000)),1);})GLSL";
inline const char *blur = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source,smaller;uniform vec2 direction;uniform int combine;
void main(){vec3 c=texture(source,texcoord).rgb*.227027;c+=(texture(source,texcoord+direction*1.38461538).rgb+texture(source,texcoord-direction*1.38461538).rgb)*.3162162;c+=(texture(source,texcoord+direction*3.23076704).rgb+texture(source,texcoord-direction*3.23076704).rgb)*.07027;if(combine!=0)c+=texture(smaller,texcoord).rgb*.65;color=vec4(clamp(c,vec3(0),vec3(60000)),1);})GLSL";
inline const char *luminance = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source,depthMap;
void main(){float l=dot(max(texture(source,texcoord).rgb,vec3(0)),vec3(.2126,.7152,.0722));
 vec2 p=texcoord-.5;float weight=(.15+exp(-dot(p,p)*9.))*(texture(depthMap,texcoord).r>=.999999?.05:1.);
 // Deep black paint and tiny intense emitters should not drive the whole view.
 weight*=clamp(l/(l+.03)/(1.+l/64.),.025,1.);
 float value=log(clamp(l,.000244140625,4096.));color=vec4(value*weight,weight,value,1);})GLSL";
inline const char *exposure = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source,previous;uniform float level,dt,speed,minEV,maxEV;uniform int valid;
void main(){vec2 meter=textureLod(source,vec2(.5),level).rg;float logAverage=meter.x/max(meter.y,.00001);
 float target=clamp(log2(.18)-logAverage*1.442695,minEV,maxEV),old=log2(max(texture(previous,vec2(.5)).r,.000001));
 float rate=speed*(target>old?.45:1.),value=valid!=0?mix(old,target,1.-exp(-dt*rate)):target;color=vec4(exp2(value),0,0,1);})GLSL";
inline const char *environment = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source;uniform int face,irradiance;uniform float roughness;
float radical(uint bits){bits=(bits<<16u)|(bits>>16u);bits=((bits&0x55555555u)<<1u)|((bits&0xAAAAAAAAu)>>1u);bits=((bits&0x33333333u)<<2u)|((bits&0xCCCCCCCCu)>>2u);bits=((bits&0x0F0F0F0Fu)<<4u)|((bits&0xF0F0F0F0u)>>4u);bits=((bits&0x00FF00FFu)<<8u)|((bits&0xFF00FF00u)>>8u);return float(bits)*2.3283064365386963e-10;}
vec3 direction(vec2 uv){vec2 p=uv*2.-1.;if(face==0)return normalize(vec3(1,-p.y,-p.x));if(face==1)return normalize(vec3(-1,-p.y,p.x));if(face==2)return normalize(vec3(p.x,1,p.y));if(face==3)return normalize(vec3(p.x,-1,-p.y));if(face==4)return normalize(vec3(p.x,-p.y,1));return normalize(vec3(-p.x,-p.y,-1));}
vec3 sampleEnv(vec3 d,float lod){vec2 uv=vec2(atan(d.z,d.x)/6.283185+.5,acos(clamp(d.y,-1.,1.))/3.14159265);return textureLod(source,uv,lod).rgb;}
void main(){vec3 n=direction(texcoord),up=abs(n.y)<.99?vec3(0,1,0):vec3(1,0,0),t=normalize(cross(up,n)),b=cross(n,t);vec3 total=vec3(0);float weight=0.;for(uint i=0u;i<64u;i++){vec2 xi=vec2(float(i)/64.,radical(i));float angle=6.283185*xi.x;float cosTheta=irradiance!=0?sqrt(1.-xi.y):sqrt((1.-xi.y)/(1.+(pow(max(roughness,.001),4.)-1.)*xi.y));vec3 h=normalize(t*cos(angle)*sqrt(1.-cosTheta*cosTheta)+b*sin(angle)*sqrt(1.-cosTheta*cosTheta)+n*cosTheta);vec3 l=irradiance!=0?h:normalize(2.*dot(n,h)*h-n);float w=max(dot(n,l),0.);if(irradiance!=0)w=1.;if(w>0.){float nh=max(dot(n,h),.001),vh=nh,a2=pow(max(roughness,.001),4.),denom=nh*nh*(a2-1.)+1.,D=a2/max(3.14159265*denom*denom,.000001);float pdf=irradiance!=0?max(dot(n,l),.001)/3.14159265:D*nh/max(4.*vh,.001);float sampleSolidAngle=1./(64.*pdf+.0001),texelSolidAngle=12.56637/float(textureSize(source,0).x*textureSize(source,0).y);float lod=irradiance==0&&roughness<.001?0.:max(0.,.5*log2(sampleSolidAngle/texelSolidAngle));total+=sampleEnv(l,lod)*w;weight+=w;}}color=vec4(clamp(total/max(weight,.001),vec3(0),vec3(60000)),1);})GLSL";
inline const char *brdf = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;
float radical(uint bits){bits=(bits<<16u)|(bits>>16u);bits=((bits&0x55555555u)<<1u)|((bits&0xAAAAAAAAu)>>1u);bits=((bits&0x33333333u)<<2u)|((bits&0xCCCCCCCCu)>>2u);bits=((bits&0x0F0F0F0Fu)<<4u)|((bits&0xF0F0F0F0u)>>4u);bits=((bits&0x00FF00FFu)<<8u)|((bits&0xFF00FF00u)>>8u);return float(bits)*2.3283064365386963e-10;}
void main(){float nv=max(texcoord.x,.001),r=texcoord.y;vec3 v=vec3(sqrt(1.-nv*nv),0,nv);vec2 sum=vec2(0);for(uint i=0u;i<64u;i++){float x=float(i)/64.,y=radical(i),ct=sqrt((1.-y)/(1.+(pow(r,4.)-1.)*y));vec3 h=vec3(cos(6.283185*x)*sqrt(1.-ct*ct),sin(6.283185*x)*sqrt(1.-ct*ct),ct),l=normalize(2.*dot(v,h)*h-v);float nl=max(l.z,0.),nh=max(h.z,0.),vh=clamp(dot(v,h),0.,1.);if(nl>0.){float k=r*r*.5,G=nv/(nv*(1.-k)+k)*nl/(nl*(1.-k)+k),visibility=G*vh/max(nh*nv,.001),f=pow(1.-vh,5.);sum+=vec2((1.-f)*visibility,f*visibility);}}color=vec4(sum/64.,0,1);})GLSL";
} // namespace rf::gfx::advanced
