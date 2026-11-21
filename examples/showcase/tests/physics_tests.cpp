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
                for(auto&draw:scene.caveScenery){auto name=scene.caveNames.at(draw.objectId);if(name.starts_with("Process water settling tank")){auto q=rf::transformPoint(rf::inverse(*draw.modelMatrix),p);if(std::abs(q.y)<.51f&&q.x*q.x+q.z*q.z<.26f){if(solidClear)std::cout<<"VESSEL_INTERSECTION "<<time<<'\n';solidClear=false;}}}
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
        check(scene.caveEffects.size()==8,"bounded process emitters keep stable export identities");
        jsd::project::TerrainData exported;exported.generator="sundog-v2";exported.validate();
        bool terrainSame=true;for(int z=-40;z<200;z+=7)for(int x=-100;x<200;x+=11)
            terrainSame&=std::abs(exported.height(float(x),float(z))-Showcase::height(float(x),float(z)))<1e-6f;
        check(terrainSame,"exported terrain and showcase use the same graded geology");
        int stairTreads=0,motors=0,stringers=0;bool closedDuct=false,feed=false;float priorY=0;
        for(const auto &draw:scene.caveScenery){
            const auto& name=scene.caveNames.at(draw.objectId);
            if(name=="Gallery stair tread"){
                float y=draw.modelMatrix->m[13];
                if(stairTreads)check(y-priorY>.17f&&y-priorY<.19f,"stair risers remain walkable");
                priorY=y;stairTreads++;
            }
            motors+=name=="Enclosed tank agitator gearbox";
            stringers+=name=="Stair load-bearing stringer";
            closedDuct|=name=="Main extraction duct / exhausted outside portal";
            feed|=name=="Conveyor covered discharge chute";
            check(!name.starts_with("Molten")&&!name.starts_with("Furnace"),"no open underground furnace or slag channel");
        }
        check(stairTreads==42&&stringers==2&&motors==3&&closedDuct&&feed,
              "stairs have stringers, tank drives are enclosed, conveyor and extraction connect");
        check(std::abs(Showcase::height(128,87)-7.95f)<.02f,"service approach meets the portal floor without a floating slab");
        check(!Showcase::enclosedView(Showcase::cameraAt(68)),"exterior terrain remains visible on the approach");
        check(!Showcase::enclosedView(Showcase::cameraAt(92),4),"wide aspect ratios retain a portal at the edge of view");
        auto interior=Showcase::cameraAt(84);check(Showcase::enclosedView(interior),"opaque rock encloses the forward gallery view");
        interior.direction=rf::normalized(V3{128,14,88}-interior.position);
        check(!Showcase::enclosedView(interior),"looking out through the portal preserves exterior rendering");
        interior.position={150,32,140};interior.direction={0,0,1};
        check(!Showcase::enclosedView(interior),"above-vault views never hide exterior scenery");
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
