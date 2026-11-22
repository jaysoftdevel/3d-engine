#include "../src/rover_motion.hpp"
#include <iostream>
#include <numbers>

int main() {
    using namespace rf;
    try {
        int checks = 0;
        auto check = [&](bool value, const char *name) {
            if (!value) throw std::runtime_error(name);
            ++checks;
            std::cout << "PASS " << name << '\n';
        };
        constexpr double radius = .71474;
        for (int hz : {30, 60, 120, 240}) {
            sundog::RoverMotion motion(radius);
            std::array<double, 6> stopped{};
            for (int step = 1; step <= 24 * hz; ++step) motion.advance(double(step) / hz, 1. / hz);
            for (size_t i = 0; i < 6; ++i) {
                check(std::abs(std::abs(motion.wheel(i).signedDistance()) - 38) < 1e-5,
                      "every wheel covers the complete 38-metre route");
                check((i < 3) == (motion.wheel(i).angle() < 0),
                      "left/right showcase mounts use physically correct spin direction");
                stopped[i] = motion.wheel(i).angle();
            }
            for (int step = 1; step <= hz; ++step) motion.advance(24. + double(step) / hz, 1. / hz);
            for (size_t i = 0; i < 6; ++i)
                check(motion.wheel(i).angle() == stopped[i] && motion.wheel(i).angularSpeed() == 0,
                      "all wheels stop when rover translation stops");
            check(motion.speed() == 0, "dust emission can use actual stopped rover speed");
            auto first = motion.wheelMatrix(0);
            auto second = motion.wheelMatrix(0);
            check(first.m == second.m, "rendering wheel poses cannot advance the simulation");
            motion.reset();
            for (size_t i = 0; i < 6; ++i)
                check(motion.wheel(i).angle() == 0, "mission restart resets all six rolling histories");
        }
        auto slope = +[](float x, float z) { return .1f * x + .2f * z; };
        sundog::RoverMotion inclined(radius, slope);
        for (int step = 1; step <= 120; ++step) inclined.advance(double(step) / 120, 1. / 120);
        for (size_t i = 0; i < 6; ++i) {
            V3 center = inclined.center(i), axle = inclined.axle(i), n = inclined.surfaceNormal(i);
            V3 radial = normalized(n - axle * dot(axle, n));
            V3 contact = center - radial * float(radius);
            check(std::abs(contact.y - slope(contact.x, contact.z)) < 1e-4f,
                  "tilted wheel support follows the local terrain contact plane");
        }
        sundog::RoverMotion contact(radius);
        contact.advance(12, 12);
        auto before = contact.center(3);
        contact.advance(12. + 1. / 120, 1. / 120);
        V3 velocity = (contact.center(3) - before) * 120;
        V3 rolling = cross(contact.axle(3) * float(contact.wheel(3).angularSpeed()),
                           contact.surfaceNormal(3) * float(-radius));
        check(length(velocity + rolling) < 1e-4f, "assembled rover wheel has zero-slip contact velocity");
        jsd::PhysicsWorld world({0,0,0},1);
        jsd::BodySettings ground;ground.motion=jsd::MotionType::Static;ground.position={0,-.5f,0};ground.halfExtent={20,.5f,60};auto floor=world.box(ground);
        auto slab=ground;slab.motion=jsd::MotionType::Kinematic;slab.position={0,.05f,0};slab.halfExtent={8,.27f,5};auto platform=world.box(slab);
        sundog::RoverMotion crossing(radius);crossing.bindWorld(world);crossing.reset();
        bool frontFirst=false,pitched=false,allFinite=true;float maxStroke=0,maxHubStep=0;
        std::array<V3,6> last{};
        crossing.advanceToZ(-9,1);for(size_t i=0;i<6;i++)last[i]=crossing.center(i);
        for(int j=1;j<=600;j++){
            crossing.advanceToZ(-9+j*.015f,1./120);
            for(size_t i=0;i<6;i++){
                maxStroke=std::max(maxStroke,std::abs(crossing.compression(i)));
                maxHubStep=std::max(maxHubStep,std::abs(crossing.center(i).y-last[i].y));last[i]=crossing.center(i);
                allFinite&=std::isfinite(crossing.center(i).y);
            }
            frontFirst|=crossing.supportBody(2)==platform&&crossing.supportBody(0)==floor&&crossing.center(2).y>crossing.center(0).y+.04f;
            pitched|=crossing.body().m[6]<-.01f;
        }
        for(int i=0;i<180;i++)crossing.advanceToZ(0,1./120);
        check(allFinite,"wheel contact remains finite over the slab edge");
        check(frontFirst&&pitched,"front axle climbs the actual raised platform before the rear axle and pitches the hull");
        check(maxStroke>.025f&&maxHubStep<.09f,"individual suspension stroke is visible and tire contact does not jump across the edge");
        for(size_t i=0;i<6;i++)check(std::abs(crossing.center(i).y-float(radius)-.32f)<.003f&&crossing.supportBody(i)==platform,
            "all six tire hubs settle on the collider top, not the terrain height");
        float previousBody=crossing.body().m[13];
        world.teleport(platform,{0,.25f,0});crossing.advanceToZ(0,1./120);
        check(crossing.center(2).y>float(radius)+.5f&&crossing.body().m[13]<previousBody+.03f,
              "moving support is queried again while chassis heave remains damped");
        jsd::SuspensionSpring spring;spring.reset(0);spring.update(1,1./120);
        check(spring.position()>0&&spring.position()<.1,"spring responds to a contact change without snapping");
        std::cout << checks << " rover checks passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAILED " << error.what() << '\n';
        return 1;
    }
}
