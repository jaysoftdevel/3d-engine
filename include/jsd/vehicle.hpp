#pragma once
#include "runtime.hpp"

namespace jsd {
// Grounded-wheel odometry. Axle direction follows the mesh's positive rotation axis.
class RollingWheel {
    double radius_ = 1, distance_ = 0, angularSpeed_ = 0;
    V3 position_{}, tangent_{};
    bool initialized_ = false;

  public:
    RollingWheel() : RollingWheel(1) {}
    explicit RollingWheel(double radius);
    // Repositioning/teleporting starts a new rolling history, without phantom travel.
    void reset(V3 center, V3 axle, V3 surfaceNormal);
    void advance(V3 center, V3 axle, V3 surfaceNormal, double seconds);
    double radius() const { return radius_; }
    double signedDistance() const { return distance_; }
    double angle() const { return distance_ / radius_; }
    double angularSpeed() const { return angularSpeed_; }
    double phase() const;
};
} // namespace jsd
