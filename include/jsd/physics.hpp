#pragma once
#include "runtime.hpp"
namespace jsd {
struct Body {
    uint32_t value = ~0u;
    bool operator==(const Body &) const = default;
};
struct BodyState {
    V3 position, velocity;
    Quat rotation;
    bool active = false;
};
struct BodySettings {
    V3 position, halfExtent{.5f, .5f, .5f};
    Quat rotation;
    float mass = 1, friction = .6f, restitution = .1f;
    bool dynamic = true, continuous = false, sensor = false;
    uint64_t userData = 0;
};
struct RayHit {
    Body body;
    float fraction = 1;
    V3 position;
};
struct Contact {
    Body a, b;
    bool began = true;
};
class PhysicsWorld {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    explicit PhysicsWorld(V3 gravity = {0, -9.81f, 0}, unsigned workers = 2);
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld &) = delete;
    PhysicsWorld &operator=(const PhysicsWorld &) = delete;
    Body box(const BodySettings &);
    Body sphere(const BodySettings &, float radius);
    Body capsule(const BodySettings &, float halfHeight, float radius);
    void remove(Body);
    BodyState state(Body) const;
    void impulse(Body, V3);
    void velocity(Body, V3);
    void step(float seconds, unsigned collisionSteps = 1);
    std::optional<RayHit> rayCast(V3 from, V3 to) const;
    std::vector<Contact> contacts();
    static const char *backend();
};
} // namespace jsd
