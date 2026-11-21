#include "showcase.hpp"
#include <cmath>
namespace sundog {
namespace {
constexpr float pi=rf::pi;
float smooth(float a,float b,float t){t=rf::clamp((t-a)/(b-a));return t*t*(3-2*t);}
Quat axis(V3 n,float a){float s=std::sin(a*.5f);return {n.x*s,n.y*s,n.z*s,std::cos(a*.5f)};}
Mat4 pose(V3 p,V3 s={1,1,1},float yaw=0,float pitch=0,float roll=0){return rf::compose(p)*rf::compose({},axis({0,1,0},yaw))*rf::compose({},axis({0,0,1},roll))*rf::compose({},axis({1,0,0},pitch),s);}
float noise(unsigned n){return rf::random01(rf::hash(n+90217));}
std::vector<g::Vertex> panel(float w,float h){return {{{-w/2,h/2,0},{0,0,-1},{},0,0},{{w/2,h/2,0},{0,0,-1},{},1,0},{{w/2,-h/2,0},{0,0,-1},{},1,1},{{-w/2,h/2,0},{0,0,-1},{},0,0},{{w/2,-h/2,0},{0,0,-1},{},1,1},{{-w/2,-h/2,0},{0,0,-1},{},0,1}};}
// Continuous excavated arch. The tunnel expands into a 52 m wide process hall.
V3 arch(float angle,float z){float widen=smooth(99,119,z),rx=rf::mix(8.f,26.f,widen),ry=rf::mix(13.f,23.f,widen);float n=.35f*std::sin(z*.53f+angle*11)+.24f*std::sin(z*1.43f-angle*23)+.1f*std::sin(z*3.7f+angle*41);return {128+std::cos(angle)*(rx+n),8+std::sin(angle)*(ry+n),z};}
std::vector<g::Vertex> archStrip(float z0,float z1){std::vector<g::Vertex>out;constexpr int segments=48;auto vertex=[&](float a,float z){auto p=arch(a,z),dA=arch(a+.001f,z)-arch(a-.001f,z),dZ=arch(a,z+.01f)-arch(a,z-.01f);auto n=rf::normalized(rf::cross(dA,dZ))*-1;float tint=.67f+.08f*std::sin(z*2+a*17);return g::Vertex{p,n,{tint,tint*.94f,tint*.85f,1},a,z*.07f};};for(int j=0;j<6;j++)for(int i=0;i<segments;i++){float a=i*pi/segments,b=(i+1)*pi/segments,z=rf::mix(z0,z1,j/6.f),w=rf::mix(z0,z1,(j+1)/6.f);auto A=vertex(a,z),B=vertex(b,z),C=vertex(b,w),D=vertex(a,w);out.insert(out.end(),{A,C,B,A,D,C});}return out;}
}
float Showcase::caveBlend(float t){return smooth(74,80,t)*(1-smooth(140,148,t));}
g::Camera Showcase::cameraAt(float t){static const CameraTrack track({
 {0,{9,4.1f,-48},{-1,1.9f,-29},55},{8,{8,3.4f,-35},{-1,2,-19},55},
 {16,{8,3.6f,-19},{-1,1.9f,-3},54},{24,{9,4,-8},{0,2.2f,5},55},
 {32,{8,3.6f,-2},{1,2.5f,6},58},{40,{-5.6f,3.2f,5},{2.4f,2.3f,10.4f},55},
 {48,{-9.5f,6.4f,-.5f},{3.5f,5.3f,14},55},{56,{-15.2f,9.1f,-16},{.2f,4.5f,10},54},
 {60,{14,22,-3},{76,25,50},60},{64,{60,58,29},{126,23,91},64},
 {68,{102,46,62},{128,15,102},63},{71,{124,13,76},{128,14,112},60},{72,{125,12.5f,82},{128,14,119},60},
 {76,{128,12.5f,102},{127,15,136},61},{80,{128,16,118},{128,13,145},65},
 {84,{129,18.5f,132},{124,12,149},63},{92,{143,24,151},{122,11,143},58},
 {100,{134,16.5f,160},{119,11.5f,151},58},{108,{126,12.5f,152},{117,12,160},57},
 {116,{109,12.5f,163},{117,10.7f,175},58},{124,{123,15,176},{141,13.4f,169},61},
 {132,{143,18,159},{127,13.5f,138},61},{136,{128,19,139},{127,13,111},62},{138,{128,17,127},{127,13,107},62},
 {144,{128,12.5f,103},{127,12,81},61},{148,{124,15,78},{76,19,45},65},
 {152,{35,16,-31},{1,3,-26},62},{156,{9.5f,4.45f,-54.5f},{-1,1.85f,-34},55},
 {160,{9,4.1f,-48},{-1,1.9f,-29},55}});return track.sample(t);}
std::vector<VfxEmitter*> Showcase::projectEffects(){std::vector<VfxEmitter*>list{&dustLeft,&dustRight,&steam,&steam2,&sparks,&sand,&gravel};for(auto&e:caveEffects)list.push_back(&e);return list;}
void Showcase::buildCave(){
 auto rock=materials.at("rock"),steel=materials.at("steel"),dark=materials.at("dark"),paint=materials.at("paint"),concrete=materials.at("concrete"),ivory=materials.at("ivory"),white=materials.at("white"),cyan=materials.at("cyan"),amber=materials.at("amber");
 rock.albedo={.61f,.65f,.66f,1};rock.roughness=.94f;rock.metallic=0;rock.doubleSided=true;rock.textureScale=.28f;
 steel.albedo={.55f,.64f,.69f,1};steel.metallic=.85f;steel.roughness=.34f;steel.normalScale=.48f;
 dark.albedo={.24f,.3f,.33f,1};dark.metallic=.7f;dark.roughness=.58f;
 paint.albedo={.78f,.52f,.22f,1};paint.roughness=.46f;
 concrete.albedo={.52f,.56f,.58f,1};concrete.roughness=.8f;concrete.metallic=0;
 ivory.albedo={.64f,.76f,.73f,1};ivory.metallic=.4f;ivory.roughness=.45f;
 g::Material hazard;hazard.albedo={.91f,.62f,.15f,1};hazard.roughness=.65f;
 auto molten=amber;molten.emissive={8,1.65f,.12f,1};molten.albedo={.7f,.16f,.015f,1};
 auto crystal=cyan;crystal.albedo={.055f,.31f,.26f,1};crystal.emissive={.025f,.3f,.19f,1};crystal.metallic=.48f;crystal.roughness=.22f;crystal.castShadow=true;
 auto oreAmber=crystal;oreAmber.albedo={.46f,.19f,.04f,1};oreAmber.emissive={.4f,.09f,.015f,1};
 uint64_t next=9000000;
 auto put=[&](const std::string&name,g::MeshId shape,const g::Material&mat,V3 p,V3 s=V3{1,1,1},float yaw=0,float pitch=0,float roll=0){g::Draw d;d.mesh=shape;d.material=mat;d.modelMatrix=pose(p,s,yaw,pitch,roll);d.objectId=next++;caveScenery.push_back(d);caveNames[d.objectId]=name;};
 auto box=[&](const std::string&name,const g::Material&mat,V3 p,V3 s,float yaw=0){put(name,bevel,mat,p,s,yaw);};
 auto pipe=[&](const std::string&name,const g::Material&mat,std::vector<V3>points,float radius){put(name,mesh(g::makeTube(points,radius,12)),mat,{});};
 auto lamp=[&](const std::string&name,V3 p,Color color,float intensity,float radius){g::Light l;l.position=p;l.color=color;l.intensity=intensity*15;l.radius=radius*1.5f;caveLights.push_back(l);box(name+" housing",dark,p,{1.05f,.23f,.6f});auto emission=white;emission.emissive={color.r*5,color.g*5,color.b*5,1};box(name+" diffuser",emission,p-V3{0,.14f,0},{.8f,.07f,.38f});};
 // Mountain entrance, natural weathered shoulders and a continuous vaulted roof.
 for(unsigned i=0;i<8;i++){float side=i%2?1.f:-1.f;V3 p{128+side*(43+noise(i)*7),22+noise(i+20)*6,94+float(i/2)*22};put("Mountain shoulder / eroded rock "+std::to_string(i+1),rocks[i%4],rock,p,{25+noise(i+1)*8,76+noise(i+2)*20,31+noise(i+3)*12},noise(i+4)*6);}
 for(float side:{-1.f,1.f})put("Mountain portal buttress",rocks[1],rock,{128+side*17,16,88},{17,64,18},side*.09f);
 put("Portal rock lintel",rocks[2],rock,{128,33,88},{32,23,17},.12f);
 for(int i=0;i<24;i++)put("Excavated cavern roof / section "+std::to_string(i+1),mesh(archStrip(88+i*4,92+i*4)),rock,{});
 box("Deep gallery end wall",rock,{128,19,185},{54,25,5});
 for(unsigned i=0;i<38;i++){float side=i%2?1.f:-1.f;float z=115+noise(i+40)*66,x=128+side*(24+noise(i+80)*2);put("Cavern wall outcrop "+std::to_string(i+1),rocks[i%4],rock,{x,10+noise(i+100)*10,z},{3+noise(i+3)*4,4+noise(i+4)*8,3+noise(i+5)*4},noise(i+7)*6);}
 for(unsigned i=0;i<22;i++){float x=108+noise(i+300)*40,z=118+noise(i+301)*63;put("Ceiling mineral formation "+std::to_string(i+1),rocks[(i+2)%4],rock,{x,28+noise(i+302)*2,z},{1.5f,4+noise(i+303)*3,1.7f},noise(i)*6);}
 box("Tunnel approach deck",concrete,{128,7.7f,99},{15,.6f,24});box("Refinery foundation slab",concrete,{128,7.65f,148},{51,.7f,73});
 for(float x:{121.2f,134.8f})box("Portal upright / yellow steel",paint,{x,14,89},{.6f,12,.7f});box("Portal transom",paint,{128,20.1f,89},{14.2f,.7f,.9f});
 std::vector<V3>portalArch;for(int i=0;i<=32;i++){auto p=arch(i*pi/32,90);p.y-=.3f;portalArch.push_back(p);}pipe("Portal arch / structural rib",steel,portalArch,.19f);
 for(float z:{94.f,100.f,107.f}){std::vector<V3>rib;for(int i=0;i<=32;i++){auto p=arch(i*pi/32,z);p.y-=.4f;rib.push_back(p);}pipe("Access tunnel structural arch",steel,rib,.12f);for(float x:{122.f,134.f}){box("Tunnel guide light",cyan,{x,8.2f,z},{.13f,.08f,3.2f});lamp("Tunnel lamp",{x,13,z},{.38f,.65f,.75f,1},8,6);}}
 auto sign=materials.at("dark");sign.albedo={1,1,1,1};sign.albedoTexture=texture("cave/refinery-sign.png");sign.emissiveTexture=sign.albedoTexture;sign.emissive={.5f,.5f,.5f,1};sign.projection=g::TextureProjection::UV;sign.roughness=.6f;sign.metallic=0;put("KESTREL DEEP / entrance sign",mesh(panel(12,2.2f)),sign,{128,22.3f,87.9f});
 // Working circulation lane, hazard markings and an uninterrupted central fly-through aisle.
 for(float x:{125.f,131.f})box("Central aisle edge marking",hazard,{x,8.025f,143},{.09f,.018f,60});
 for(int i=0;i<11;i++){float z=115+i*6;box("Floor lane center dash",ivory,{128,8.03f,z},{.13f,.02f,1.6f});}
 for(int i=0;i<22;i++)for(float x:{115.8f,122.2f})box("Conveyor safety chevron",i%2?hazard:dark,{x,8.04f,123+i*1.6f},{.45f,.025f,.62f},.5f);
 // Twenty-six metre belt, metal rollers, support trestles and feed hopper.
 box("Ore conveyor / return belt",dark,{119,9.45f,140},{3.7f,.34f,30});
 for(float x:{116.9f,121.1f}){box("Conveyor side beam",steel,{x,9.1f,140},{.25f,.65f,30});box("Conveyor top guard rail",hazard,{x,10.3f,140},{.11f,.12f,29});}
 for(int i=0;i<13;i++){float z=126+i*2.2f;put("Conveyor roller "+std::to_string(i+1),cylinder,steel,{119,9.65f,z},{.36f,4.1f,.36f},0,0,pi/2);for(float x:{117.1f,120.9f})box("Conveyor trestle",paint,{x,8.6f,z},{.23f,1.25f,.35f});}
 box("Crusher base",dark,{119,9.7f,123},{6,3.3f,5});box("Crusher feed hopper",paint,{119,12.5f,123},{5.9f,2.8f,5.1f});box("Hopper dark throat",dark,{119,14,123},{4.9f,.1f,4.1f});
 for(float x:{116.f,122.f})for(float z:{120.4f,125.6f})box("Crusher braced corner",steel,{x,11.3f,z},{.35f,6.3f,.35f});
 for(int i=0;i<5;i++)box("Crusher intake ribs",steel,{117.f+i,14.18f,123},{.08f,.13f,4.4f});
 // Pressure vessels and their flanges, fasteners, service valves and inspection windows.
 for(int i=0;i<3;i++){float x=i==2?143:137,z=i==0?131:i==1?145:166;
  box("Pressure vessel foundation",concrete,{x,8.5f,z},{6.4f,1,6.4f});put("Refinery pressure vessel "+std::to_string(i+1),cylinder,ivory,{x,14.6f,z},{5.2f,11.1f,5.2f});put("Vessel dome",sphere,steel,{x,20.15f,z},{5.2f,1.5f,5.2f});
  for(float y:{9.4f,12.5f,17.5f,19.8f})put("Bolted vessel flange",torus,steel,{x,y,z},{6.65f,.8f,6.65f});
  for(int b=0;b<12;b++){float a=b*2*pi/12;put("Vessel flange stud",cylinder,dark,{x+2.63f*std::cos(a),12.57f,z+2.63f*std::sin(a)},{.13f,.22f,.13f});}
  box("Inspection port backing",dark,{x,14.2f,z-2.63f},{1.4f,3.4f,.16f});for(int w=0;w<4;w++)box("Reactor amber sight glass",amber,{x,13.2f+w*.65f,z-2.73f},{.85f,.25f,.08f});
  pipe("High pressure transfer",steel,{{x,18,z},{x+3,18,z},{x+3,11,z+5},{x+1,11,z+5}},.22f);
  put("Valve stem",cylinder,steel,{x+3,12,z+3},{.12f,1.1f,.12f});put("Red valve wheel",torus,paint,{x+3,12.7f,z+3},{.9f,.9f,.9f});
  lamp("Pressure vessel work light",{x-3.3f,16,z-3},{1,.65f,.3f,1},15,9);
 }
 // Furnace and slag channel, lit by actual local lights as well as emissive surfaces.
 box("Furnace main casing",dark,{118,11.6f,159},{7,7.2f,6.8f});box("Furnace refractory face",concrete,{118,11.8f,155.5f},{6.1f,6.1f,.45f});box("Furnace hot opening",molten,{118,11.4f,155.23f},{3.6f,2.8f,.08f});
 for(float x:{115.9f,120.1f})box("Furnace door upright",steel,{x,11.4f,155.08f},{.42f,3.9f,.25f});
 box("Furnace door top rail",steel,{118,13.35f,155.08f},{4.6f,.4f,.3f});
 for(int b=0;b<7;b++)box("Furnace safety screen",dark,{116.4f+b*.53f,11.4f,154.9f},{.08f,2.8f,.1f});
 pipe("Furnace exhaust stack",steel,{{118,15.5f,160},{118,24,160},{122,24,160},{122,25.6f,160}},.7f);
 box("Slag trough",dark,{112,8.65f,165},{10,.9f,2.8f});box("Molten slag surface",molten,{112,9.12f,165},{9.6f,.08f,2.15f});
 for(int i=0;i<6;i++)box("Slag trough retaining rib",steel,{108+i*1.6f,8.9f,165},{.18f,1.3f,3.1f});
 lamp("Furnace glow",{118,11.7f,153.5f},{1,.27f,.045f,1},24,11);lamp("Slag warm pool",{112,10,165},{1,.38f,.07f,1},16,8);
 // Elevated gantry: open slats, two levels of rails, cross bracing and real stair treads.
 auto walkway=[&](V3 center,V3 size){box("Mezzanine walkway frame",steel,center,size);for(float side:{-1.f,1.f}){float x=center.x+side*(size.x/2-.08f);for(float y:{center.y+1,center.y+1.8f})box("Mezzanine hand rail",hazard,{x,y,center.z},{.09f,.09f,size.z});for(int k=0;k<int(size.z/3);k++)box("Mezzanine rail post",steel,{x,center.y+1,center.z-size.z/2+k*3},{.09f,2,.09f});}};
 walkway({147,15.5f,145},{3.6f,.35f,48});
 box("Cross hall bridge beams",steel,{128,16,151},{40,.35f,3.1f});for(int i=0;i<80;i++)box("Bridge open grating slat",steel,{108+i*.5f,16.2f,151},{.14f,.1f,3});
 for(float z:{149.5f,152.5f}){box("Bridge hand rail",hazard,{128,18,z},{40,.1f,.1f});for(int i=0;i<14;i++)box("Bridge guard post",steel,{109+i*2.9f,17.2f,z},{.09f,2,.09f});}
 for(float z:{123.f,135.f,151.f,166.f})for(float x:{145.5f,148.5f})box("Gantry structural column",steel,{x,11.7f,z},{.38f,7.8f,.38f});
 for(int i=0;i<21;i++){float y=8.25f+i*.35f,z=176-i*.4f;box("Gallery stair tread",steel,{146,y,z},{3.1f,.12f,.48f});}pipe("Stair guard rail",hazard,{{144.5f,9.6f,176},{144.5f,17,167.6f}},.065f);pipe("Stair guard rail",hazard,{{147.5f,9.6f,176},{147.5f,17,167.6f}},.065f);
 // Service pipe rack with elbows, valve wheels and cable trays down the hall.
 for(int p=0;p<5;p++){auto mat=p%2?paint:steel;float x=151+p*.5f;pipe("Process pipe rack "+std::to_string(p+1),mat,{{x,10,117},{x,22+p*.32f,117},{x,22+p*.32f,174},{140,22+p*.32f,174},{140,19,174}},.12f+p*.025f);}
 for(float z:{122.f,134.f,146.f,158.f,170.f}){box("Overhead cross brace",steel,{128,25,z},{43,.45f,.45f});for(float x:{108.f,148.f}){pipe("Suspended cable drop",dark,{{x,25,z},{x,20,z}},.045f);lamp("Pendant process light",{x,20,z},{.75f,.85f,1,1},14,12);}}
 // Control bay with monitors, cabinets, status bars and protected windows.
 box("Control bay podium",concrete,{142,8.45f,117},{10,.9f,7});box("Control bay rear cabinet",dark,{144,11,119.7f},{7,5,1.1f});
 auto control=sign;control.albedoTexture=texture("cave/control-panel.png");control.emissiveTexture=control.albedoTexture;control.emissive={1.1f,1.1f,1.1f,1};
 for(int i=0;i<3;i++){box("Control monitor frame",steel,{139.4f+i*2.2f,11.6f,119},{2.05f,1.6f,.35f});put("Control monitor / process telemetry",mesh(panel(1.87f,1.35f)),control,{139.4f+i*2.2f,11.6f,118.8f});for(int b=0;b<5;b++)box("Console status indicator",b%2?cyan:amber,{138.6f+i*2.2f+b*.3f,10.15f,118.55f},{.16f,.1f,.07f});}
 box("Control desk",ivory,{142,9.7f,117.7f},{8,.35f,1.5f});for(float x:{138.5f,145.5f})box("Control desk pedestal",dark,{x,8.9f,117.7f},{.7f,1.5f,1.1f});lamp("Control bay downlight",{142,15,117},{.32f,.72f,.9f,1},11,8);
 // Crystal-bearing extraction face and a rotating industrial drill.
 for(unsigned i=0;i<45;i++){float x=105+noise(i+500)*43,z=180+noise(i+510)*2.5f;auto mat=i%3?crystal:oreAmber;put("Exposed ore crystal "+std::to_string(i+1),rocks[i%4],mat,{x,9.5f+noise(i+520)*10,z},{.35f+noise(i+530),1.2f+noise(i+540)*2.6f,.4f+noise(i+550)},noise(i)*6,.1f,.3f);}
 box("Drill rig chassis",paint,{114,9.5f,175},{5,2.4f,5});for(float x:{111.6f,116.4f}){box("Rig track housing",dark,{x,8.85f,175},{1,1.25f,6});for(int j=0;j<9;j++)box("Track tread",steel,{x,9,172.5f+j*.62f},{1.1f,1.2f,.12f});}
 pipe("Drill hydraulic supply",dark,{{113,10.5f,173},{112,13,174},{114,14,177},{114,13,179}},.12f);
 // Reuse detailed rotor meshes; animation remains transform-only and separately editable.
 auto rotor=[&](std::string name,V3 p,float size,float speed,bool drill=false){std::vector<g::Vertex>v;auto B=g::makeBeveledBox(.05f,2),C=g::makeCylinder(24);g::appendMesh(v,C,pose({}, {size*.4f,.32f,size*.4f}));int blades=drill?12:8;for(int b=0;b<blades;b++){float a=b*2*pi/blades;g::appendMesh(v,B,pose({std::cos(a)*size*.38f,0,std::sin(a)*size*.38f},{size*.75f,.13f,size*.18f},-a,0,.15f));}Rotor r;r.position=p;r.speed=speed;r.draw.mesh=mesh(std::move(v));r.draw.material=steel;r.draw.objectId=9500000+caveRotors.size();caveNames[r.draw.objectId]=name;caveRotors.push_back(r);};
 rotor("Crusher rotor / 42 RPM",{119,14.55f,123},2.8f,4.4f);rotor("Main vessel turbine / 29 RPM",{137,21,131},3.7f,3);rotor("Separator turbine / 35 RPM",{137,21,145},3.7f,3.7f);rotor("Extraction drill rotor",{114,11.5f,178.5f},2.7f,5.2f,true);
 for(float x:{106.f,111.f,144.f,149.f})lamp("Extraction face wash",{x,14,177},{.16f,.85f,.7f,1},13,10);
 for(int i=0;i<8;i++){float x=106+i*6;box("Gallery utility cabinet",i%2?ivory:paint,{x,9.7f,179.5f},{2,3.3f,1.2f});box("Cabinet lit status stripe",cyan,{x,10.7f,178.83f},{1.5f,.08f,.06f});}
 for(unsigned i=0;i<30;i++){float x=104+noise(i+600)*46,z=114+noise(i+610)*65;if(x>113&&x<141)continue;put("Loose mineral ore / floor "+std::to_string(i+1),rocks[(i+3)%4],rock,{x,8.2f,z},{.2f+noise(i)*.5f,.3f,.4f},noise(i+12)*6);}
 for(int i=0;i<5;i++){float z=120+i*11;put("Wall cable reel",torus,dark,{105,10.6f,z},{1.4f,1.4f,1.4f},0,0,pi/2);box("Wall emergency light",amber,{104.8f,12,z},{.25f,.6f,.2f});}
 // Plumes are bounded, seeded emitters, not screen overlays.
 const V3 vents[]={{137,20.9f,131},{140,18.3f,136},{140,18.3f,150},{122,25.8f,160},{145,19,173},{109,13,177},{119,14.2f,123},{118,11.8f,154.9f}};
 for(unsigned i=0;i<8;i++){auto s=i==6?VfxSettings::dust():i==7?VfxSettings::sparks():VfxSettings::steam();s.origin=vents[i];s.seed=5000+i*113;s.capacity=1024;s.rate=i==7?15:i==6?18:24;s.lifetime=i==7?1.1f:2.8f;s.size=i==7?.022f:.13f;s.sizeGrowth=i==7?0:.65f;s.velocity=i==7?V3{0,.9f,-.5f}:V3{0,1.3f,0};s.wind={.08f,0,.03f};s.floor=8;s.spread=i==7?.7f:.18f;if(i<6){s.start={.7f,.82f,.86f,.22f};s.end={.4f,.5f,.57f,0};s.gravity={0,.13f,0};}caveEffects.emplace_back(s);}
 // Stable source IDs for the moving ore parcels are registered before export samples.
 for(unsigned i=0;i<18;i++)caveNames[9600000+i]="Conveyor ore parcel "+std::to_string(i+1);
 // The two visible slabs are also real SI static collision surfaces.
 for(auto floor:std::array<std::pair<V3,V3>,2>{{{{128,7.7f,99},{7.5f,.3f,12}},{{128,7.65f,148},{25.5f,.35f,36.5f}}}}){BodySettings b;b.motion=MotionType::Static;b.position=floor.first;b.halfExtent=floor.second;physics.box(b);}
}
void Showcase::updateCave(float dt){for(unsigned i=0;i<caveEffects.size();i++){caveEffects[i].setEmitting(i==7?std::sin(elapsed*5.7f)>.15f:i<6?std::sin(elapsed*.8f+i) > -.75f:true);if(dt>0)caveEffects[i].update(dt);}}
void Showcase::drawCave(bool particles){for(auto&draw:caveScenery)graphics.submit(draw);for(auto&r:caveRotors){auto d=r.draw;d.modelMatrix=pose(r.position,{1,1,1},elapsed*r.speed);graphics.submit(d);}auto ore=materials.at("rock");ore.albedo={.42f,.29f,.13f,1};ore.roughness=.72f;ore.metallic=.45f;for(unsigned i=0;i<18;i++){float u=std::fmod(i/18.f+elapsed*.023f,1.f);g::Draw d;d.mesh=rocks[(i+4)%8];d.material=ore;d.objectId=9600000+i;d.modelMatrix=pose({118.3f+(i%3)*.65f,10.1f,124+u*29},{.55f+noise(i)*.45f,.55f,.65f},i*1.7f);graphics.submit(d);}for(auto&l:caveLights)graphics.light(l);if(particles)for(auto&e:caveEffects)e.draw(graphics);}
}
