#pragma once
#include "runtime.hpp"
#include "physics.hpp"

namespace jsd {
struct WheelSupport {
    V3 point{}, normal{0,1,0}; Body body{}; float hubHeight=0;
};
// Query the circular tire footprint against scene bodies and an optional terrain.
// The caller excludes its own chassis through QueryFilter::ignore.
std::optional<WheelSupport> wheelSupport(const PhysicsWorld&,V3 hub,V3 axle,float radius,
    const std::function<float(float,float)>& terrain={},QueryFilter filter={});
class SuspensionSpring {
    double position_=0,velocity_=0;
public:
    void reset(double value){position_=value;velocity_=0;}
    void update(double target,double seconds,double frequencyHz=3);
    double position()const{return position_;}
    double velocity()const{return velocity_;}
};
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
