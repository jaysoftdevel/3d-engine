#include "../src/showcase.hpp"
#include <iostream>
namespace sundog {
struct ShowcasePhysicsProbe {
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
        return checks;
    }
};
}
int main(){try{
    jsd::Platform platform(true,320,180);rf::gfx::Renderer renderer(platform,rf::gfx::GraphicsSettings::minimal());
    auto assets=std::filesystem::read_symlink("/proc/self/exe").parent_path().parent_path()/"share/jsd-game-engine/showcase/assets";
    if(!std::filesystem::exists(assets/"showcase/title.png"))assets=JSD_SHOWCASE_TEST_ASSETS;
    sundog::Showcase scene(renderer,assets);int n=sundog::ShowcasePhysicsProbe::run(scene);
    std::cout<<n<<" showcase physics checks passed.\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAILED "<<e.what()<<'\n';return 1;}}
