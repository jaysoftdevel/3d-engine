#include "jsd/vehicle.hpp"
#include <numbers>
#include <stdexcept>

namespace jsd {
namespace {
bool finite(V3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
V3 tangent(V3 center, V3 axle, V3 normal) {
    if (!finite(center) || !finite(axle) || !finite(normal) ||
        rf::length(axle) < 1e-6f || rf::length(normal) < 1e-6f)
        throw std::invalid_argument("Invalid rolling-wheel position or contact frame");
    V3 result = rf::cross(rf::normalized(axle), rf::normalized(normal));
    if (rf::length(result) < 1e-6f)
        throw std::invalid_argument("Wheel axle cannot be parallel to its contact normal");
    return rf::normalized(result);
}
}
RollingWheel::RollingWheel(double radius) : radius_(radius) {
    if (!std::isfinite(radius) || radius <= 0)
        throw std::invalid_argument("Wheel radius must be finite and positive");
}
void RollingWheel::reset(V3 center, V3 axle, V3 normal) {
    V3 direction = tangent(center, axle, normal);
    position_ = center;
    tangent_ = direction;
    distance_ = angularSpeed_ = 0;
    initialized_ = true;
}
void RollingWheel::advance(V3 center, V3 axle, V3 normal, double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0)
        throw std::invalid_argument("Wheel step duration must be finite and positive");
    V3 direction = tangent(center, axle, normal);
    if (!initialized_) { reset(center, axle, normal); return; }
    V3 average = rf::normalized(tangent_ + direction);
    if (rf::length(average) < 1e-6f)
        throw std::invalid_argument("Reset rolling history after discontinuous axle changes");
    // Project onto the rolling tangent: suspension bounce and lateral slip do not add spin.
    double travel = (double(center.x) - position_.x) * average.x +
                    (double(center.y) - position_.y) * average.y +
                    (double(center.z) - position_.z) * average.z;
    double accumulated = distance_ + travel;
    double speed = travel / (radius_ * seconds);
    if (!std::isfinite(accumulated / radius_) || !std::isfinite(speed))
        throw std::invalid_argument("Rolling-wheel motion exceeds the supported numeric range");
    distance_ = accumulated;
    angularSpeed_ = speed;
    position_ = center;
    tangent_ = direction;
}
double RollingWheel::phase() const { return std::remainder(angle(), 2 * std::numbers::pi); }
} // namespace jsd
