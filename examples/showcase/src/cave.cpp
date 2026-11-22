#include "showcase.hpp"
#include <cmath>
#include <frontier/detail/light_bounds.hpp>
namespace sundog {
namespace {
constexpr float pi=rf::pi;
float smooth(float a,float b,float t){t=rf::clamp((t-a)/(b-a));return t*t*(3-2*t);}
Quat axis(V3 n,float a){float s=std::sin(a*.5f);return {n.x*s,n.y*s,n.z*s,std::cos(a*.5f)};}
Mat4 pose(V3 p,V3 s={1,1,1},float yaw=0,float pitch=0,float roll=0){return rf::compose(p)*rf::compose({},axis({0,1,0},yaw))*rf::compose({},axis({0,0,1},roll))*rf::compose({},axis({1,0,0},pitch),s);}
float noise(unsigned n){return rf::random01(rf::hash(n+90217));}
float bedrockNoise(V3 p){
 int x=int(std::floor(p.x)),y=int(std::floor(p.y)),z=int(std::floor(p.z));
 V3 f{p.x-x,p.y-y,p.z-z};f={f.x*f.x*(3-2*f.x),f.y*f.y*(3-2*f.y),f.z*f.z*(3-2*f.z)};
 float value=0;for(int k=0;k<2;k++)for(int j=0;j<2;j++)for(int i=0;i<2;i++)
  value+=noise(unsigned(x+i)*73856093u^unsigned(y+j)*19349663u^unsigned(z+k)*83492791u)*(i?f.x:1-f.x)*(j?f.y:1-f.y)*(k?f.z:1-f.z);
 return value*2-1;
}
std::vector<g::Vertex> panel(float w,float h){return {{{-w/2,h/2,0},{0,0,-1},{},0,0},{{w/2,h/2,0},{0,0,-1},{},1,0},{{w/2,-h/2,0},{0,0,-1},{},1,1},{{-w/2,h/2,0},{0,0,-1},{},0,0},{{w/2,-h/2,0},{0,0,-1},{},1,1},{{-w/2,-h/2,0},{0,0,-1},{},0,1}};}
// Excavated rock follows bedding and slow fracture changes, not repeated vertical spikes.
V3 arch(float angle,float z,bool outer=false){
 float widen=smooth(99,119,z),rx=rf::mix(8.f,26.f,widen),ry=rf::mix(13.f,23.f,widen);
 float bedding=.14f*std::sin(z*.23f+angle*5)+.08f*std::sin(z*.63f-angle*9);
 if(outer){
  // An asymmetric eroded ridge surrounds the engineered excavation.
  rx+=18+2.5f*std::sin(z*.071f+angle*3);
  ry+=18+6*std::sin(z*.037f)*std::sin(z*.037f)+2*std::sin(angle*7+z*.09f);
 }
 V3 p{128+std::cos(angle)*(rx+bedding),8+std::sin(angle)*(ry+bedding),z};
 if(outer){
  // Broad erosion and smaller fractures are evaluated in world space. The
  // entrance blends into a sloping outcrop rather than a flat cutaway face.
  float relief=2.8f*bedrockNoise(p*.12f)+.9f*bedrockNoise(p*.38f);
  p.x+=std::cos(angle)*relief;p.y+=std::sin(angle)*relief;
  p.z-=9*smooth(0,.5f,std::sin(angle))*(1-smooth(88,110,z));
 }
 return p;
}
std::vector<g::Vertex> archStrip(float z0,float z1,bool outer=false){
 std::vector<g::Vertex>out;constexpr int segments=48;
 auto vertex=[&](float a,float z){auto p=arch(a,z,outer),dA=arch(a+.001f,z,outer)-arch(a-.001f,z,outer),dZ=arch(a,z+.01f,outer)-arch(a,z-.01f,outer);
  auto n=rf::normalized(rf::cross(dA,dZ))*(outer?1.f:-1.f);
  float tint=.83f+.04f*std::sin(p.y*1.7f+z*.07f);
  return g::Vertex{p,n,{tint,tint*.97f,tint*.92f,1},a,z*.07f};};
 std::array<g::Vertex,7*49>grid;
 for(int j=0;j<=6;j++)for(int i=0;i<=segments;i++)grid[j*49+i]=vertex(i*pi/segments,rf::mix(z0,z1,j/6.f));
 out.reserve(6*segments*6);
 for(int j=0;j<6;j++)for(int i=0;i<segments;i++){
  auto A=grid[j*49+i],B=grid[j*49+i+1],C=grid[(j+1)*49+i+1],D=grid[(j+1)*49+i];
  if(outer)out.insert(out.end(),{A,B,C,A,C,D});else out.insert(out.end(),{A,C,B,A,D,C});
 }return out;
}
std::vector<g::Vertex> portalFace(){
 std::vector<g::Vertex> out;
 auto point=[](float a,float u){auto p=rf::mix(arch(a,88),arch(a,88,true),u);
  p.z-=std::sin(pi*u)*(1.5f+1.2f*bedrockNoise(p*.27f))*smooth(0,.3f,std::sin(a));return p;};
 auto vertex=[&](float a,float u){auto p=point(a,u),dA=point(a+.001f,u)-point(a-.001f,u),dU=point(a,u+.001f)-point(a,u-.001f);
  return g::Vertex{p,rf::normalized(rf::cross(dA,dU)),{.88f,.85f,.79f,1},p.x*.1f,p.y*.1f};};
 for(int j=0;j<8;j++)for(int i=0;i<48;i++){
  float a=i*pi/48,b=(i+1)*pi/48,u=j/8.f,v=(j+1)/8.f;
  auto A=vertex(a,u),B=vertex(b,u),C=vertex(b,v),D=vertex(a,v);out.insert(out.end(),{A,B,C,A,C,D});
 }return out;
}
}
bool Showcase::insideCave(V3 p){
 if(p.z<=89||p.z>=183||p.y<=8.1f)return false;
 float u=smooth(99,119,p.z),rx=rf::mix(8.f,26.f,u)-.4f,ry=rf::mix(13.f,23.f,u)-.4f;
 float x=(p.x-128)/rx,y=(p.y-8)/ry;return x*x+y*y<1;
}
bool Showcase::enclosedView(const g::Camera& camera,float aspect){
 if(!insideCave(camera.position)||camera.position.z<=115||camera.optics||aspect<=0)return false;
 auto local=rf::transformPoint(rf::lookAt(camera.position,camera.position+camera.direction),{128,14.5f,88});
 std::array<float,4> bounds;float f=1/std::tan(camera.fovDegrees*pi/360);
 return !rf::gfx::detail::lightBounds(local,11,camera.nearPlane,f/aspect,f,bounds);
}
float Showcase::caveBlend(float t){return smooth(74,80,t)*(1-smooth(140,148,t));}
g::Camera Showcase::cameraAt(float t){static const CameraTrack track({
 {0,{9,4.1f,-48},{-1,1.9f,-29},55},{8,{8,3.4f,-35},{-1,2,-19},55},
 {16,{8,3.6f,-19},{-1,1.9f,-3},54},
 {18,{4.3f,1.8f,-4.7f},{-1,1.05f,-2.7f},54},
 {22,{4.3f,1.8f,2.2f},{-1,1.15f,2.5f},54},{24,{9,4,-8},{0,2.2f,5},55},
 {32,{8,3.6f,-2},{1,2.5f,6},58},{40,{-5.6f,3.2f,5},{2.4f,2.3f,10.4f},55},
 {48,{-9.5f,6.4f,-.5f},{3.5f,5.3f,14},55},{56,{-15.2f,9.1f,-16},{.2f,4.5f,10},54},
 {60,{14,22,-3},{76,25,50},60},{64,{60,58,29},{126,23,91},64},
 {68,{102,46,62},{128,15,102},63},{71,{124,13,76},{128,14,112},60},{72,{125,12.5f,82},{128,14,119},60},
 {76,{128,12.5f,102},{127,15,136},61},{80,{128,11.5f,118},{128,11.5f,145},65},
 {84,{128,11.8f,132},{129,11.5f,149},63},{92,{128,12.2f,151},{137,12,145},58},
 {100,{131.5f,10.9f,156},{130,8.4f,165},58},{108,{126,11.8f,167},{117,10,168},57},
 {116,{124,12.1f,174},{114,11.5f,180},58},{124,{128,12.3f,174},{141,13.4f,169},61},
 {132,{134,12.8f,159},{143,14,166},61},{136,{128,12.4f,139},{127,13,111},62},{138,{128,12.2f,127},{127,13,107},62},
 {144,{128,12.5f,103},{127,12,81},61},{148,{124,15,78},{76,19,45},65},
 {152,{35,16,-31},{1,3,-26},62},{156,{9.5f,4.45f,-54.5f},{-1,1.85f,-34},55},
 {160,{9,4.1f,-48},{-1,1.9f,-29},55}});return track.sample(t);}
std::vector<VfxEmitter*> Showcase::projectEffects(){std::vector<VfxEmitter*>list{&dustLeft,&dustRight,&steam,&steam2,&sparks,&sand,&gravel};for(auto&e:caveEffects)list.push_back(&e);return list;}
void Showcase::buildCave(){
 auto rock=materials.at("rock"),steel=materials.at("steel"),dark=materials.at("dark"),paint=materials.at("paint"),concrete=materials.at("concrete"),ivory=materials.at("ivory"),white=materials.at("white"),cyan=materials.at("cyan"),amber=materials.at("amber");
 cyan.emissive={.03f,.3f,.38f,1};amber.emissive={.45f,.16f,.025f,1};
 rock.albedo={.3f,.255f,.20f,1};rock.roughness=.94f;rock.metallic=0;rock.doubleSided=false;rock.textureScale=.28f;
 steel.albedo={.55f,.64f,.69f,1};steel.metallic=.85f;steel.roughness=.48f;steel.normalScale=.35f;steel.anisotropy=.62f;steel.clearcoat=.18f;
 dark.albedo={.24f,.3f,.33f,1};dark.metallic=.7f;dark.roughness=.58f;
 paint.albedo={.78f,.52f,.22f,1};paint.roughness=.46f;
 concrete.albedo={.52f,.56f,.58f,1};concrete.roughness=.8f;concrete.metallic=0;
 ivory.albedo={.64f,.76f,.73f,1};ivory.metallic=.4f;ivory.roughness=.3f;ivory.clearcoat=.45f;ivory.dust=.08f;
 g::Material hazard;hazard.albedo={.91f,.62f,.15f,1};hazard.roughness=.65f;
 auto molten=dark;molten.emissive={0,0,0,1};molten.albedo={.12f,.14f,.13f,1};
 auto crystal=cyan;crystal.surface=g::SurfaceKind::Rock;crystal.surfaceSeed=778;crystal.albedo={.055f,.31f,.26f,1};crystal.emissive={0,0,0,1};crystal.metallic=.15f;crystal.roughness=.88f;crystal.castShadow=true;
 auto oreAmber=crystal;oreAmber.albedo={.46f,.19f,.04f,1};oreAmber.emissive={0,0,0,1};
 uint64_t next=9000000;
 auto put=[&](const std::string&name,g::MeshId shape,const g::Material&mat,V3 p,V3 s=V3{1,1,1},float yaw=0,float pitch=0,float roll=0){g::Draw d;d.mesh=shape;d.material=mat;d.modelMatrix=pose(p,s,yaw,pitch,roll);d.objectId=next++;caveScenery.push_back(d);caveNames[d.objectId]=name;};
 auto box=[&](const std::string&name,const g::Material&mat,V3 p,V3 s,float yaw=0){put(name,bevel,mat,p,s,yaw);};
 auto pipe=[&](const std::string&name,const g::Material&mat,std::vector<V3>points,float radius){put(name,mesh(g::makeTube(points,radius,12)),mat,{});};
 auto lamp=[&](const std::string&name,V3 p,Color color,float intensity,float radius){
  g::Light l;l.position=p-V3{0,.22f,0};l.color=color;l.intensity=intensity*8;l.radius=radius*1.4f;l.falloff=g::LightFalloff::InverseSquare;l.sourceRadiusMetres=.28f;
  l.type=g::LightType::Spot;l.direction={0,-1,0};l.innerCone=.65f;l.outerCone=1.12f;l.castShadow=true;l.shadowId=7000000+caveLights.size();caveLights.push_back(l);
  box(name+" housing",dark,p,{1.05f,.23f,.6f});auto emission=white;emission.emissive={color.r*7,color.g*7,color.b*7,1};box(name+" diffuser",emission,p-V3{0,.14f,0},{.8f,.07f,.38f});};
 // Mountain entrance, natural weathered shoulders and a continuous vaulted roof.
 for(unsigned i=0;i<8;i++){float side=i%2?1.f:-1.f;V3 p{128+side*(40+noise(i)*5),7,106+float(i/2)*22};put("Weathered mountain talus "+std::to_string(i+1),rocks[i%4],rock,p,{17,15+noise(i+2)*8,23},noise(i+4)*6);}
 put("Portal bedrock / continuous opening",mesh(portalFace()),rock,{});
 std::vector<g::Vertex> endcap;
 for(int i=0;i<48;i++){
  auto A=arch(i*pi/48,184,true),B=arch((i+1)*pi/48,184,true);
  for(auto q:{A,B,V3{128,8,184}})endcap.push_back({q,{0,0,1},{.83f,.8f,.74f,1},q.x*.08f,q.y*.08f});
 }
 put("Mountain rear bedrock closure",mesh(std::move(endcap)),rock,{});
 for(int i=0;i<24;i++){
  put("Excavated cavern roof / section "+std::to_string(i+1),mesh(archStrip(88+i*4,92+i*4)),rock,{});
  put("Mountain weathered outer shell / section "+std::to_string(i+1),mesh(archStrip(88+i*4,92+i*4,true)),rock,{});
 }
 // Fit the rear wall to the excavation instead of pushing a rectangular slab
 // through the mountain's curved silhouette.
 std::vector<g::Vertex> galleryEnd;
 for(int i=0;i<48;i++){
  auto A=arch(i*pi/48,183.99f),B=arch((i+1)*pi/48,183.99f);
  for(auto q:{A,V3{128,8,183.99f},B})galleryEnd.push_back({q,{0,0,-1},{.83f,.8f,.74f,1},q.x*.08f,q.y*.08f});
 }
 put("Deep gallery end wall",mesh(std::move(galleryEnd)),rock,{});
 // Closed procedural outcrops sit within the bedrock without exposed scan edges.
 for(unsigned i=0;i<24;i++){
  float side=i%2?1.f:-1.f,z=119+float(i/2)*5.4f;
  for(const auto& part:scanCliff[0].parts){auto mat=part.material;mat.roughness=.96f;mat.metallic=0;
   put("Exposed stratified bedrock "+std::to_string(i+1),part.mesh,mat,{128+side*25.4f,13,z},{8,15,8},side>0?pi:0);
  }
 }
 for(unsigned i=0;i<22;i++){float a=.25f+noise(i+300)*(pi-.5f),z=118+noise(i+301)*63;auto p=arch(a,z);put("Ceiling bedding fracture "+std::to_string(i+1),rocks[(i+2)%4],rock,p,{1.6f,1.1f,2.8f},noise(i)*6);}
 box("Tunnel approach deck",concrete,{128,7.7f,99},{17,.6f,24});box("Refinery foundation slab",concrete,{128,7.65f,148},{53,.7f,73});
 for(float x:{121.2f,134.8f})box("Portal upright / yellow steel",paint,{x,14,89},{.6f,12,.7f});box("Portal transom",paint,{128,20.1f,89},{14.2f,.7f,.9f});
 std::vector<V3>portalArch;for(int i=0;i<=32;i++){auto p=arch(i*pi/32,90);p.y-=.3f;portalArch.push_back(p);}pipe("Portal arch / structural rib",steel,portalArch,.19f);
 for(float z:{94.f,100.f,107.f}){std::vector<V3>rib;for(int i=0;i<=32;i++){auto p=arch(i*pi/32,z);p.y-=.4f;rib.push_back(p);}pipe("Access tunnel structural arch",steel,rib,.12f);for(float x:{122.f,134.f}){box("Tunnel guide light",cyan,{x,8.2f,z},{.13f,.08f,3.2f});lamp("Tunnel lamp",{x,13,z},g::lightColorTemperature(4300),8,6);}}
 auto sign=materials.at("dark");sign.surface=g::SurfaceKind::Textured;sign.albedo={1,1,1,1};sign.albedoTexture=texture("cave/refinery-sign.png");sign.emissiveTexture=sign.albedoTexture;sign.emissive={.5f,.5f,.5f,1};sign.projection=g::TextureProjection::UV;sign.roughness=.6f;sign.metallic=0;put("KESTREL DEEP / entrance sign",mesh(panel(12,2.2f)),sign,{128,22.3f,87.9f});
 // Working circulation lane, hazard markings and an uninterrupted central fly-through aisle.
 for(float x:{125.f,131.f})box("Central aisle edge marking",hazard,{x,8.025f,143},{.09f,.018f,60});
 for(int i=0;i<11;i++){float z=115+i*6;box("Floor lane center dash",ivory,{128,8.03f,z},{.13f,.02f,1.6f});}
 for(int i=0;i<22;i++)for(float x:{115.8f,122.2f})box("Conveyor safety chevron",i%2?hazard:dark,{x,8.04f,123+i*1.6f},{.45f,.025f,.62f},.5f);
 // Process-water drainage leaves shallow wet patches beside the collection sump.
 {auto wet=concrete;wet.albedo={.34f,.37f,.36f,1};wet.wetness=.85f;wet.clearcoat=.65f;wet.normalScale=.18f;
  std::vector<g::Vertex> patch;V3 center{129.5f,8.035f,162};
  for(int i=0;i<31;i++){auto point=[&](int j){float a=j*2*pi/31,r=1+.17f*(noise(2300+j%31)+noise(2300+(j+1)%31)+noise(2300+(j+30)%31))/3;return center+V3{std::cos(a)*2.5f*r,0,std::sin(a)*3.1f*r};};
   for(auto q:{center,point(i+1),point(i)})patch.push_back({q,{0,1,0},{},q.x,q.z});}
  put("Process-water wet floor / reflection",mesh(std::move(patch)),wet,{});
 }
 // Twenty-six metre belt, metal rollers, support trestles and feed hopper.
 box("Ore conveyor / return belt",dark,{119,9.45f,140},{3.7f,.34f,30});
 for(float x:{116.9f,121.1f}){box("Conveyor side beam",steel,{x,9.1f,140},{.25f,.65f,30});box("Conveyor top guard rail",hazard,{x,10.3f,140},{.11f,.12f,29});}
 for(int i=0;i<13;i++){float z=126+i*2.2f;put("Conveyor roller "+std::to_string(i+1),cylinder,steel,{119,9.65f,z},{.36f,4.1f,.36f},0,0,pi/2);for(float x:{117.1f,120.9f})box("Conveyor trestle",paint,{x,8.6f,z},{.23f,1.25f,.35f});}
 box("Crusher base",dark,{119,9.7f,123},{6,3.3f,5});box("Crusher feed hopper",paint,{119,12.5f,123},{5.9f,2.8f,5.1f});box("Hopper dark throat",dark,{119,14,123},{4.9f,.1f,4.1f});
 for(float x:{116.f,122.f})for(float z:{120.4f,125.6f})box("Crusher braced corner",steel,{x,11.3f,z},{.35f,6.3f,.35f});
 for(int i=0;i<5;i++)box("Crusher intake ribs",steel,{117.f+i,14.18f,123},{.08f,.13f,4.4f});
 // Pressure vessels and their flanges, fasteners, service valves and inspection windows.
 for(int i=0;i<3;i++){float x=i==2?143:137,z=i==0?131:i==1?145:166;
  box("Pressure vessel foundation",concrete,{x,8.5f,z},{6.4f,1,6.4f});put("Process water settling tank "+std::to_string(i+1),cylinder,ivory,{x,14.6f,z},{5.2f,11.1f,5.2f});put("Vessel dome",sphere,steel,{x,20.15f,z},{5.2f,1.5f,5.2f});
  for(float y:{9.4f,12.5f,17.5f,19.8f})put("Bolted vessel flange",torus,steel,{x,y,z},{6.65f,.8f,6.65f});
  for(int b=0;b<12;b++){float a=b*2*pi/12;put("Vessel flange stud",cylinder,dark,{x+2.63f*std::cos(a),12.57f,z+2.63f*std::sin(a)},{.13f,.22f,.13f});}
  box("Inspection port backing",dark,{x,14.2f,z-2.63f},{1.4f,3.4f,.16f});for(int w=0;w<4;w++)box("Tank level gauge",amber,{x,13.2f+w*.65f,z-2.73f},{.85f,.25f,.08f});
  pipe("High pressure transfer",steel,{{x,18,z},{x+3,18,z},{x+3,11,z+5},{x+1,11,z+5}},.22f);
  put("Valve stem",cylinder,steel,{x+3,12,z+3},{.12f,1.1f,.12f});put("Red valve wheel",torus,paint,{x+3,12.7f,z+3},{.9f,.9f,.9f});
  lamp("Pressure vessel work light",{x-3.3f,16,z-3},g::lightColorTemperature(3300),15,9);
 }
 // Furnace and slag channel, lit by actual local lights as well as emissive surfaces.
 box("Enclosed ore screen housing",dark,{118,11.6f,159},{7,7.2f,6.8f});box("Screen maintenance face",concrete,{118,11.8f,155.5f},{6.1f,6.1f,.45f});box("Screen covered feed aperture",dark,{119,10.1f,155.23f},{3.6f,1.4f,.08f});
 for(float x:{115.9f,120.1f})box("Screen access frame",steel,{x,11.4f,155.08f},{.42f,3.9f,.25f});
 box("Screen access header",steel,{118,13.35f,155.08f},{4.6f,.4f,.3f});
 for(int b=0;b<7;b++)box("Screen protective intake grille",dark,{116.4f+b*.53f,11.4f,154.9f},{.08f,2.8f,.1f});
 pipe("Screen dust extraction duct",steel,{{118,15.5f,160},{118,24,160},{121,24,160}},.55f);
 pipe("Main extraction duct / exhausted outside portal",dark,{{121,24,179},{121,24,118},{125,18,104},{125,18,86}},.6f);
 pipe("Screen extraction branch",steel,{{118,24,160},{121,24,160}},.42f);
 pipe("Crusher extraction hood branch",steel,{{119,14.8f,123},{119,24,123},{121,24,123}},.38f);
 box("Conveyor covered discharge chute",paint,{119,10.05f,155.4f},{3.6f,1.25f,2.6f});
 pipe("Sump pump discharge to process-water tanks",steel,{{112,9.5f,165},{112,10.5f,167},{137,10.5f,167},{137,10.5f,145}},.23f);
 for(float x:{112.f,119.f,137.f}){
  box("Bridge load-bearing column",steel,{x,12.05f,151},{.45f,8.1f,.45f});
  box("Bridge column footing",concrete,{x,8.15f,151},{1.4f,.3f,1.4f});
 }
 // Every vessel rests on a plinth; its motor is enclosed, with no exposed fan blades.
 for(auto p:std::array<V3,3>{{{137,21.1f,131},{137,21.1f,145},{143,21.1f,166}}}){
  put("Enclosed tank agitator gearbox",cylinder,dark,p,{.9f,.75f,.9f});
  box("Motor protective cover",paint,p+V3{0,.65f,0},{1.15f,.6f,.8f});
 }
 box("Dewatering collection sump",dark,{112,8.65f,165},{10,.9f,2.8f});box("Sump covered inspection grate",molten,{112,9.12f,165},{9.6f,.08f,2.15f});
 for(int i=0;i<6;i++)box("Dewatering collection sump retaining rib",steel,{108+i*1.6f,8.9f,165},{.18f,1.3f,3.1f});
 lamp("Screen service light",{118,14.7f,153.5f},g::lightColorTemperature(4100),18,11);lamp("Sump work light",{112,13,165},g::lightColorTemperature(4800),12,8);
 // Elevated gantry: open slats, two levels of rails, cross bracing and real stair treads.
 auto walkway=[&](V3 center,V3 size){box("Mezzanine walkway frame",steel,center,size);for(float side:{-1.f,1.f}){float x=center.x+side*(size.x/2-.08f);for(float y:{center.y+.55f,center.y+1.1f})box("Mezzanine hand rail",hazard,{x,y,center.z},{.09f,.09f,size.z});for(int k=0;k<int(size.z/3);k++)box("Mezzanine rail post",steel,{x,center.y+.55f,center.z-size.z/2+k*3},{.09f,1.1f,.09f});}};
 walkway({147,15.5f,145},{3.6f,.35f,48});
 box("Cross hall bridge beams",steel,{128,16,151},{40,.35f,3.1f});for(int i=0;i<80;i++)box("Bridge open grating slat",steel,{108+i*.5f,16.2f,151},{.14f,.1f,3});
 for(float z:{149.5f,152.5f}){box("Bridge hand rail",hazard,{128,17.3f,z},{40,.1f,.1f});for(int i=0;i<14;i++)box("Bridge guard post",steel,{109+i*2.9f,16.75f,z},{.09f,1.1f,.09f});}
 for(float z:{123.f,135.f,151.f,166.f})for(float x:{145.5f,148.5f})box("Gantry structural column",steel,{x,11.7f,z},{.38f,7.8f,.38f});
 for(int i=0;i<42;i++){float y=8.15f+i*.182f,z=180-i*.265f;box("Gallery stair tread",steel,{146,y,z},{3.1f,.12f,.3f});}
 for(float x:{144.5f,147.5f}){
  pipe("Stair load-bearing stringer",steel,{{x,8.05f,180},{x,15.5f,169}},.14f);
  pipe("Stair guard rail",hazard,{{x,9.3f,180},{x,16.9f,169}},.055f);
  for(int i=0;i<=7;i++){float u=i/7.f;box("Stair rail post",steel,{x,rf::mix(8.7f,16.3f,u),rf::mix(180.f,169.f,u)},{.08f,1.25f,.08f});}
 }
 box("Bridge connection landing",steel,{147,15.9f,151},{3.6f,.4f,3.2f});
 // Service pipe rack with elbows, valve wheels and cable trays down the hall.
 for(int p=0;p<5;p++){auto mat=p%2?paint:steel;float x=146+p*.35f;pipe("Process pipe rack "+std::to_string(p+1),mat,{{x,10,117},{x,20+p*.18f,117},{x,20+p*.18f,174},{140,20+p*.18f,174},{140,19,174}},.12f+p*.025f);}
 for(float z:{122.f,134.f,146.f,158.f,170.f}){box("Overhead cross brace",steel,{128,25,z},{34,.45f,.45f});for(float x:{111.f,145.f}){box("Roof support column",steel,{x,16.5f,z},{.4f,17,.4f});box("Roof support footing",concrete,{x,8.12f,z},{1.3f,.24f,1.3f});}for(float x:{112.f,144.f}){pipe("Suspended cable drop",dark,{{x,25,z},{x,20,z}},.045f);lamp("Pendant process light",{x,20,z},g::lightColorTemperature(4600),14,12);}}
 // Control bay with monitors, cabinets, status bars and protected windows.
 box("Control bay podium",concrete,{142,8.45f,117},{10,.9f,7});box("Control bay rear cabinet",dark,{144,11,119.7f},{7,5,1.1f});
 auto control=sign;control.albedoTexture=texture("cave/control-panel.png");control.emissiveTexture=control.albedoTexture;control.emissive={1.1f,1.1f,1.1f,1};
 for(int i=0;i<3;i++){box("Control monitor frame",steel,{139.4f+i*2.2f,11.6f,119},{2.05f,1.6f,.35f});put("Control monitor / process telemetry",mesh(panel(1.87f,1.35f)),control,{139.4f+i*2.2f,11.6f,118.8f});for(int b=0;b<5;b++)box("Console status indicator",b%2?cyan:amber,{138.6f+i*2.2f+b*.3f,10.15f,118.55f},{.16f,.1f,.07f});}
 box("Control desk",ivory,{142,9.7f,117.7f},{8,.35f,1.5f});for(float x:{138.5f,145.5f})box("Control desk pedestal",dark,{x,8.9f,117.7f},{.7f,1.5f,1.1f});lamp("Control bay downlight",{142,15,117},g::lightColorTemperature(5200),11,8);
 // Crystal-bearing extraction face and a rotating industrial drill.
 for(unsigned i=0;i<45;i++){float x=105+noise(i+500)*43,z=180+noise(i+510)*2.5f;auto mat=i%3?crystal:oreAmber;put("Exposed ore crystal "+std::to_string(i+1),rocks[i%4],mat,{x,9.5f+noise(i+520)*10,z},{.35f+noise(i+530),1.2f+noise(i+540)*2.6f,.4f+noise(i+550)},noise(i)*6,.1f,.3f);}
 box("Drill rig chassis",paint,{114,9.5f,175},{5,2.4f,5});for(float x:{111.6f,116.4f}){box("Rig track housing",dark,{x,8.85f,175},{1,1.25f,6});for(int j=0;j<9;j++)box("Track tread",steel,{x,9,172.5f+j*.62f},{1.1f,1.2f,.12f});}
 pipe("Drill hydraulic supply",dark,{{113,10.5f,173},{112,13,174},{114,14,177},{114,13,179}},.12f);
 // Reuse detailed rotor meshes; animation remains transform-only and separately editable.
 auto rotor=[&](std::string name,V3 p,float size,float speed,bool drill=false){std::vector<g::Vertex>v;auto B=g::makeBeveledBox(.05f,2),C=g::makeCylinder(24);g::appendMesh(v,C,pose({}, {size*.4f,.32f,size*.4f}));int blades=drill?12:8;for(int b=0;b<blades;b++){float a=b*2*pi/blades;g::appendMesh(v,B,pose({std::cos(a)*size*.38f,0,std::sin(a)*size*.38f},{size*.75f,.13f,size*.18f},-a,0,.15f));}Rotor r;r.position=p;r.speed=speed;r.draw.mesh=mesh(std::move(v));r.draw.material=steel;r.draw.objectId=9500000+caveRotors.size();caveNames[r.draw.objectId]=name;caveRotors.push_back(r);};
 // Cutter sits at the extraction face and turns about its horizontal spindle.
 rotor("Extraction drill cutter",{114,11.5f,182.95f},1.35f,5.2f,true);
 pipe("Drill drive spindle",steel,{{114,11.5f,175},{114,11.5f,182.95f}},.32f);
 box("Drill feed guide / supported spindle",steel,{114,10.7f,179},{1.3f,.7f,6});
 box("Crusher guarded drive enclosure",paint,{119,14.8f,123},{3.3f,1.1f,2.5f});
 pipe("Drill dust-collection branch",dark,{{115.4f,12,182},{115.4f,24,179},{121,24,179}},.27f);
 for(float x:{106.f,111.f,144.f,149.f})lamp("Extraction face wash",{x,14,177},g::lightColorTemperature(4700),13,10);
 for(int i=0;i<8;i++){float x=106+i*6;if(x>140)continue;box("Gallery utility cabinet",i%2?ivory:paint,{x,9.7f,179.5f},{2,3.3f,1.2f});box("Cabinet lit status stripe",cyan,{x,10.7f,178.83f},{1.5f,.08f,.06f});}
 for(unsigned i=0;i<30;i++){float x=104+noise(i+600)*46,z=114+noise(i+610)*65;if(x>113&&x<141)continue;put("Loose mineral ore / floor "+std::to_string(i+1),rocks[(i+3)%4],rock,{x,8.2f,z},{.2f+noise(i)*.5f,.3f,.4f},noise(i+12)*6);}
 for(int i=0;i<5;i++){float z=120+i*11;put("Wall cable reel",torus,dark,{105,10.6f,z},{1.4f,1.4f,1.4f},0,0,pi/2);box("Wall emergency light",amber,{104.8f,12,z},{.25f,.6f,.2f});}
 // Plumes are bounded, seeded emitters, not screen overlays.
 const V3 vents[]={{137,20.9f,131},{140,18.3f,136},{140,18.3f,150},{122,25.8f,160},{145,19,173},{109,13,177},{119,14.2f,123},{118,11.8f,154.9f}};
 for(unsigned i=0;i<8;i++){auto s=i==6?VfxSettings::dust():i==7?VfxSettings::dust():VfxSettings::steam();s.origin=i==7?V3{114,11.5f,182.85f}:vents[i];s.seed=5000+i*113;s.capacity=1024;s.rate=i==7?6:i==6?12:0;s.lifetime=i==7?1.1f:2.8f;s.size=i==7?.022f:.13f;s.sizeGrowth=i==7?0:.65f;s.velocity=i==7?V3{0,.9f,-.5f}:V3{0,1.3f,0};s.wind={.08f,0,.03f};s.floor=8;s.spread=i==7?.7f:.18f;if(i<6){s.start={.7f,.82f,.86f,.22f};s.end={.4f,.5f,.57f,0};s.gravity={0,.13f,0};}caveEffects.emplace_back(s);}
 // Stable source IDs for the moving ore parcels are registered before export samples.
 for(unsigned i=0;i<18;i++)caveNames[9600000+i]="Conveyor ore parcel "+std::to_string(i+1);
 // The two visible slabs are also real SI static collision surfaces.
 for(auto floor:std::array<std::pair<V3,V3>,2>{{{{128,7.7f,99},{8.5f,.3f,12}},{{128,7.65f,148},{26.5f,.35f,36.5f}}}}){BodySettings b;b.motion=MotionType::Static;b.position=floor.first;b.halfExtent=floor.second;physics.box(b);}
}
void Showcase::updateCave(float dt){for(unsigned i=0;i<caveEffects.size();i++){caveEffects[i].setEmitting(i>=6);if(dt>0)caveEffects[i].update(dt);}}
void Showcase::drawCave(bool particles){for(auto&draw:caveScenery)graphics.submit(draw);for(auto&r:caveRotors){auto d=r.draw;d.modelMatrix=pose(r.position)*rf::compose({},axis({1,0,0},pi/2))*rf::compose({},axis({0,1,0},elapsed*r.speed));graphics.submit(d);}auto ore=materials.at("rock");ore.albedo={.42f,.29f,.13f,1};ore.roughness=.72f;ore.metallic=.45f;for(unsigned i=0;i<18;i++){float u=std::fmod(i/18.f+elapsed*.023f,1.f);g::Draw d;d.mesh=rocks[(i+4)%8];d.material=ore;d.objectId=9600000+i;d.modelMatrix=pose({118.3f+(i%3)*.65f,10.1f,126+u*28},{.55f+noise(i)*.45f,.55f,.65f},i*1.7f);graphics.submit(d);}for(auto&l:caveLights)graphics.light(l);if(particles)for(auto&e:caveEffects)e.draw(graphics);}
}
