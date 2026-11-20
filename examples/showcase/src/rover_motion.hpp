#pragma once
#include <jsd/vehicle.hpp>
#include <array>
#include <stdexcept>

namespace sundog {
// The showcase follows a prescribed route; wheel contact and odometry run in simulation ticks.
class RoverMotion {
    using Height = float (*)(float, float);
    Height height_ = nullptr;
    double radius_ = 1, speed_ = 0;
    jsd::Mat4 body_ = jsd::Mat4::identity();
    std::array<jsd::Mat4, 6> mounts_{};
    std::array<jsd::V3, 6> normals_{};
    std::array<jsd::RollingWheel, 6> wheels_{};
    static constexpr float HalfTrack = 1.48f, HalfWheelbase = 1.65f, HubHeight = .72f;

    static rf::Quat axis(jsd::V3 direction, double angle) {
        float sine = float(std::sin(angle * .5));
        return {direction.x * sine, direction.y * sine, direction.z * sine, float(std::cos(angle * .5))};
    }
    float ground(float x, float z) const {
        float value = height_ ? height_(x, z) : 0;
        if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite rover terrain height");
        return value;
    }
    jsd::V3 normal(float x, float z) const {
        constexpr float step = .05f;
        return rf::normalized({ground(x - step, z) - ground(x + step, z), 2 * step,
                               ground(x, z - step) - ground(x, z + step)});
    }
    void poseAtZ(float z) {
        if (!std::isfinite(z)) throw std::invalid_argument("Invalid rover position");
        float left = 0, right = 0, front = 0, rear = 0;
        for (float offset : {-HalfWheelbase, 0.f, HalfWheelbase}) {
            left += ground(-1 - HalfTrack, z + offset);
            right += ground(-1 + HalfTrack, z + offset);
        }
        for (float x : {-1 - HalfTrack, -1 + HalfTrack}) {
            front += ground(x, z + HalfWheelbase);
            rear += ground(x, z - HalfWheelbase);
        }
        float roll = std::atan2((right - left) / 3, 2 * HalfTrack);
        float pitch = -std::atan2((front - rear) / 2, 2 * HalfWheelbase);
        auto rotation = rf::compose({}, axis({0, 0, 1}, roll)) *
                        rf::compose({}, axis({1, 0, 0}, pitch));
        body_ = rf::compose({-1, 0, z}) * rotation;
        std::array<float, 6> contactHeight{};
        double heave = 0;
        size_t index = 0;
        for (float x : {-HalfTrack, HalfTrack})
            for (float offset : {-HalfWheelbase, 0.f, HalfWheelbase}) {
                auto mount = body_ * rf::compose({x, HubHeight, offset},
                    axis({0, 0, 1}, x > 0 ? -rf::pi / 2 : rf::pi / 2));
                auto center = rf::transformPoint(mount, {});
                auto axle = rf::normalized(rf::transformVector(mount, {0, 1, 0}));
                auto n = normal(center.x, center.z);
                float alignment = rf::dot(axle, n);
                // Circular wheel support against the local terrain plane, including camber.
                float support = float(radius_) * std::sqrt(std::max(0.f, 1 - alignment * alignment));
                contactHeight[index] = ground(center.x, center.z) + support / std::max(.001f, n.y);
                heave += contactHeight[index] - center.y;
                mounts_[index] = mount;
                normals_[index++] = n;
            }
        body_.m[13] = float(heave / 6);
        for (size_t i = 0; i < mounts_.size(); ++i) mounts_[i].m[13] = contactHeight[i];
    }

  public:
    explicit RoverMotion(double radius = 1, Height terrain = nullptr) : height_(terrain), radius_(radius) {
        for (auto &wheel : wheels_) wheel = jsd::RollingWheel(radius);
        reset();
    }
    void reset(double seconds = 0) {
        poseAtZ(routeZ(seconds));
        speed_ = 0;
        for (size_t i = 0; i < wheels_.size(); ++i)
            wheels_[i].reset(center(i), axle(i), normals_[i]);
    }
    static float routeZ(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid rover mission time");
        double u = std::clamp(seconds / 24., 0., 1.);
        return float(-35. + 38. * u * u * (3. - 2. * u));
    }
    void advance(double seconds, double step) { advanceToZ(routeZ(seconds), step); }
    // Odometry uses the accepted physical translation when a blocker stops the route.
    void advanceToZ(float z, double step) {
        if (!std::isfinite(step) || step <= 0)
            throw std::invalid_argument("Invalid rover step duration");
        float before = body_.m[14];
        poseAtZ(z);
        speed_ = (double(body_.m[14]) - before) / step;
        for (size_t i = 0; i < wheels_.size(); ++i)
            wheels_[i].advance(center(i), axle(i), normals_[i], step);
    }
    const jsd::Mat4 &body() const { return body_; }
    jsd::V3 center(size_t i) const { return rf::transformPoint(mounts_.at(i), {}); }
    jsd::V3 axle(size_t i) const { return rf::normalized(rf::transformVector(mounts_.at(i), {0, 1, 0})); }
    jsd::V3 surfaceNormal(size_t i) const { return normals_.at(i); }
    const jsd::RollingWheel &wheel(size_t i) const { return wheels_.at(i); }
    jsd::Mat4 wheelMatrix(size_t i) const {
        return mounts_.at(i) * rf::compose({}, axis({0, 1, 0}, wheels_.at(i).phase()));
    }
    double speed() const { return speed_; }
};
} // namespace sundog
