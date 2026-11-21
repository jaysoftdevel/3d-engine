#include "../src/showcase.hpp"
#include <iostream>
namespace sundog {
struct ShowcasePhysicsProbe {
    static void checkFinite(const gfx::Camera&c){if(!std::isfinite(c.position.x+c.position.y+c.position.z+c.direction.x+c.direction.y+c.direction.z+c.fovDegrees)||rf::length(c.direction)<.99f)throw std::runtime_error("Invalid extended camera sample");}
    static int run(Showcase &scene){
        int checks=0;auto check=[&](bool ok,const char *name){if(!ok)throw std::runtime_error(name);++checks;std::cout<<"PASS "<<name<<'\n';};
        BodySettings wall;wall.motion=MotionType::Static;wall.position={-1,3,-10};wall.halfExtent={5,3,.1f};auto blocker=scene.physics.box(wall);
        for(int i=0;i<2400;i++)scene.update(1.f/120);
        check(scene.roverBlocked&&scene.roverMotion.body().m[14]<-12.59f,"showcase rover stops at an actual scene blocker");
        auto angle=scene.roverMotion.wheel(0).angle();for(int i=0;i<120;i++)scene.update(1.f/120);
        check(std::abs(angle-scene.roverMotion.wheel(0).angle())<1e-6,"blocked rover wheels do not spin");
        scene.physics.remove(blocker);float before=scene.roverMotion.body().m[14];scene.update(1.f/120);
        check(scene.roverMotion.body().m[14]>before&&scene.roverMotion.body().m[14]-before<.021,"cleared route resumes without a catch-up teleport");
        scene.reset();for(int i=0;i<3000;i++)scene.update(1.f/120);
        check(std::abs(scene.roverMotion.body().m[14]-3)<.003,"clear showcase route reaches its original destination");
        std::cout<<"ROVER_END_Z "<<scene.roverMotion.body().m[14]<<'\n';
        for(int i=0;i<241;i++)scene.update(1.f/120);
        check(scene.liftedCargo.value!=~0u,"crane acquires a real kinematic cargo body");
        auto body=scene.liftedCargo;auto pickup=rf::transformPoint(scene.roverMotion.body(),{-.59f,2.42f,-1.05f});
        check(rf::distance(scene.physics.state(body).position,pickup)<.02,"cargo pickup starts at mounted case position");
        bool released=false;float maxReleaseJump=0;
        for(int i=0;i<1200;i++){
            auto old=scene.physics.state(body);bool wasReleased=scene.cargoReleased;scene.update(1.f/120);
            if(scene.cargoReleased&&!wasReleased){released=true;maxReleaseJump=rf::distance(old.position,scene.physics.state(body).position);}
        }
        check(released&&body==scene.liftedCargo,"release keeps the same solver body handle");
        check(maxReleaseJump<.01,"cargo release preserves position continuously");
        auto resting=scene.physics.state(body);std::cout<<"CARGO_REST "<<resting.position.x<<' '<<resting.position.y<<' '<<resting.position.z<<'\n';
        check(resting.position.x>4.6f&&resting.position.z>4&&resting.position.y>.53f&&resting.position.y<.8f,"cargo falls and rests on visible compound slab");
        check(rf::length(resting.velocity)<.1,"released cargo settles under damping and friction");
        scene.reset();check(scene.crates.empty()&&scene.liftedCargo.value==~0u,"mission reset removes released bodies");
        auto prior=Showcase::cameraAt(0);float maxStep=0,maxTurn=0;bool tunnelClear=true,solidClear=true;
        for(int i=1;i<=int(Showcase::Duration*120);i++){
            float time=i/120.f;auto camera=Showcase::cameraAt(time);maxStep=std::max(maxStep,rf::distance(prior.position,camera.position));maxTurn=std::max(maxTurn,rf::length(prior.direction-camera.direction));
            checkFinite(camera);auto p=camera.position;
            if(time>=73&&time<=146&&p.z>=89&&p.z<=184){float u=rf::clamp((p.z-99)/20);u=u*u*(3-2*u);float rx=rf::mix(8.f,26.f,u),ry=rf::mix(13.f,23.f,u);float dx=(p.x-128)/rx,dy=(p.y-8)/ry;tunnelClear&=p.y>8.4f&&dx*dx+dy*dy<.92f;}
            if(time>=58&&time<=152){
                for(auto&instance:scene.staticModels){auto local=rf::transformPoint(rf::inverse(instance.root),p),half=instance.model->halfExtent;if(std::abs(local.x)<half.x&&std::abs(local.y)<half.y&&std::abs(local.z)<half.z){if(solidClear)std::cout<<"MOUNTAIN_INTERSECTION "<<time<<" camera "<<p.x<<' '<<p.y<<' '<<p.z<<" mountain "<<instance.root.m[12]<<' '<<instance.root.m[13]<<' '<<instance.root.m[14]<<'\n';solidClear=false;}}
                for(auto&draw:scene.caveScenery){auto name=scene.caveNames.at(draw.objectId);if(name.starts_with("Refinery pressure vessel")){auto q=rf::transformPoint(rf::inverse(*draw.modelMatrix),p);if(std::abs(q.y)<.51f&&q.x*q.x+q.z*q.z<.26f){if(solidClear)std::cout<<"VESSEL_INTERSECTION "<<time<<'\n';solidClear=false;}}}
            }
            prior=camera;
        }
        std::cout<<"CAMERA_MAX_STEP "<<maxStep<<" TURN "<<maxTurn<<'\n';
        check(maxStep<.65f,"extended camera route has no position cuts at 120 Hz");
        check(maxTurn<.05f,"fast camera travel retains continuous viewing direction");
        check(solidClear,"camera clears existing mountain meshes and refinery vessel solids");
        check(tunnelClear,"camera remains inside the excavated cave volume and above the floor");
        auto end=Showcase::cameraAt(Showcase::Duration-.001f),start=Showcase::cameraAt(.001f);check(rf::distance(end.position,start.position)<.01f&&rf::distance(end.direction,start.direction)<.001f,"loop rejoins the opening camera without a cut");
        auto endV=(Showcase::cameraAt(Showcase::Duration-.001f).position-Showcase::cameraAt(Showcase::Duration-.002f).position)*1000;
        auto startV=(Showcase::cameraAt(.002f).position-Showcase::cameraAt(.001f).position)*1000;
        check(rf::distance(endV,startV)<.08f,"camera loop also matches travel velocity");
        check(scene.caveLights.size()>=24&&scene.caveEffects.size()==8&&scene.caveScenery.size()>600,"resource plant includes substantial geometry lights and seeded effects");
        check(Showcase::caveBlend(70)==0&&Showcase::caveBlend(90)==1&&Showcase::caveBlend(150)==0,"interior lighting transitions smoothly during entry and exit");
        return checks;
    }
};
}
int main(){try{
    jsd::Platform platform(true,320,180);rf::gfx::Renderer renderer(platform,rf::gfx::GraphicsSettings::minimal());
    auto assets=rf::executableDirectory().parent_path()/"share/jsd-game-engine/showcase/assets";
    if(!std::filesystem::exists(assets/"showcase/title.png"))assets=JSD_SHOWCASE_TEST_ASSETS;
    sundog::Showcase scene(renderer,assets);int n=sundog::ShowcasePhysicsProbe::run(scene);
    std::cout<<n<<" showcase physics checks passed.\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAILED "<<e.what()<<'\n';return 1;}}
