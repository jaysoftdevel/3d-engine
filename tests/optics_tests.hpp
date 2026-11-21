#pragma once
#include <frontier/renderer.hpp>
#include <fstream>
#include <iostream>
#include <numbers>
#include "process_id.hpp"
template<class Check> void opticsTests(Check check){
    using namespace rf;using namespace rf::gfx;
    auto near=[](double a,double b,double e=1e-6){return std::abs(a-b)<e;};
    CameraOptics lens;
    check(near(lens.verticalFovDegrees(),26.99146656,1e-4),"50 mm lens and 24 mm sensor define vertical field of view");
    check(lens.circleOfConfusionMetres(5,5)==0,"thin lens is sharp at focus distance");
    double wide=lens.blurRadiusPixels(10,5,1080);lens.fNumber*=2;
    check(near(wide,lens.blurRadiusPixels(10,5,1080)*2),"stopping down halves physical blur radius");
    lens.fNumber=16;lens.shutterSeconds=1.f/125;
    check(near(lens.ev100(),std::log2(32000.),1e-5),"photographic EV100 from aperture shutter ISO");
    double exposure=lens.exposureMultiplier();lens.iso*=2;
    check(near(lens.exposureMultiplier(),exposure*2),"doubling ISO doubles exposure");
    check(near(pointCandela(4*std::numbers::pi),1),"isotropic luminous flux to candela");
    check(near(illuminanceLux(100,2),25)&&near(illuminanceLux(100,4),6.25),"inverse square illuminance in lux");
    check(near(illuminanceLux(100,2,.5),12.5),"Lambert incidence cosine");
    double inner=.3,outer=.6,sum=0;
    for(int i=0;i<10000;i++){double theta=(i+.5)*outer/10000,c=std::cos(theta),t=std::clamp((c-std::cos(outer))/(std::cos(inner)-std::cos(outer)),0.,1.);sum+=t*t*(3-2*t)*std::sin(theta)*outer/10000*2*std::numbers::pi;}
    check(near(spotCandela(800,inner,outer)*sum,800,1e-3),"spotlight smooth cone conserves specified lumens");
    auto p=pointLightLumens({},800,20,{1,.5f,.2f,1});
    check(p.falloff==LightFalloff::InverseSquare&&near((.2126*p.color.r+.7152*p.color.g+.0722*p.color.b)*p.intensity,pointCandela(800),1e-4),"colored light retains photometric intensity");
    auto t=beerLambert({1,2,0},2);check(near(t.x,std::exp(-2.))&&near(t.y,std::exp(-4.))&&t.z==1,"Beer Lambert attenuation per metre");
    check(near(dielectricFresnel(1,1,1.5),.04),"dielectric normal incidence reflectance");
    check(dielectricFresnel(0,1,1)==0,"equal media have no interface reflection");
    check(dielectricFresnel(.5,1.5,1)==1,"total internal reflection returns full reflectance");
    auto ray=refractDirection({.5f,-.8660254f,0},{0,1,0},1,1.5);
    check(ray&&near(ray->x,1./3,1e-6),"Snell refraction direction");
    check(!refractDirection({.8660254f,-.5f,0},{0,1,0},1.5,1),"Snell total internal reflection reports no transmitted ray");
}
template<class Check> void opticsGraphicsTests(Check check,rf::gfx::Renderer &renderer){
    using namespace rf;using namespace rf::gfx;
    auto capture=[&]{
        renderer.end();renderer.check();auto path=std::filesystem::temp_directory_path()/("jsd-optics-"+std::to_string(jsdTestProcessId())+".ppm");renderer.screenshot(path);
        std::ifstream file(path,std::ios::binary);std::string magic;int w,h,range;file>>magic>>w>>h>>range;file.get();std::vector<unsigned char> pixels(w*h*3);file.read(reinterpret_cast<char*>(pixels.data()),pixels.size());std::filesystem::remove(path);return pixels;
    };
    auto settings=GraphicsSettings::minimal();settings.viewDistance=64;settings.autoExposure=false;settings.bloom=Quality::Off;settings.volumetricFog=Quality::Off;settings.fogDensity=0;settings.toneMapper=ToneMapper::Reinhard;settings.vignette=0;settings.contrast=settings.saturation=1;settings.exposureCompensation=0;
    renderer.setSettings(settings);Environment env;env.sunIntensity=env.ambientIntensity=0;env.fogDensity=0;
    Camera camera{{0,0,0},{0,0,1},60,.08f,{}};
    auto quad=renderer.createMesh(std::vector<Vertex>{
        {{-8,-8,10},{0,0,-1},{},0,0},{{8,8,10},{0,0,-1},{},1,1},{{8,-8,10},{0,0,-1},{},1,0},
        {{-8,-8,10},{0,0,-1},{},0,0},{{-8,8,10},{0,0,-1},{},0,1},{{8,8,10},{0,0,-1},{},1,1}});
    Draw object;object.mesh=quad;object.material.albedo={.5,.5,.5,1};object.material.roughness=1;object.material.castShadow=false;
    auto centerRadiance=[&](float distance){
        renderer.begin(camera,env);renderer.submit(object);renderer.light(pointLightLumens({0,0,10-distance},4*std::numbers::pi,30));auto image=capture();auto pixel=image[(renderer.statistics().outputHeight/2*renderer.statistics().outputWidth+renderer.statistics().outputWidth/2)*3];double linear=std::pow(pixel/255.,2.2);return linear/(1-linear);
    };
    double nearLight=centerRadiance(2),farLight=centerRadiance(4);
    std::cout<<"PHOTOMETRY "<<nearLight<<" "<<farLight<<" ratio "<<nearLight/farLight<<"\n";
    check(nearLight/farLight>3.6&&nearLight/farLight<4.4,"GPU photometric light follows inverse square irradiance");
    CameraOptics lens;lens.focalLengthMetres=.024f/(2*std::tan(60*rf::pi/360));lens.fNumber=2;lens.shutterSeconds=1;lens.physicalExposure=true;camera.optics=lens;
    auto exposed=[&](float iso){camera.optics->iso=iso;renderer.begin(camera,env);Draw glow=object;glow.material.emissive={.1,.1,.1,1};renderer.submit(glow);return capture();};
    auto iso100=exposed(100),iso200=exposed(200);check(iso100!=iso200,"GPU physical camera exposure responds to ISO");
    settings.depthOfField=Quality::High;settings.autoFocus=true;settings.maximumBlur=24;renderer.setSettings(settings);
    renderer.begin(camera,env);renderer.submit(object);capture();check(renderer.statistics().postPasses>0,"thin lens depth of field and GPU autofocus execute together");
    settings.autoFocus=false;settings.focusDistance=1;settings.motionBlur=Quality::Off;renderer.setSettings(settings);
    camera.optics->physicalExposure=false;camera.optics->focalLengthMetres=.05f;
    object.transform.scale={.08f,.25f,1};object.material.emissive={.6f,.6f,.6f,1};object.objectId=901234;
    auto apertureFrame=[&](float fNumber){camera.optics->fNumber=fNumber;renderer.resetHistory();renderer.begin(camera,env);renderer.submit(object);return capture();};
    auto open=apertureFrame(1),stopped=apertureFrame(16);
    check(open!=stopped,"GPU thin lens aperture changes defocused edge pixels");
    settings.depthOfField=Quality::Off;settings.motionBlur=Quality::High;renderer.setSettings(settings);env.deltaSeconds=1.f/60;
    auto shutterFrame=[&](float seconds){camera.optics->shutterSeconds=seconds;renderer.resetHistory();std::vector<unsigned char> result;
        for(int i=0;i<4;i++){object.transform.position.x=(i-2)*.5f;renderer.begin(camera,env);renderer.submit(object);result=capture();}return result;};
    auto shortShutter=shutterFrame(1.f/1000),longShutter=shutterFrame(1.f/30);
    check(shortShutter!=longShutter,"GPU physical shutter duration changes motion blur pixels");
    renderer.destroyMesh(quad);
}
