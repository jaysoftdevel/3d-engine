#pragma once
#include <frontier/renderer.hpp>
#include <jsd/assets.hpp>
#include <jsd/strands.hpp>
#include "process_id.hpp"
#include <iostream>
template<class Check>void lightingGraphicsTests(rf::gfx::Renderer&r,Check check){
 using namespace rf;using namespace rf::gfx;
 auto initial=r.settings().requested;auto path=std::filesystem::temp_directory_path()/("jsd-lighting-"+std::to_string(jsdTestProcessId())+".ppm");
 auto image=[&]{r.check();r.screenshot(path);return jsd::loadImage(path);};
 auto center=[](const jsd::Image&i,int channel){double sum=0;int n=0;for(int y=i.height/2-8;y<i.height/2+8;y++)for(int x=i.width/2-8;x<i.width/2+8;x++){sum+=i.rgba[(size_t(y)*i.width+x)*4+channel];n++;}return sum/n;};
 auto settings=GraphicsSettings::legacy();settings.textureQuality=TextureQuality::High;settings.globalIllumination=GlobalIllumination::Hybrid;settings.reflections=Quality::Medium;settings.viewDistance=80;r.setSettings(settings);
 Camera camera{{0,1,-4},{0,0,1},60,.08f};Environment env;env.sunIntensity=0;env.ambientIntensity=0;env.skyColor={0,0,0,1};env.groundColor={0,0,0,1};env.fogDensity=0;
 ReflectionProbe probe;probe.id=340;probe.position={0,1,0};probe.halfExtent={10,7,10};probe.refreshFrames=0;probe.blendDistance=.5f;
 auto nearby=probe;
 nearby.id=341;nearby.position.x=3.f;
 auto reflected=[&](bool capture,bool overlap=false){
  r.begin(camera,env);if(capture)r.reflectionProbe(probe);
  if(overlap)r.reflectionProbe(nearby);
  Material mirror;mirror.albedo={.8f,.8f,.8f,1};mirror.metallic=1;mirror.roughness=.12f;
  r.primitive(Primitive::Box,{{0,1,3},{2,2,1},0},mirror);
  Material wall;wall.emissive={3,.01f,.005f,1};r.primitive(Primitive::Box,{{0,1,-6},{16,8,.1f},0},wall);
  r.end();return image();
 };
 auto noProbe=reflected(false);auto withProbe=noProbe;unsigned faces=0;bool bounded=true,delayed=false;
 for(int frame=0;frame<15;frame++){withProbe=reflected(true);auto stats=r.statistics();faces+=stats.reflectionProbeFaces;bounded&=stats.reflectionProbeFaces<=1&&stats.reflectionProbeFilters<=6;if(frame==0)delayed=stats.activeReflectionProbes==0;}
 check(faces==6&&bounded&&delayed&&r.statistics().activeReflectionProbes==1,"probe publication waits for a complete capture with bounded per-frame capture/filter work");
 for(int frame=0;frame<14;frame++)withProbe=reflected(true,true);
 check(center(withProbe,0)>center(noProbe,0)+35&&center(withProbe,0)>center(withProbe,2)+35,"offscreen red scenery appears in a box-projected reflection");
 reflected(true,true);
 check(r.statistics().activeReflectionProbes==2,"overlapping probes become resident together within the shared capture budget");
 reflected(true);check(r.statistics().reflectionProbeFaces==0,"static scene probes reuse cached captures");
 probe.revision++;reflected(true);check(r.statistics().reflectionProbeFaces==1&&r.statistics().activeReflectionProbes==0,"probe edits start an incremental refresh without a six-face stall");
 for(int frame=0;frame<15;frame++)reflected(true);
 probe.refreshFrames=1;reflected(true);check(r.statistics().reflectionProbeFaces==1,"dynamic probe refresh amortizes one face per frame");
 settings.reflections=Quality::Off;settings.globalIllumination=GlobalIllumination::Off;r.setSettings(settings);
 auto lights=[&](bool sampled){settings.stochasticLighting=sampled;settings.directLightSamples=4;r.setSettings(settings);r.resetHistory();r.begin(camera,env);
  Material matte;matte.albedo={.5f,.4f,.3f,1};r.primitive(Primitive::Box,{{0,1,3},{4,4,1},0},matte);
  for(int j=0;j<40;j++){Light l;l.position={0,2,-1};l.radius=15;l.intensity=.22f;r.light(l);}r.end();return image();};
 auto exact=lights(false),sampled=lights(true);
 std::cout<<"Equal-light energy reference="<<center(exact,0)<<" sampled="<<center(sampled,0)<<" budget="<<r.statistics().stochasticLightSamples<<"\n";
 check(std::abs(center(exact,0)-center(sampled,0))<2&&r.statistics().stochasticLightSamples==4,"importance sampling preserves energy for equal overlapping lights");
 // The blocker is outside this narrow camera view, but intersects the light's
 // path to the visible wall. A screen-space ray cannot supply this shadow.
 settings.stochasticLighting=false;settings.shadows=Quality::High;r.setSettings(settings);
 Camera shadowCamera{{0,1,-4},{0,0,1},30,.08f};
 auto spot=[&](bool shadowed,bool blocked,unsigned revision=0){
  r.begin(shadowCamera,env);Material wall;wall.albedo={.45f,.45f,.45f,1};wall.roughness=.85f;
  r.primitive(Primitive::Box,{{0,1,3},{6,6,1},0},wall);
  r.primitive(Primitive::Box,{{blocked?3.f:5.f,3,.25f},{1.2f,1.2f,.8f},0},wall);
  Light l;l.type=LightType::Spot;l.position={6,5,-2};l.direction=normalized(V3{0,1,2.5f}-l.position);l.radius=18;l.intensity=1200;l.falloff=LightFalloff::InverseSquare;l.innerCone=.35f;l.outerCone=.65f;l.sourceRadiusMetres=.18f;l.castShadow=shadowed;l.shadowId=900;l.shadowRevision=revision;r.light(l);r.end();return image();
 };
 auto contactOnly=spot(false,true),mapped=spot(true,true);
 std::cout<<"Offscreen blocker contact="<<center(contactOnly,0)<<" shadow="<<center(mapped,0)<<'\n';
 check(center(contactOnly,0)>80&&center(mapped,0)<center(contactOnly,0)*.4,"spotlight shadow maps block light from an offscreen occluder");
 spot(true,true);check(r.statistics().localShadowMaps==1&&r.statistics().localShadowUpdates==0,"unchanged spotlight and caster geometry reuse the cached shadow map");
 auto opened=spot(true,false);check(center(opened,0)>center(mapped,0)+50&&r.statistics().localShadowUpdates==1,"moving a caster refreshes the shadow and opens the previously occluded light path");
 spot(true,false,1);check(r.statistics().localShadowUpdates==1,"explicit light revision invalidates its cached shadow map");
 r.setSettings(settings);bool budget=true;
 for(int frame=0;frame<3;frame++){
  r.begin(shadowCamera,env);r.primitive(Primitive::Box,{{0,1,3},{6,6,1},0});
  for(int j=0;j<8;j++){Light l;l.type=LightType::Spot;l.position={0,4,-1};l.direction={0,-.4f,1};l.radius=20;l.intensity=2;l.innerCone=.3f;l.outerCone=.7f;l.castShadow=true;l.shadowId=1000+j;r.light(l);}r.end();r.check();
  budget&=r.statistics().localShadowUpdates<=2&&r.statistics().localShadowMaps<=4;
 }
 check(budget&&r.statistics().localShadowMaps==4&&r.statistics().localShadowUpdates==0,"many shadow requests retain four maps with at most two updates per frame and settle into cache reuse");
 settings.shadows=Quality::Off;r.setSettings(settings);
 auto sphereEmitter=[&](float radius){r.begin(camera,env);Material m;m.albedo={.7f,.7f,.7f,1};m.metallic=1;m.roughness=.1f;
  r.primitive(Primitive::Box,{{0,1,3},{4,4,1},0},m);Light l;l.position={0,1,-2};l.intensity=.15f;l.radius=15;l.sourceRadiusMetres=radius;l.falloff=LightFalloff::InverseSquare;r.light(l);r.end();r.validateFrame();return image();};
 auto pointEmitter=sphereEmitter(.001f),wideEmitter=sphereEmitter(.5f);
 auto highlightPixels=[](auto &i){unsigned count=0;for(size_t p=0;p<i.rgba.size();p+=4)if(i.rgba[p]>20)count++;return count;};
 std::cout<<"Emitter highlight footprint="<<highlightPixels(pointEmitter)<<'/'<<highlightPixels(wideEmitter)<<'\n';
 check(highlightPixels(wideEmitter)>highlightPixels(pointEmitter)*1.2,"finite emitter radius broadens glossy highlights instead of behaving like a point source");
 r.begin(camera,env);Material polished;polished.albedo={.95f,.95f,.95f,1};polished.metallic=1;polished.roughness=.06f;
 r.primitive(Primitive::Box,{{0,1,3},{4,4,1},0},polished);Light intense;intense.position={0,1,-1};intense.intensity=1000000;intense.radius=15;intense.falloff=LightFalloff::InverseSquare;r.light(intense);r.end();r.validateFrame();
 check(true,"strong emitter highlights remain finite in half-float scene and lighting buffers");
 settings.shadows=Quality::Off;settings.autoExposure=true;settings.minExposureEV=settings.maxExposureEV=1;r.setSettings(settings);
 auto exposureWall=[&](float emission){r.begin(camera,env);Material m;m.albedo={0,0,0,1};m.emissive={emission,emission,emission,1};r.primitive(Primitive::Box,{{0,1,3},{20,20,1},0},m);r.end();return image();};
 auto boundedExposure=exposureWall(.2f);settings.autoExposure=false;settings.exposureCompensation=1;r.setSettings(settings);auto fixedExposure=exposureWall(.2f);
 check(std::abs(center(boundedExposure,0)-center(fixedExposure,0))<1,"bounded logarithmic auto exposure matches an equivalent manual exposure");
 settings.autoExposure=true;settings.exposureCompensation=0;settings.minExposureEV=-4;settings.maxExposureEV=4;r.setSettings(settings);exposureWall(.04f);auto flash=exposureWall(4);
 jsd::Image adapted;for(int frame=0;frame<45;frame++)adapted=exposureWall(4);
 check(center(flash,0)>center(adapted,0)+20&&center(adapted,0)>60,"eye adaptation converges smoothly after a dark-to-bright lighting change");
 settings.autoExposure=false;settings.localExposure=0;settings.globalIllumination=GlobalIllumination::AmbientProbe;env.ambientIntensity=1;env.skyColor={.7f,.7f,.7f,1};env.groundColor={.7f,.7f,.7f,1};
 auto occlusion=[&](bool enabled,bool emissive){settings.ambientOcclusion=enabled?Quality::High:Quality::Off;r.setSettings(settings);r.begin(camera,env);
  Material m;m.albedo=emissive?Color{0,0,0,1}:Color{.5f,.5f,.5f,1};m.emissive=emissive?Color{.25f,.25f,.25f,1}:Color{0,0,0,1};
  r.primitive(Primitive::Box,{{0,1,3},{4,4,1},0},m);Material dark;dark.albedo={.05f,.05f,.05f,1};
  for(float side:{-1.f,1.f})r.primitive(Primitive::Box,{{side*.7f,1,2.1f},{.3f,4,1.2f},0},dark);
  r.end();return image();};
 auto emitClear=occlusion(false,true),emitAO=occlusion(true,true),diffuseClear=occlusion(false,false),diffuseAO=occlusion(true,false);
 std::cout<<"AO emission="<<center(emitClear,0)<<'/'<<center(emitAO,0)<<" diffuse="<<center(diffuseClear,0)<<'/'<<center(diffuseAO,0)<<'\n';
 check(std::abs(center(emitClear,0)-center(emitAO,0))<1&&center(diffuseClear,0)>center(diffuseAO,0)+2,"ambient occlusion darkens indirect illumination while preserving emissive energy");
 env.ambientIntensity=0;env.skyColor={0,0,0,1};env.groundColor={0,0,0,1};settings.ambientOcclusion=Quality::Off;settings.globalIllumination=GlobalIllumination::Off;
 settings.stochasticLighting=false;settings.volumetricFog=Quality::Low;settings.fogDensity=0;r.setSettings(settings);
 auto volume=[&](int mode){r.begin(camera,env);Material m;m.emissive={.05f,.05f,.05f,1};r.primitive(Primitive::Box,{{0,1,3},{4,4,1},0},m);
  if(mode){FogVolume f;f.position={0,1,mode==1?6.f:0.f};f.halfExtent={2,2,1};f.density=.7f;f.emission={1,.02f,.005f,1};r.fogVolume(f);}r.end();return image();};
 auto clear=volume(0),behind=volume(1),front=volume(2);
 check(std::abs(center(clear,0)-center(behind,0))<1,"opaque depth blocks volumetric emission behind a surface");
 check(center(front,0)>center(clear,0)+20&&r.statistics().fogVolumes==1,"bounded emissive volumetrics scatter in front of scene geometry");
 r.begin(camera,env);
 Material back;back.emissive={.05f,.05f,.05f,1};
 r.primitive(Primitive::Box,{{0,1,38},{18,18,1},0},back);
 FogVolume small;small.position={0,1,6.2f};small.halfExtent={1.f,1.f,.14f};
 small.density=2.f;small.emission={1,.02f,.005f,1};r.fogVolume(small);
 r.end();
 check(center(image(),0)>center(clear,0)+25,"volumetric integration retains shallow volumes between coarse step centers");
 settings.volumetricFog=Quality::Off;settings.shadows=Quality::Medium;r.setSettings(settings);r.begin(camera,env);r.primitive(Primitive::Box,{{0,1,3},{2,2,1},0});r.end();r.check();
 check(r.statistics().shadowCascades==4,"sun shadows use four stabilized atlas cascades");
 settings.shadows=Quality::Off;r.setSettings(settings);
 {
  const auto restore=settings;
  settings.globalIllumination=GlobalIllumination::AmbientProbe;
  settings.exposureCompensation=0;
  r.setSettings(settings);
  Environment authored=env;
  authored.skyColor={.055f,.10f,.39f,1};
  authored.groundColor=authored.skyColor;
  authored.ambientIntensity=1;
  std::array<float,16*8*3> red{};
  for (std::size_t i=0;i<red.size();i+=3) {
   red[i]=1.3f;red[i+1]=.06f;red[i+2]=.025f;
  }
  authored.image=r.createEnvironment(16,8,red);
  const auto skyFrame=[&](float light,float backdrop,bool geometry) {
   authored.imageLightingWeight=light;
   authored.imageBackgroundWeight=backdrop;
   r.begin(camera,authored);
   if (geometry) {
    Material diffuse;diffuse.albedo={.55f,.55f,.55f,1};diffuse.roughness=.95f;
    r.primitive(Primitive::Box,{{0,1,3},{4,4,1},0},diffuse);
   }
   r.end();return image();
  };
  const auto procedural=skyFrame(1,0,false),photo=skyFrame(0,1,false);
  const auto ambient=skyFrame(0,1,true),imageLit=skyFrame(1,0,true);
  check(center(photo,0)>center(procedural,0)+40&&center(imageLit,0)>center(ambient,0)+25,
      "photographic backdrop and image-based ambient lighting can be weighted independently");
  r.destroyEnvironment(authored.image);
  authored.image={};
  authored.aerial={.04f,1.f,9.f,{.88f,.91f,1,1}};
  const auto hazeFrame=[&](float height,float distance,float extinction) {
   authored.aerial.extinctionPerMetre=extinction;
   auto elevated=camera;elevated.position.y=height;
   r.begin(elevated,authored);
   Material marker;marker.albedo={0,0,0,1};marker.emissive={.65f,.65f,.65f,1};
   r.primitive(Primitive::Box,{{0,height,distance},{15,15,.3f},0},marker);
   r.end();return image();
  };
  const auto distantClear=hazeFrame(1,46,0),distantHaze=hazeFrame(1,46,.04f);
  const auto nearbyHaze=hazeFrame(1,3,.04f),highHaze=hazeFrame(30,46,.04f);
  check(center(distantClear,0)>center(distantHaze,0)+40&&
        center(nearbyHaze,0)>center(distantHaze,0)+25&&center(highHaze,0)>center(distantHaze,0)+25,
      "analytic aerial perspective attenuates distant geometry and falls off with height");
  settings=restore;
  r.setSettings(settings);
 }
 auto dense=r.createLodMesh(makeSphere(96,48),1.5f);
 auto detail=[&](float z){r.begin({{0,0,z},{0,0,1},60,.08f},env);r.submit({dense,{},Material{},0,{}});r.end();r.check();return r.statistics();};
 auto near=detail(-1.2f),far=detail(-60);
 check(far.lodInstances>0&&far.lodTrianglesSaved>0&&far.triangles<near.triangles,"screen-space mesh error selects less geometry for distant scenery");r.destroyMesh(dense);
 auto pebble=r.createMesh(makeBox());ProceduralScatter scatter;scatter.id=341;scatter.mesh=pebble;scatter.center={0,0,2};scatter.halfExtent={3,0,3};scatter.count=400;scatter.seed=17;scatter.minScale=.1f;scatter.maxScale=.3f;scatter.heightOffset=.5f;scatter.height=[](float,float){return 0.f;};scatter.material.emissive={1,.4f,.02f,1};
 auto scatterFrame=[&](bool excluded){r.begin({{0,5,-4},normalized(V3{0,-4,6}),60,.08f},env);
  if(excluded)scatter.exclusions[0]={{-10,-10,-10},{10,10,10}};else scatter.exclusions={};r.scatter(scatter);r.end();return image();};
 auto first=scatterFrame(false),again=scatterFrame(false),excluded=scatterFrame(true);
 check(first.rgba==again.rgba&&first.rgba!=excluded.rgba&&r.statistics().proceduralInstances==400,"GPU scatter is deterministic and respects authored exclusion regions");r.destroyMesh(pebble);
 auto streamed=r.stageMesh(makeBox());
 const std::vector<unsigned char> texels(128*128*4,255);
 auto mip=r.stageTexture(128,128,texels);
 check(r.textureDimensions(mip)==std::pair{32,32},"streamed textures initially allocate a complete coarse mip tail");
 r.begin(camera,env);
 check(!r.requestMesh(streamed,4.f),"staged meshes become resident during the upload phase");
 r.requestTexture(mip,128);
 r.end();r.check();
 check(r.textureDimensions(mip)==std::pair{128,128} && r.statistics().mipUploadBytes>0 &&
       r.statistics().streamedMeshes==1,"mesh and mip requests upload against their frame budgets");
 r.begin(camera,env);
 check(r.requestMesh(streamed,4.f),"resident mesh keeps its original handle");
 Material finish;finish.albedoTexture=mip;
 r.submit({streamed,{{0,1,2},{2,2,2},0},finish,0,{}});
 r.end();r.validateFrame();
 auto channels=texels;
 for (std::size_t i=3;i<channels.size();i+=4) channels[i]=0;
 auto retention=r.stageTexture(128,128,channels,false,false);
 auto wet=finish;
 wet.albedoTexture={};
 wet.ormTexture=retention;
 wet.wetness=1.f;
 const auto maps=std::filesystem::temp_directory_path()/("jsd-wetness-"+std::to_string(jsdTestProcessId()));
 const auto coated=[&](float variation,int edge,float detailScale) {
  r.begin(camera,env);
  r.requestTexture(retention,edge,3.f);
  wet.wetnessVariation=variation;wet.detail={detailScale,.32f,.48f,.32f};
  r.primitive(Primitive::Box,{{0,1,3},{4,4,1},0},wet);
  r.end();r.check();r.validateFrame();r.captureBuffers(maps);
  return jsd::loadImage(maps/"material.ppm");
 };
 auto coarseDry=coated(1.f,32,0.f),finestDry=coated(1.f,128,0.f);
 auto unmasked=coated(0.f,128,0.f),detailed=coated(1.f,128,3.f);
 check(r.textureDimensions(retention)==std::pair{128,128} && finestDry.rgba==coarseDry.rgba &&
       center(finestDry,0)>center(unmasked,0)+32,"packed mip streaming preserves linear water retention through all mip levels");
 check(detailed.rgba!=finestDry.rgba,"metre-scale detail affects material roughness over the authored map");
 std::filesystem::remove_all(maps);
 r.destroyTexture(retention);
 r.destroyMesh(streamed);
 r.destroyTexture(mip);
 r.setSettings(initial);r.resetHistory();std::filesystem::remove(path);
}

template<class Check>void lightingUnitTests(Check check){
 using namespace rf::gfx;auto warm=lightColorTemperature(3000),cool=lightColorTemperature(9000);
 auto luma=[](auto c){return .2126f*c.r+.7152f*c.g+.0722f*c.b;};
 check(warm.r>warm.b&&cool.b>cool.r&&std::abs(luma(warm)-1)<.00001f&&std::abs(luma(cool)-1)<.00001f,"Planckian light colors preserve luminance across warm and cool temperatures");
 bool valid=true;for(float t:{1000.f,2000.f,6500.f,15000.f,25000.f}){auto c=lightColorTemperature(t);valid&=std::isfinite(c.r+c.g+c.b)&&c.r>=0&&c.g>=0&&c.b>=0;}
 check(valid,"temperature colors remain finite and nonnegative across their supported range");
 bool rejected=false;try{lightColorTemperature(999);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"invalid light temperatures are rejected");
 auto settings=GraphicsSettings::high();settings.minExposureEV=3;settings.maxExposureEV=-2;settings.localExposure=2;
 auto effective=resolveSettings(settings,{}).effective;
 check(effective.minExposureEV==-2&&effective.maxExposureEV==3&&effective.localExposure==1,"exposure controls normalize reversed bounds and excessive local strength");
}
template<class Check>void strandTests(Check check){
 using namespace jsd;std::vector<V3> rest;for(int j=0;j<13;j++)rest.push_back({j*.15f,1.5f,0});
 StrandChain strand;strand.reset(rest);PhysicsWorld world({0,0,0},1);BodySettings floor;floor.motion=MotionType::Static;floor.position={0,-.5f,0};floor.halfExtent={10,.5f,10};world.box(floor);
 for(int j=0;j<360;j++)strand.advance(1.f/120,{0,1.5f,0},{.2f,-9.80665f,0},3,&world,{},.01f);
 float error=0,lowest=10;auto points=strand.points();for(size_t j=1;j<points.size();j++){error=std::max(error,std::abs(rf::distance(points[j],points[j-1])-.15f));lowest=std::min(lowest,points[j].y);}
 check(rf::distance(points[0],{0,1.5f,0})<1e-5f&&error<.025f,"strand dynamics preserve the pinned root and segment lengths");
 check(lowest>=-.005f&&lowest<.2f,"strand points respond to actual scene collision surfaces");
 auto mesh=strandRibbon(points,{0,2,-3},.01f);bool finite=true;for(auto &v:mesh)finite&=std::isfinite(v.p.x+v.p.y+v.p.z)&&rf::length(v.n)>.9f;
 check(finite&&mesh.size()==(points.size()-1)*6,"hair-compatible guide chains render as finite tapered ribbons");
}
