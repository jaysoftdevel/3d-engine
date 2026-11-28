#pragma once
#include <string>
#include "procedural_shader.hpp"
#include "lighting_shaders.hpp"
namespace rf::gfx::shader {
inline const char *meshVertex = R"GLSL(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;layout(location=2)in vec4 color;layout(location=3)in vec2 uv;
layout(location=4)in mat4 model;layout(location=8)in mat4 previousModel;layout(location=12)in vec4 instanceColor;layout(location=13)in mat3 instanceNormal;
invariant gl_Position;uniform float windStrength,windTime;uniform vec3 windVector;uniform mat4 vp,previousVP,view,previousView;out vec3 N,P,L;out vec4 tint,currentClip,previousClip;out vec2 texcoord;out vec2 linearDepths;
void main(){vec4 world=model*vec4(position,1),old=previousModel*vec4(position,1);if(windStrength!=0.){world.xz+=vec2(sin(dot(world.xz,vec2(.37,.21))+windTime*1.9),cos(dot(world.xz,vec2(.19,.28))+windTime*1.3))*windStrength*uv.y*uv.y;old.xz+=vec2(sin(dot(old.xz,vec2(.37,.21))+(windTime-.01667)*1.9),cos(dot(old.xz,vec2(.19,.28))+(windTime-.01667)*1.3))*windStrength*uv.y*uv.y;}P=world.xyz;L=position;N=normalize(instanceNormal*normal);tint=color*instanceColor;texcoord=uv;currentClip=vp*world;previousClip=previousVP*old;linearDepths=vec2(-(previousView*old).z,-(view*world).z);gl_Position=currentClip;}
)GLSL";
// Depth-only work avoids normals, inverse matrices, previous transforms and motion outputs.
inline const char *shadowVertex = R"GLSL(#version 330 core
layout(location=0)in vec3 position;layout(location=2)in vec4 color;layout(location=3)in vec2 uv;
layout(location=4)in mat4 model;layout(location=12)in vec4 instanceColor;
invariant gl_Position;uniform mat4 vp;uniform float windStrength,windTime;out vec2 texcoord;out vec4 tint;
void main(){vec4 world=model*vec4(position,1);if(windStrength!=0.)world.xz+=vec2(sin(dot(world.xz,vec2(.37,.21))+windTime*1.9),cos(dot(world.xz,vec2(.19,.28))+windTime*1.3))*windStrength*uv.y*uv.y;gl_Position=vp*world;texcoord=uv;tint=color*instanceColor;}
)GLSL";
inline const std::string minimalFragment = std::string(R"GLSL(#version 330 core
in vec3 N,P;in vec4 tint;in vec2 texcoord;
uniform vec3 eye,sun,sunColor,skyColor,groundColor,emissive;uniform float sunIntensity,ambientIntensity,viewDistance,storm,fogDensity,roughness,metallic,textureScale,alphaCutoff,normalScale;
uniform sampler2D albedoMap,emissiveMap;uniform samplerBuffer lightData;uniform usamplerBuffer lightGrid,lightIndices;
uniform int textured,hasEmissive,textureProjection,alphaMode,doubleSided,localLightCount,lightGridWidth,physicalLighting;
layout(location=0)out vec4 color;
vec3 safe(vec3 v,vec3 fallback){float d=dot(v,v);return d>1e-12?v*inversesqrt(d):fallback;}
vec2 mapUV(){vec3 n=abs(N);return textureScale*(textureProjection==0?texcoord:n.y>=n.x&&n.y>=n.z?P.xz:n.x>=n.z?P.zy:P.xy);}
vec3 shade(vec3 base,vec3 n,vec3 v,vec3 l,float pr,float pm){float nl=max(dot(n,l),0.);vec3 h=safe(v+l,n);float spec=pow(max(dot(n,h),0.),mix(8.,48.,1.-clamp(pr,0.,1.)));return (base*mix(.31831,.18,clamp(pm,0.,1.))+spec*(.025+.12*pm))*nl;}
)GLSL") + proceduralSurfaceCode + R"GLSL(
void main(){vec2 uv=mapUV();vec4 base=tint;if(textured!=0)base*=texture(albedoMap,uv);if(alphaMode==1&&base.a<alphaCutoff)discard;vec3 n=safe(N,vec3(0,1,0));if(doubleSided!=0&&!gl_FrontFacing)n=-n;float pr=roughness,pm=metallic;if(surfaceKind!=0)proceduralSurface(base.rgb,n,pr,pm);vec3 v=safe(eye-P,n);vec3 lit=base.rgb*(physicalLighting!=0?vec3(0):mix(groundColor,skyColor,n.y*.5+.5)*ambientIntensity*.5)+shade(base.rgb,n,v,safe(sun,n),pr,pm)*sunColor*sunIntensity;
if(localLightCount>0){ivec2 tile=ivec2(gl_FragCoord.xy)/16;uvec2 range=texelFetch(lightGrid,tile.y*lightGridWidth+tile.x).rg;for(uint i=0u;i<128u;i++){if(i>=range.y)break;int index=int(texelFetch(lightIndices,int(range.x+i)).r)*4;vec4 a=texelFetch(lightData,index),b=texelFetch(lightData,index+1),c=texelFetch(lightData,index+2),d=texelFetch(lightData,index+3);vec3 delta=a.xyz-P;float dd=dot(delta,delta);if(dd>=a.w*a.w||dd<.000001)continue;float dist=sqrt(dd);vec3 l=delta/dist;float q=dd/(a.w*a.w),fall=max(1.-q*q,0.);float attenuation=d.z>.5?1./max(dd,d.w*d.w):fall*fall/(dd+1.);if(d.y>.5)attenuation*=smoothstep(c.w,d.x,dot(-l,safe(c.xyz,vec3(0,-1,0))));lit+=shade(base.rgb,n,v,l,pr,pm)*b.rgb*b.w*attenuation;}}
vec3 e=emissive*surfaceEmission();if(hasEmissive!=0)e*=texture(emissiveMap,uv).rgb;lit+=e;float fog=smoothstep(viewDistance*.35,viewDistance*(1.-.55*storm),length(P-eye))*fogDensity;lit=mix(lit,mix(groundColor,skyColor,.65),clamp(fog,0.,1.));color=vec4(clamp(lit,vec3(0),vec3(60000)),alphaMode>=2?surfaceAlpha(base.a):1.);}
)GLSL";

inline const std::string sceneFragment = std::string(R"GLSL(#version 330 core
in vec3 N,P;in vec4 tint,currentClip,previousClip;in vec2 texcoord,linearDepths;
uniform vec3 eye,sun,sunColor,skyColor,groundColor,emissive,constantOrm;uniform float sunIntensity,ambientIntensity,roughness,metallic,viewDistance,storm,fogDensity,normalScale,alphaCutoff;
uniform sampler2D albedoMap,normalMap,ormMap,emissiveMap,brdfMap;uniform sampler2DShadow shadowPcf;uniform sampler3D materialNoise;uniform int textureProjection,lowestTextures;uniform float textureScale,macroVariation,clearcoat,clearcoatRoughness,environmentRotation,occlusionStrength;uniform samplerCube irradianceMap,environmentMap;
uniform samplerBuffer lightData;uniform usamplerBuffer lightGrid,lightIndices;
uniform int physicalLighting;
uniform int textured,hasNormal,hasOrm,hasEmissive,alphaMode,doubleSided,shadowQuality,receiveShadow,giMode,iblEnabled,localLightCount,lightGridWidth,reactive;
uniform mat4 lightVP,view;
layout(location=0)out vec4 color;layout(location=1)out vec4 normal;layout(location=2)out vec4 surface;layout(location=3)out vec4 motion;layout(location=4)out vec4 albedo;layout(location=5)out vec4 direct;
vec3 safeNormal(vec3 a,vec3 fallback){float len=dot(a,a);return len>1e-12?a*inversesqrt(len):fallback;}
vec3 brdf(vec3 base,vec3 n,vec3 v,vec3 l,float r,float metal){vec3 h=safeNormal(v+l,n);float nl=clamp(dot(n,l),0.,1.),nv=clamp(dot(n,v),.001,1.),nh=clamp(dot(n,h),0.,1.),vh=clamp(dot(v,h),0.,1.);float a=r*r,a2=a*a,d=nh*nh*(a2-1.)+1.,D=a2/max(3.14159265*d*d,1.e-10),k=(r+1.)*(r+1.)/8.;float G=nv/(nv*(1.-k)+k)*nl/max(nl*(1.-k)+k,.001);vec3 f0=mix(vec3(.04),base,metal),F=f0+(1.-f0)*pow(1.-vh,5.);return ((1.-F)*(1.-metal)*base/3.14159265+D*G*F/max(4.*nv*nl,.001))*nl;}
vec3 environmentDirection(vec3 d){float c=cos(-environmentRotation),s=sin(-environmentRotation);return vec3(c*d.x+s*d.z,d.y,-s*d.x+c*d.z);}
vec3 weights(vec3 n){vec3 w=pow(abs(n),vec3(5));return w/max(dot(w,vec3(1)),.001);}
vec4 mapped(sampler2D map){if(textureProjection==0)return texture(map,texcoord*textureScale);if(lowestTextures!=0){vec3 a=abs(N);vec2 uv=a.y>=a.x&&a.y>=a.z?P.xz:a.x>=a.z?P.zy:P.xy;return texture(map,uv*textureScale);}vec3 w=weights(normalize(N));return texture(map,P.zy*textureScale)*w.x+texture(map,P.xz*textureScale)*w.y+texture(map,P.xy*textureScale)*w.z;}
)GLSL") + proceduralSurfaceCode + lighting::shadows + lighting::probes + lighting::ambient + lighting::layers + lighting::direct + R"GLSL(
void main(){vec4 baseColor=tint;if(textured!=0)baseColor*=mapped(albedoMap);if(alphaMode==1&&baseColor.a<alphaCutoff)discard;vec3 base=baseColor.rgb,n=safeNormal(N,vec3(0,1,0)),v=safeNormal(eye-P,n);if(doubleSided!=0&&!gl_FrontFacing)n=-n;
if(surfaceKind==0&&macroVariation>0.){float detail=texture(materialNoise,P*.045).r*.65+texture(materialNoise,P*.14).g*.35;base*=mix(1.,.55+detail*.65,macroVariation);}
if(hasNormal!=0){if(textureProjection!=0){vec3 w=weights(n),nx=texture(normalMap,P.zy*textureScale).xyz*2.-1.,ny=texture(normalMap,P.xz*textureScale).xyz*2.-1.,nz=texture(normalMap,P.xy*textureScale).xyz*2.-1.;nx.xy*=normalScale;ny.xy*=normalScale;nz.xy*=normalScale;n=safeNormal(vec3(nx.z*sign(n.x),nx.y,nx.x)*w.x+vec3(ny.x,ny.z*sign(n.y),ny.y)*w.y+vec3(nz.x,nz.y,nz.z*sign(n.z))*w.z,n);}else{vec3 dp1=dFdx(P),dp2=dFdy(P);vec2 duv1=dFdx(texcoord),duv2=dFdy(texcoord);float determinant=duv1.x*duv2.y-duv1.y*duv2.x;if(abs(determinant)>.0000001){vec3 fallbackT=safeNormal(cross(abs(n.y)<.9?vec3(0,1,0):vec3(1,0,0),n),vec3(1,0,0));vec3 rawT=(dp1*duv2.y-dp2*duv1.y)/determinant;vec3 T=safeNormal(rawT-n*dot(n,rawT),fallbackT);vec3 rawB=(-dp1*duv2.x+dp2*duv1.x)/determinant;vec3 B=safeNormal(cross(n,T),cross(n,fallbackT))*(dot(cross(n,T),rawB)<0.?-1.:1.);vec3 mappedNormal=texture(normalMap,texcoord*textureScale).xyz*2.-1.;mappedNormal.xy*=normalScale;n=safeNormal(mat3(T,B,n)*mappedNormal,n);}}}
vec3 orm=hasOrm!=0?mapped(ormMap).rgb:lowestTextures!=0?constantOrm:vec3(1);orm.r=mix(1.,orm.r,occlusionStrength);float r=clamp(roughness*orm.g,.06,1.),metal=clamp(metallic*orm.b,0.,1.);if(surfaceKind!=0)proceduralSurface(base,n,r,metal);materialLayers(base,r,metal,n);
if(abs(anisotropy)>.001)layerFrame(n);
vec3 lit=indirectLighting(base,n,v,r,metal,P,orm.r,false);
if(sunIntensity!=0.){
 vec3 l=normalize(sun);
 if(dot(n,l)>0.){
  float visibility=dot(n,sun)>0.?shadow(n):1.;
  lit+=layeredBrdf(base,n,v,l,r,metal)*sunColor*sunIntensity*visibility;
 }
}
vec3 local=localLighting(base,n,v,r,metal);direct=vec4(clamp(local,vec3(0),vec3(60000)),1);lit+=local;
vec3 e=emissive*surfaceEmission();if(hasEmissive!=0)e*=mapped(emissiveMap).rgb;lit+=e;float fog=smoothstep(viewDistance*.35,viewDistance*(1.-.55*storm),length(P-eye))*fogDensity;lit=mix(lit,mix(groundColor,skyColor,.65),clamp(fog,0.,1.));color=vec4(clamp(lit,vec3(0),vec3(60000)),alphaMode>=2?surfaceAlpha(baseColor.a):1.);normal=vec4(n*.5+.5,1);albedo=vec4(base,clamp(clearcoat+wetness,0.,1.));surface=vec4(r,metal,orm.r,reactive!=0||alphaMode>=2?1.:0.);motion=vec4(currentClip.xy/currentClip.w*.5-previousClip.xy/previousClip.w*.5,linearDepths);}
)GLSL";
inline const char *shadowFragment = R"GLSL(#version 330 core
in vec2 texcoord;in vec4 tint;uniform sampler2D albedoMap;uniform int alphaMode,textured;uniform float alphaCutoff;
void main(){if(alphaMode==1){float a=tint.a;if(textured!=0)a*=texture(albedoMap,texcoord).a;if(a<alphaCutoff)discard;}}
)GLSL";
inline const char *fullscreen = R"GLSL(#version 330 core
out vec2 texcoord;void main(){texcoord=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(texcoord*2.-1.,0,1);})GLSL";
inline const char *aoFragment = R"GLSL(#version 330 core
in vec2 texcoord;out float value;uniform sampler2D depthMap,normalMap;uniform mat4 view;uniform vec4 projectionXY;uniform vec2 depthTerms;uniform int samples;uniform float radius;uniform vec4 sampleTable[48];
float viewZ(float d){return -depthTerms.y/(d*2.-1.+depthTerms.x);}
vec3 position(vec2 uv,float d){float z=viewZ(d);vec2 n=uv*2.-1.;return vec3(-z*(n.x+projectionXY.z)/projectionXY.x,-z*(n.y+projectionXY.w)/projectionXY.y,z);}
vec2 screenUV(vec3 q){return vec2(q.x*projectionXY.x+q.z*projectionXY.z,q.y*projectionXY.y+q.z*projectionXY.w)/-q.z*.5+.5;}
void main(){float d=texture(depthMap,texcoord).r;if(d>=.999999){value=1.;return;}vec3 p=position(texcoord,d),n=normalize(mat3(view)*(texture(normalMap,texcoord).xyz*2.-1.));vec3 basis=abs(n.y)<.9?vec3(0,1,0):vec3(1,0,0),t=normalize(cross(basis,n)),b=cross(n,t);float rotation=fract(sin(dot(gl_FragCoord.xy,vec2(12.9898,78.233)))*43758.5453)*6.28318,occ=0.;vec2 cs=vec2(cos(rotation),sin(rotation));for(int i=0;i<48;i++){if(i>=samples)break;vec4 e=sampleTable[i];vec3 dir=(t*cs.x+b*cs.y)*e.y+n*e.x;cs=vec2(cs.x*-.7373687228988248-cs.y*.6754904636562651,cs.y*-.7373687228988248+cs.x*.6754904636562651);vec3 q=p+dir*(radius*e.z);vec2 uv=screenUV(q);if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))continue;float actual=viewZ(texture(depthMap,uv).r);float range=smoothstep(0.,1.,radius/max(abs(p.z-actual),.001));occ+=(actual>=q.z+.025?1.:0.)*range;}value=clamp(1.-occ/float(samples)*1.5,0.,1.);})GLSL";
inline const char *aoBlur = R"GLSL(#version 330 core
in vec2 texcoord;out float value;uniform sampler2D source,depthMap;uniform vec2 direction;uniform vec2 depthTerms;
float linearDepth(vec2 uv){return depthTerms.y/(texture(depthMap,uv).r*2.-1.+depthTerms.x);}
void main(){float center=linearDepth(texcoord),sum=0.,weights=0.;for(int j=-3;j<=3;j++){vec2 uv=texcoord+direction*float(j);float weight=exp(-float(j*j)*.24)*exp(-abs(linearDepth(uv)-center)*8.);sum+=texture(source,uv).r*weight;weights+=weight;}value=sum/max(weights,.001);})GLSL";
inline const char *dofFragment = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 outputColor;uniform sampler2D source,depthMap,focusMap;uniform vec2 depthTerms;uniform int autoFocus;uniform float focusDistance,aperture,maxBlur,lensFocalLength,lensFNumber,lensSensorHeight,imageHeight;uniform int physicalLens;uniform int samples;uniform vec2 pixel;
float depthAt(vec2 uv){return depthTerms.y/(texture(depthMap,uv).r*2.-1.+depthTerms.x);}
void main(){float focus=autoFocus!=0?texture(focusMap,vec2(.5)).r:focusDistance;float d=depthAt(texcoord),coc=clamp(abs(d-focus)/max(d,.1)*aperture,0.,1.)*maxBlur;if(physicalLens!=0)coc=min(maxBlur,.5*lensFocalLength*lensFocalLength*abs(d-focus)/max(lensFNumber*d*(focus-lensFocalLength),.0000001)*imageHeight/lensSensorHeight);vec3 color=texture(source,texcoord).rgb;float weights=1.;for(int i=0;i<32;i++){if(i>=samples)break;float angle=float(i)*2.399963,dist=sqrt((float(i)+.5)/float(samples));vec2 uv=texcoord+vec2(cos(angle),sin(angle))*coc*dist*pixel;float other=depthAt(uv);float weight=other<d-max(.15,d*.04)?0.:1.;color+=texture(source,uv).rgb*weight;weights+=weight;}outputColor=vec4(color/weights,1);})GLSL";
inline const char *tonemapFragment = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 outputColor;uniform sampler2D source,depthMap,normalMap,bloomMap,exposureMap,luminanceMap;uniform vec2 pixel;uniform float fusion,bloomStrength,exposureEV,contrast,saturation,temperature,vignette,localExposure;uniform int toneMapper,autoExposure;
vec3 tonemap(vec3 x){if(toneMapper==1)return x/(1.+x);if(toneMapper==2){float start=.76,desaturation=.15;float mn=min(x.r,min(x.g,x.b)),offset=mn<.08?mn-6.25*mn*mn:.04;x-=offset;float peak=max(x.r,max(x.g,x.b));if(peak<start)return x;float d=1.-start,newPeak=1.-d*d/(peak+d-start);x*=newPeak/peak;float g=1.-1./(desaturation*(peak-newPeak)+1.);return mix(x,vec3(newPeak),g);}return clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.,1.);}
void main(){vec3 color=texture(source,texcoord).rgb;if(fusion>0.){float d=texture(depthMap,texcoord).r;vec3 n=texture(normalMap,texcoord).xyz*2.-1.;vec3 sum=color;float weights=1.;for(int i=0;i<4;i++){vec2 uv=texcoord+vec2(i==0?1:i==1?-1:0,i==2?1:i==3?-1:0)*pixel;float delta=abs(texture(depthMap,uv).r-d);vec3 nn=texture(normalMap,uv).xyz*2.-1.;float w=clamp(1.-dot(n,nn),0.,1.)*exp(-delta*4000.)*fusion;sum+=texture(source,uv).rgb*w;weights+=w;}color=sum/weights;}
if(bloomStrength>0.)color+=texture(bloomMap,texcoord).rgb*bloomStrength;float exposure=exp2(exposureEV)*(autoExposure!=0?texture(exposureMap,vec2(.5)).r:1.);if(localExposure>0.){
 vec2 globalMeter=textureLod(luminanceMap,vec2(.5),7.).rg;float globalLog=globalMeter.x/max(globalMeter.y,.00001),localLog=0.,weights=0.;
 float depth=texture(depthMap,texcoord).r,linearDepth=1./max(.00001,1.-depth);
 for(int y=-1;y<=1;y++)for(int x=-1;x<=1;x++){
  vec2 uv=clamp(texcoord+vec2(x,y)/32.,vec2(0),vec2(1)),meter=textureLod(luminanceMap,uv,2.).rg;
  float other=texture(depthMap,uv).r,otherDepth=1./max(.00001,1.-other);
  float w=exp(-float(x*x+y*y)*.5-abs(otherDepth-linearDepth)/max(1.,linearDepth*.08));
  localLog+=meter.x/max(meter.y,.00001)*w;weights+=w;
 }
 float correction=clamp((globalLog-localLog/max(weights,.00001))*1.442695,-2.,2.);exposure*=exp2(correction*localExposure);
}color*=exposure*vec3(1.+temperature*.12,1.,1.-temperature*.12);color=tonemap(max(color,vec3(0)));float l=dot(color,vec3(.2126,.7152,.0722));color=mix(vec3(l),color,saturation);color=(color-.18)*contrast+.18;color*=1.-vignette*smoothstep(.15,.75,length(texcoord-.5));outputColor=vec4(pow(clamp(color,0.,1.),vec3(1./2.2)),1);}
)GLSL";
inline const char *copyFragment = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source;void main(){color=texture(source,texcoord);})GLSL";
inline const char *fxaaFragment = R"GLSL(#version 330 core
in vec2 texcoord;out vec4 color;uniform sampler2D source;uniform vec2 pixel;
void main(){vec3 nw=texture(source,texcoord+vec2(-1,-1)*pixel).rgb,ne=texture(source,texcoord+vec2(1,-1)*pixel).rgb,sw=texture(source,texcoord+vec2(-1,1)*pixel).rgb,se=texture(source,texcoord+vec2(1,1)*pixel).rgb,m=texture(source,texcoord).rgb;vec3 lum=vec3(.299,.587,.114);float a=dot(nw,lum),b=dot(ne,lum),c=dot(sw,lum),d=dot(se,lum),e=dot(m,lum);float lo=min(e,min(min(a,b),min(c,d))),hi=max(e,max(max(a,b),max(c,d)));if(hi-lo<max(.0312,hi*.125)){color=vec4(m,1);return;}vec2 dir=vec2(-((a+b)-(c+d)),(a+c)-(b+d));float reduce=max((a+b+c+d)*.03125,.0078125);float reciprocal=1./(min(abs(dir.x),abs(dir.y))+reduce);dir=clamp(dir*reciprocal,vec2(-8),vec2(8))*pixel;vec3 A=.5*(texture(source,texcoord+dir*(1./3.-.5)).rgb+texture(source,texcoord+dir*(2./3.-.5)).rgb);vec3 B=A*.5+.25*(texture(source,texcoord+dir*-.5).rgb+texture(source,texcoord+dir*.5).rgb);float l=dot(B,lum);color=vec4(l<lo||l>hi?A:B,1);})GLSL";
inline const char *uiVertex = R"GLSL(#version 330 core
layout(location=0)in vec2 position;layout(location=1)in vec4 color;out vec4 tint;void main(){gl_Position=vec4(position.x/640.-1.,1.-position.y/360.,0,1);tint=color;})GLSL";
inline const char *uiFragment = R"GLSL(#version 330 core
in vec4 tint;out vec4 color;void main(){color=tint;})GLSL";
inline const char *tessVertex = R"GLSL(#version 400 core
layout(location=0)in vec3 position;void main(){gl_Position=vec4(position,1);})GLSL";
inline const char *tessControl = R"GLSL(#version 400 core
layout(vertices=4)out;uniform float tessLevel;uniform vec2 tileOrigin,cameraXZ;float level(vec2 p){return max(1.,round(tessLevel/(1.+length(p+tileOrigin-cameraXZ)/90.)));}void main(){gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;if(gl_InvocationID==0){vec2 a=gl_in[0].gl_Position.xz,b=gl_in[1].gl_Position.xz,c=gl_in[2].gl_Position.xz,d=gl_in[3].gl_Position.xz;gl_TessLevelOuter[0]=level((a+d)*.5);gl_TessLevelOuter[1]=level((a+b)*.5);gl_TessLevelOuter[2]=level((b+c)*.5);gl_TessLevelOuter[3]=level((d+c)*.5);gl_TessLevelInner[0]=max(gl_TessLevelOuter[1],gl_TessLevelOuter[3]);gl_TessLevelInner[1]=max(gl_TessLevelOuter[0],gl_TessLevelOuter[2]);}})GLSL";
inline const char *tessEvaluation = R"GLSL(#version 400 core
invariant gl_Position;layout(quads,equal_spacing,ccw)in;uniform float windStrength,windTime;uniform vec3 windVector;uniform mat4 vp,previousVP,view,previousView;uniform sampler2D heightMap;uniform vec2 tileOrigin;uniform vec4 terrainTint;out vec3 N;out vec3 P,L;out vec4 tint;out vec2 texcoord;out vec4 currentClip,previousClip;out vec2 linearDepths;
float height(vec2 local){vec2 size=vec2(textureSize(heightMap,0));vec2 uv=(clamp(local/32.,vec2(0),vec2(1))*(size-1.)+.5)/size;return textureLod(heightMap,uv,0.).r;}
void main(){vec2 a=mix(gl_in[0].gl_Position.xz,gl_in[1].gl_Position.xz,gl_TessCoord.x),b=mix(gl_in[3].gl_Position.xz,gl_in[2].gl_Position.xz,gl_TessCoord.x),local=mix(a,b,gl_TessCoord.y);float h=height(local);L=P=vec3(tileOrigin.x+local.x,h,tileOrigin.y+local.y);float dx=height(local+vec2(.25,0))-height(local-vec2(.25,0)),dz=height(local+vec2(0,.25))-height(local-vec2(0,.25));N=normalize(vec3(-dx,.5,-dz));texcoord=P.xz*.125;tint=terrainTint;currentClip=vp*vec4(P,1);previousClip=previousVP*vec4(P,1);linearDepths=vec2(-(previousView*vec4(P,1)).z,-(view*vec4(P,1)).z);gl_Position=currentClip;})GLSL";
} // namespace rf::gfx::shader
