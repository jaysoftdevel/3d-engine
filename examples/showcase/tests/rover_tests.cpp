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
        std::cout << checks << " rover checks passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAILED " << error.what() << '\n';
        return 1;
    }
}
