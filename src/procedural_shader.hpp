#pragma once
namespace rf::gfx::shader {
// Integer cell hashes cover world coordinates; no sampled noise volume or UV tile.
inline const char* proceduralSurfaceCode=R"PROC(
uniform int surfaceKind,surfaceSeed,surfaceWorld,surfaceUV;
uniform float windTime;
in vec3 L;
uint cellHash(uvec3 p){p=p*1664525u+1013904223u;p.x+=p.y*p.z;p.y+=p.z*p.x;p.z+=p.x*p.y;p^=p>>16u;p.x+=p.y*p.z;return p.x^p.y^p.z;}
float cellValue(ivec3 p){return float(cellHash(uvec3(p)^uvec3(uint(surfaceSeed)*747796405u))>>8u)*(1./16777215.);}
// Value and analytic gradient of smooth, non-periodic lattice noise.
vec4 field(vec3 p){ivec3 c=ivec3(floor(p));vec3 f=fract(p),u=f*f*f*(f*(f*6.-15.)+10.),du=30.*f*f*(f*(f-2.)+1.);
float a=cellValue(c),b=cellValue(c+ivec3(1,0,0)),d=cellValue(c+ivec3(0,1,0)),e=cellValue(c+ivec3(1,1,0));
float g=cellValue(c+ivec3(0,0,1)),h=cellValue(c+ivec3(1,0,1)),j=cellValue(c+ivec3(0,1,1)),k=cellValue(c+ivec3(1,1,1));
float x0=mix(a,b,u.x),x1=mix(d,e,u.x),x2=mix(g,h,u.x),x3=mix(j,k,u.x),z0=mix(x0,x1,u.y),z1=mix(x2,x3,u.y);
return vec4(mix(z0,z1,u.z),mix(mix(b-a,e-d,u.y),mix(h-g,k-j,u.y),u.z)*du.x,mix(x1-x0,x3-x2,u.z)*du.y,(z1-z0)*du.z);}
vec3 surfacePoint(){return (surfaceUV!=0?vec3(texcoord,0.):surfaceWorld!=0?P:L)*textureScale;}
float panelMask(vec2 point,vec2 size){
 float row=floor(point.y/size.y);float shift=cellValue(ivec3(int(row),13,7))*size.x;
 vec2 cell=vec2(floor((point.x+shift)/size.x),row),local=vec2(fract((point.x+shift)/size.x)*size.x,fract(point.y/size.y)*size.y);
 float edge=min(min(local.x,size.x-local.x),min(local.y,size.y-local.y));
 float aa=max(.0015,max(fwidth(point.x),fwidth(point.y)));
 float seam=1.-smoothstep(.004,.009+aa,edge);
 float bolt=0.;
 for(int j=0;j<2;j++)for(int i=0;i<2;i++){
  vec2 p=vec2(i==0?.043:size.x-.043,j==0?.043:size.y-.043);
  bolt=max(bolt,1.-smoothstep(.008,.015+aa,length(local-p)));
 }
 float panelTone=.91+.15*cellValue(ivec3(cell,31));
 return panelTone*(1.-seam*.40)-bolt*.22;
}
// Display content is authored as readable glyphs by the application. A display
// substrate is stable glass; it must never invent random telemetry bars.
float surfaceAlpha(float opacity){
 if(surfaceKind!=15)return opacity;
 float irradiance=clamp(sunIntensity/2.4,0.,1.)*abs(dot(normalize(N),normalize(sun)));
 return clamp(opacity*(.86+.62*irradiance),.16,.48);
}
vec3 surfaceEmission(){return surfaceKind==13?vec3(.08):vec3(1);}
float pressurePlating(vec2 p,inout vec3 base,inout float rough,inout float metal,bool door){
 vec2 size=door?vec2(.52,1.05):vec2(1.65,1.12);
 // Regular engineered joints, with unique finish per plate rather than warped tiles.
 vec2 cell=floor(p/size),local=mod(p,size),edge=min(local,size-local);
 float d=min(edge.x,edge.y),aa=max(.0007,max(fwidth(p.x),fwidth(p.y)));
 float joint=1.-smoothstep(.003,.007+aa,d);
 float lip=smoothstep(.008,.015+aa,d)*(1.-smoothstep(.019,.026+aa,d));
 float fastener=0.;
 for(int y=0;y<2;++y)for(int x=0;x<2;++x){
  vec2 c=vec2(x==0?.052:size.x-.052,y==0?.052:size.y-.052);
  float r=length(local-c);fastener=max(fastener,1.-smoothstep(.007,.012+aa,r));
 }
 float plate=.96+.065*cellValue(ivec3(cell,77));
 float wear=field(vec3(p.x*.7,p.y*41.,11.)).x;
 float brushed=smoothstep(.70,.92,wear)*.025*(1.-smoothstep(.08,.4,aa*41.));
 base=mix(base,vec3(.09,.115,.13),joint*.85);
 base=mix(base,vec3(.32,.36,.39),lip*.22+fastener*.55);
 rough=mix(clamp(rough+.045*(plate-1.)+brushed,.38,.72),.30,lip*.35+fastener);
 metal=mix(.12,.78,clamp(lip*.65+fastener,0.,1.));
 return plate*(1.-brushed);
}
void proceduralSurface(inout vec3 base,inout vec3 n,inout float r,inout float metal){
vec3 q=surfacePoint();float footprint=max(length(dFdx(q)),length(dFdy(q)));
vec4 broad=field(q*.42),grain=field(q*7.1+vec3(19.2,43.7,8.4));
float grainWeight=1.-smoothstep(.35,.95,footprint*7.1);grain=vec4(mix(.5,grain.x,grainWeight),grain.yzw*grainWeight);
float detailWeight=1.-smoothstep(.25,.9,footprint*65.);vec4 micro=vec4(.5,0,0,0);
if(detailWeight>.001&&normalScale>.001)micro=field(q*65.+vec3(71.1,6.3,39.4));
float variation=.72+.42*broad.x+.16*grain.x,bump=.018;
vec3 gradient=grain.yzw*7.1*.08+micro.yzw*65.*detailWeight*.003;
if(surfaceKind==1){variation=.58+.6*broad.x+.16*grain.x;bump=.06;r=clamp(r*(.88+.2*grain.x),.6,1.);metal=0.;}
else if(surfaceKind==2){variation=.78+.26*broad.x+.12*grain.x;bump=.035;r=clamp(.87+.12*grain.x,.8,1.);metal=0.;}
else if(surfaceKind==3){variation=.88+.15*broad.x-.12*smoothstep(.8,.98,grain.x);bump=.009;r=clamp(.72+.23*grain.x,.6,1.);metal=0.;}
else if(surfaceKind==4){variation=.95+.07*broad.x;bump=.002;r=clamp(r+.08*(grain.x-.5),.12,.9);}
else if(surfaceKind==5){variation=.9+.16*broad.x;bump=.0015;r=clamp(r+.12*(grain.x-.5),.16,.9);metal=max(metal,.72);}
else if(surfaceKind==6){variation=.88+.15*grain.x;bump=.003;r=clamp(.82+.15*grain.x,.8,1.);metal=0.;}
if(surfaceKind>=7){
 // Aperiodic domain warping produces unique mineral/dye fields without UV tiles.
 vec3 warp=vec3(field(q*.17+vec3(7,19,31)).x,field(q*.13+vec3(43,5,17)).x,field(q*.19+vec3(11,41,3)).x)-.5;
 vec4 organic=field(q*.38+warp*1.7);
 variation=.82+.24*organic.x+.07*(grain.x-.5);
 if(surfaceKind==7){
  variation=.70+.48*organic.x+.16*(grain.x-.5);r=.92+.07*grain.x;metal=0.;bump=.09;
  base*=mix(vec3(.92,.95,1.02),vec3(1.08,1.02,.91),organic.x);
 }else if(surfaceKind==8 || surfaceKind==14){
  variation=pressurePlating(q.xy,base,r,metal,surfaceKind==14);bump=.004;
 }else if(surfaceKind==9 || surfaceKind==10){
  vec2 size=surfaceKind==10?vec2(.43,.31):surfaceKind==9?vec2(.72,1.05):vec2(1.14,.81);
  variation*=panelMask(q.xy,size);
  float scratch=field(vec3(q.x*.9,q.y*57.,q.z+8.)).x;
  float scuff=smoothstep(.84,.96,grain.x)*grainWeight;
  variation*=1.-scuff*(surfaceKind==10?.34:.11);
  r=clamp(r+.11*(organic.x-.5)+.12*scuff,.17,.91);
  if(surfaceKind==10){float chip=smoothstep(.91,.99,scratch)*grainWeight;base=mix(base,vec3(.25,.28,.30),chip*.65);metal=mix(metal,.85,chip);}
  bump=surfaceKind==9?.007:.012;
 }else if(surfaceKind==11){
  variation*=panelMask(q.xy,vec2(.60,.90));
  vec2 diagonal=vec2(q.x+q.y,q.x-q.y)*34.;
  float aa=max(fwidth(diagonal.x),fwidth(diagonal.y));
  float grip=(1.-smoothstep(.12,.24+aa,abs(fract(diagonal.x)-.5)))*(1.-smoothstep(.28,.40+aa,abs(fract(diagonal.y)-.5)));
  variation*=1.-grip*.13*(1.-smoothstep(.4,1.2,aa));r=.82;bump=.018;
 }else if(surfaceKind==12){
  float weaveFoot=footprint*190.;vec2 weave=sin(q.xy*1194.);
  variation*=1.+weave.x*weave.y*.10*(1.-smoothstep(.25,.95,weaveFoot));r=.91;metal=0.;bump=.035;
 }else if(surfaceKind==16){
  vec2 size=vec2(.172,.135),local=mod(q.xy,size),cell=floor(q.xy/size);
  float aa=max(.0005,max(fwidth(q.x),fwidth(q.y)));
  float edge=min(min(local.x,size.x-local.x),min(local.y,size.y-local.y));
  float gap=1.-smoothstep(.001,.003+aa,edge);
  float bus=min(abs(local.x-size.x*.33),abs(local.x-size.x*.67));
  float conductor=(1.-smoothstep(.0005,.0010+aa,bus))*.50;
  base=mix(base,vec3(.38,.43,.48),gap*.7+conductor*.35);
  variation=.94+.05*cellValue(ivec3(cell,97));r=.24;metal=.42;bump=.001;
 }else if(surfaceKind==15){
  // Neutral-density solar film, a blue UV-cut coating and fine ceramic frit.
  // Screen derivatives fade the frit before it can shimmer in the distance.
  vec2 dotCell=fract(q.xy*42.)-.5;
  float aa=max(fwidth(q.x),fwidth(q.y))*42.;
  float frit=(1.-smoothstep(.09,.15+aa,length(dotCell)))*(1.-smoothstep(.18,.65,aa));
  float sunFacing=abs(dot(n,normalize(sun)));
  base=mix(base,vec3(.31,.39,.34),.14*sunFacing);
  base=mix(base,vec3(.45,.50,.51),frit*.24);variation=1.;r=.13+.23*frit;metal=.025;bump=0.;
 }else if(surfaceKind==13){
  base=vec3(.008,.014,.019);variation=1.;r=.32;metal=.08;bump=0.;
 }
}
base*=variation;
// Gradient transfer through screen derivatives keeps object-local detail attached
// to moving/rotating objects without needing extra instance attributes.
vec3 dx=dFdx(P),dy=dFdy(P),a=cross(dy,n),b=cross(n,dx);float determinant=dot(dx,a);
if(abs(determinant)>1e-10){float hx=dot(gradient,dFdx(q)),hy=dot(gradient,dFdy(q));
vec3 perturb=(a*hx+b*hy)/determinant;float len=length(perturb);if(len>1.)perturb/=len;
n=normalize(n-perturb*bump*normalScale);}
}
)PROC";
}
