#include "jsd/physics.hpp"

// Jolt.h defines configuration macros required by every other Jolt header.
#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <mutex>
#include <set>
namespace jsd {
namespace {
struct JoltRuntime {
    JoltRuntime() {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory;
        JPH::RegisterTypes();
    }
    ~JoltRuntime() {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
};
JoltRuntime &initialize() {
    static JoltRuntime runtime;
    return runtime;
}
JPH::Vec3 vec(V3 p) {
    return {p.x, p.y, p.z};
}
V3 vec(JPH::Vec3 p) {
    return {p.GetX(), p.GetY(), p.GetZ()};
}
struct Broadphase : JPH::BroadPhaseLayerInterface {
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l) const override {
        return JPH::BroadPhaseLayer(l);
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer l) const override {
        return l.GetValue() == 0 ? "Static" : "Moving";
    }
#endif
};
struct PairFilter : JPH::ObjectLayerPairFilter {
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override { return a != 0 || b != 0; }
};
struct ObjectFilter : JPH::ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override {
        return a != 0 || b.GetValue() != 0;
    }
};
} // namespace
struct PhysicsWorld::Impl : JPH::ContactListener {
    Broadphase broad;
    PairFilter pairs;
    ObjectFilter objects;
    JPH::PhysicsSystem system;
    JPH::TempAllocatorImpl temporary;
    JPH::JobSystemThreadPool jobs;
    std::set<uint32_t> bodies;
    std::mutex mutex;
    std::vector<Contact> events;
    Impl(V3 gravity, unsigned workers)
        : temporary(16 * 1024 * 1024),
          jobs(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::clamp(workers, 1u, 8u)) {
        system.Init(8192, 0, 32768, 16384, broad, objects, pairs);
        system.SetGravity(vec(gravity));
        system.SetContactListener(this);
    }
    void OnContactAdded(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &,
                        JPH::ContactSettings &) override {
        std::lock_guard lock(mutex);
        events.push_back(
            {{a.GetID().GetIndexAndSequenceNumber()}, {b.GetID().GetIndexAndSequenceNumber()}, true});
    }
    void OnContactRemoved(const JPH::SubShapeIDPair &p) override {
        std::lock_guard lock(mutex);
        events.push_back({{p.GetBody1ID().GetIndexAndSequenceNumber()},
                          {p.GetBody2ID().GetIndexAndSequenceNumber()},
                          false});
    }
    Body create(const BodySettings &s, JPH::RefConst<JPH::Shape> shape) {
        auto q = rf::normalize(s.rotation);
        if (!std::isfinite(s.position.x) || !std::isfinite(s.position.y) || !std::isfinite(s.position.z) ||
            s.mass <= 0 || !std::isfinite(s.mass))
            throw std::invalid_argument("invalid physics body");
        JPH::BodyCreationSettings c(
            shape, JPH::RVec3(s.position.x, s.position.y, s.position.z), JPH::Quat(q.x, q.y, q.z, q.w),
            s.dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static, s.dynamic ? 1 : 0);
        c.mFriction = s.friction;
        c.mRestitution = s.restitution;
        c.mIsSensor = s.sensor;
        c.mUserData = s.userData;
        c.mMotionQuality = s.continuous ? JPH::EMotionQuality::LinearCast : JPH::EMotionQuality::Discrete;
        c.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        c.mMassPropertiesOverride.mMass = s.mass;
        auto id = system.GetBodyInterface().CreateAndAddBody(c, s.dynamic ? JPH::EActivation::Activate
                                                                          : JPH::EActivation::DontActivate);
        if (id.IsInvalid())
            throw std::runtime_error("physics body budget exceeded");
        bodies.insert(id.GetIndexAndSequenceNumber());
        return {id.GetIndexAndSequenceNumber()};
    }
    JPH::BodyID id(Body b) const {
        if (!bodies.contains(b.value))
            throw std::invalid_argument("stale physics body");
        return JPH::BodyID(b.value);
    }
};
PhysicsWorld::PhysicsWorld(V3 g, unsigned workers) {
    initialize();
    impl = std::make_unique<Impl>(g, workers);
}
PhysicsWorld::~PhysicsWorld() {
    for (auto b : std::vector<uint32_t>(impl->bodies.begin(), impl->bodies.end()))
        remove({b});
}
Body PhysicsWorld::box(const BodySettings &s) {
    if (!std::isfinite(s.halfExtent.x) || !std::isfinite(s.halfExtent.y) || !std::isfinite(s.halfExtent.z) ||
        s.halfExtent.x <= 0 || s.halfExtent.y <= 0 || s.halfExtent.z <= 0)
        throw std::invalid_argument("invalid box");
    return impl->create(
        s,
        new JPH::BoxShape(vec(s.halfExtent),
                          std::min(.02f, std::min({s.halfExtent.x, s.halfExtent.y, s.halfExtent.z}) * .1f)));
}
Body PhysicsWorld::sphere(const BodySettings &s, float r) {
    if (r <= 0 || !std::isfinite(r))
        throw std::invalid_argument("invalid sphere");
    return impl->create(s, new JPH::SphereShape(r));
}
Body PhysicsWorld::capsule(const BodySettings &s, float h, float r) {
    if (h <= 0 || r <= 0 || !std::isfinite(h + r))
        throw std::invalid_argument("invalid capsule");
    return impl->create(s, new JPH::CapsuleShape(h, r));
}
void PhysicsWorld::remove(Body b) {
    auto id = impl->id(b);
    auto &api = impl->system.GetBodyInterface();
    api.RemoveBody(id);
    api.DestroyBody(id);
    impl->bodies.erase(b.value);
}
BodyState PhysicsWorld::state(Body b) const {
    auto id = impl->id(b);
    auto &api = impl->system.GetBodyInterface();
    auto p = api.GetPosition(id);
    auto q = api.GetRotation(id);
    return {{float(p.GetX()), float(p.GetY()), float(p.GetZ())},
            vec(api.GetLinearVelocity(id)),
            {q.GetX(), q.GetY(), q.GetZ(), q.GetW()},
            api.IsActive(id)};
}
void PhysicsWorld::impulse(Body b, V3 v) {
    impl->system.GetBodyInterface().AddImpulse(impl->id(b), vec(v));
}
void PhysicsWorld::velocity(Body b, V3 v) {
    impl->system.GetBodyInterface().SetLinearVelocity(impl->id(b), vec(v));
}
void PhysicsWorld::step(float dt, unsigned count) {
    if (!std::isfinite(dt) || dt <= 0 || dt > .25f || count < 1 || count > 8)
        throw std::invalid_argument("invalid physics step");
    auto error = impl->system.Update(dt, count, &impl->temporary, &impl->jobs);
    if (error != JPH::EPhysicsUpdateError::None)
        throw std::runtime_error("physics simulation budget exhausted");
}
std::optional<RayHit> PhysicsWorld::rayCast(V3 from, V3 to) const {
    JPH::RRayCast ray(JPH::RVec3(from.x, from.y, from.z), vec(to - from));
    JPH::RayCastResult hit;
    if (!impl->system.GetNarrowPhaseQuery().CastRay(ray, hit))
        return {};
    return RayHit{
        {hit.mBodyID.GetIndexAndSequenceNumber()}, hit.mFraction, from + (to - from) * hit.mFraction};
}
std::vector<Contact> PhysicsWorld::contacts() {
    std::lock_guard lock(impl->mutex);
    std::vector<Contact> result;
    result.swap(impl->events);
    return result;
}
const char *PhysicsWorld::backend() {
    return "Jolt Physics 5.7.0";
}
} // namespace jsd
