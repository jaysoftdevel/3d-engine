#pragma once
#include <jsd/vehicle.hpp>
#include <numbers>
#include <limits>

template <class Check> void vehicleTests(Check check) {
    using namespace jsd;
    constexpr double radius = .715, tau = 2 * std::numbers::pi;
    const V3 up{0, 1, 0}, right{1, 0, 0}, left{-1, 0, 0};
    RollingWheel wheel(radius);
    wheel.reset({}, right, up);
    wheel.advance({0, 0, float(tau * radius)}, right, up, 1);
    check(std::abs(wheel.angle() - tau) < 1e-6,
          "one tyre circumference produces exactly one forward revolution");
    wheel.advance({}, right, up, 1);
    check(std::abs(wheel.angle()) < 1e-9 && wheel.angularSpeed() < 0,
          "reversing travel reverses wheel rotation without residual drift");
    for (auto axle : {left, right}) {
        RollingWheel side(radius);
        side.reset({}, axle, up);
        side.advance({0, 0, 2}, axle, up, .5);
        V3 contactVelocity = V3{0, 0, 4} + rf::cross(axle * float(side.angularSpeed()), up * float(-radius));
        check(rf::length(contactVelocity) < 1e-5f,
              "both mirrored wheel axles satisfy zero-slip ground contact");
        check((axle.x > 0) == (side.angle() > 0),
              "mirrored wheel meshes rotate in opposite local directions");
    }
    wheel.reset({}, right, up);
    wheel.advance({2, 3, 0}, right, up, 1);
    check(wheel.angle() == 0, "suspension movement and sideways drift do not spin grounded wheels");
    V3 normal = rf::normalized({0, 1, -.5f});
    wheel.reset({}, right, normal);
    wheel.advance({0, 2, 4}, right, normal, 1);
    check(std::abs(wheel.signedDistance() - std::sqrt(20.)) < 1e-6,
          "inclined terrain uses three-dimensional surface travel");
    double reference = 0;
    for (int hz : {30, 60, 120, 240}) {
        wheel.reset({}, right, up);
        for (int step = 1; step <= hz; ++step)
            wheel.advance({0, 0, float(10. * step / hz)}, right, up, 1. / hz);
        if (hz == 30) reference = wheel.angle();
        check(std::abs(wheel.angle() - reference) < 1e-9,
              "distance-driven rotation is independent of update frequency");
    }
    wheel.reset({1000, 0, 1000}, right, up);
    check(wheel.angle() == 0 && wheel.angularSpeed() == 0,
          "teleport/reset starts fresh odometry without phantom wheel travel");
    wheel.advance({1000, 0, 1000}, right, up, 1);
    check(wheel.angle() == 0 && wheel.angularSpeed() == 0, "stationary wheels stay stationary");
    bool rejected = false;
    try { RollingWheel invalid(0); } catch (const std::invalid_argument &) { rejected = true; }
    check(rejected, "zero wheel radius is rejected");
    rejected = false;
    double before = wheel.angle();
    try { wheel.advance({}, right, up, std::numeric_limits<double>::quiet_NaN()); }
    catch (const std::invalid_argument &) { rejected = true; }
    check(rejected && wheel.angle() == before, "invalid wheel timesteps leave odometry unchanged");
    rejected = false;
    try { wheel.advance({}, up, up, 1); } catch (const std::invalid_argument &) { rejected = true; }
    check(rejected && wheel.angle() == before, "degenerate contact frames cannot corrupt wheel state");
}
