#pragma once
namespace rf::gfx::shader {
inline constexpr char skyDome[]=R"GLSL(
uniform vec3 skyColor,sun,sunColor;
uniform float turbidity,coverage,environmentRotation,skyImageWeight;
uniform sampler2D skyEquirect;
uniform int atmosphereEnabled;

vec3 atmosphericSky(vec3 d){
 float altitude=max(d.y,0.),haze=pow(1.-altitude,3.6),daylight=smoothstep(-.17,.15,sun.y);
 float overcast=smoothstep(.39,.97,coverage);
 vec3 night=vec3(.0006,.0011,.0025)+skyColor*haze*(.10+.36*overcast);
 vec3 zenith=mix(vec3(.068,.166,.320),skyColor*.74,overcast*.72);
 vec3 horizon=mix(vec3(.53,.59,.64),vec3(.56,.405,.29),exp(-pow((sun.y-.03)*5.5,2.)));
 vec3 day=mix(zenith,mix(horizon,skyColor*.92,overcast*.79),haze);
 float angle=dot(d,sun);
 float glare=pow(max(angle,0.),14.)*.13*turbidity;
 float sunDisc=smoothstep(cos(.0082),cos(.0044),angle)*6.;
 vec3 result=mix(night,day,daylight)+sunColor*(glare+sunDisc)*daylight*(1.-overcast*.92);
 return mix(result,vec3(.18,.17,.15)*daylight+night*(1.-daylight),1.-smoothstep(-.15,0.,d.y));
}
vec3 skyDomeColor(vec3 direction,vec3 flatSky){
 vec3 color=atmosphereEnabled!=0?atmosphericSky(direction):flatSky;
 if(skyImageWeight<=0.)return color;
 float c=cos(-environmentRotation),s=sin(-environmentRotation);
 vec3 d=vec3(c*direction.x+s*direction.z,direction.y,-s*direction.x+c*direction.z);
 vec2 uv=vec2(atan(d.z,d.x)/6.283185+.5,acos(clamp(d.y,-1.,1.))/3.14159265);
 return mix(color,textureLod(skyEquirect,uv,0.).rgb,skyImageWeight);
}
)GLSL";
}
