#pragma once
#include "runtime.hpp"
#include "units.hpp"
namespace jsd {
struct Body { uint32_t value = ~0u; bool operator==(const Body &) const = default; };
enum class MotionType { Legacy, Static, Kinematic, Dynamic };
enum class MassMode { ExplicitKilograms, Density };
struct BodyState {
    V3 position, velocity; Quat rotation; bool active = false;
    V3 angularVelocity; MotionType motion = MotionType::Static;
};
struct BodySettings {
    V3 position, halfExtent{.5f,.5f,.5f}; Quat rotation;
    float mass=1, friction=.6f, restitution=.1f;
    bool dynamic=true, continuous=false, sensor=false; uint64_t userData=0;
    MotionType motion=MotionType::Legacy;
    MassMode massMode=MassMode::ExplicitKilograms;
    double densityKgPerM3=1000;
    float linearDampingPerSecond=.05f, angularDampingPerSecond=.05f, gravityScale=1;
    float maxLinearSpeedMps=500, maxAngularSpeedRadPerSecond=100;
    // Optional aerodynamic drag: F = -0.5 rho Cd A |v-wind| (v-wind).
    float dragCoefficient=0, dragAreaM2=0;
};
struct MassProperties { double massKg=0, volumeM3=0; V3 inertiaDiagonalKgM2; double inverseMassPerKg=0; };
struct RayHit { Body body; float fraction=1; V3 position; V3 normal; };
struct Contact { Body a,b; bool began=true; V3 position,normal; bool sensor=false; };
struct QueryFilter { Body ignore{}; bool includeSensors=false; };
struct MoveOptions { bool block=true; float skinMetres=.002f; Body ignore{}; };
struct MovementResult {
    V3 requestedPosition, acceptedPosition, normal; Quat acceptedRotation;
    Body blocker{}; float fraction=1; bool blocked=false;
};
struct WorldSettings {
    V3 gravityMps2{0,-9.80665f,0}; unsigned workers=0;
    double fixedStepSeconds=1./120; unsigned maxStepsPerAdvance=32, maxBodies=8192;
    float airDensityKgM3=1.225f; V3 windMps{};
};
struct AdvanceResult { unsigned steps=0; double simulatedSeconds=0, pendingSeconds=0; float interpolationAlpha=0; bool behind=false; };
struct PhysicsStatistics { uint64_t simulationSteps=0; double simulatedSeconds=0; unsigned bodies=0; };
class PhysicsWorld {
    struct Impl; std::unique_ptr<Impl> impl;
public:
    explicit PhysicsWorld(V3 gravity={0,-9.80665f,0},unsigned workers=0);
    explicit PhysicsWorld(const WorldSettings &);
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld &)=delete; PhysicsWorld &operator=(const PhysicsWorld &)=delete;
    Body box(const BodySettings &); Body sphere(const BodySettings &,float radiusMetres);
    Body capsule(const BodySettings &,float halfCylinderHeightMetres,float radiusMetres);
    Body cylinder(const BodySettings &,float halfHeightMetres,float radiusMetres);
    void remove(Body); BodyState state(Body) const;
    BodyState interpolatedState(Body,float alpha) const;
    MassProperties massProperties(Body) const;
    void impulse(Body,V3 newtonSeconds); void impulseAt(Body,V3 newtonSeconds,V3 worldPointMetres);
    void angularImpulse(Body,V3 kgM2PerSecond);
    void force(Body,V3 newtons); void forceAt(Body,V3 newtons,V3 worldPointMetres);
    void torque(Body,V3 newtonMetres);
    void continuousForce(Body,V3 newtons); void clearForces(Body);
    void velocity(Body,V3 metresPerSecond); void angularVelocity(Body,V3 radiansPerSecond);
    void motionType(Body,MotionType);
    void teleport(Body,V3 positionMetres,Quat rotation={},bool clearVelocity=true);
    MovementResult moveKinematic(Body,V3 targetMetres,Quat rotation,float seconds,MoveOptions={});
    std::optional<RayHit> sweep(Body,V3 displacementMetres,QueryFilter={}) const;
    std::optional<RayHit> rayCast(V3 fromMetres,V3 toMetres,QueryFilter={}) const;
    bool occluded(V3 fromMetres,V3 toMetres,QueryFilter filter={}) const { return rayCast(fromMetres,toMetres,filter).has_value(); }
    void gravity(V3 metresPerSecondSquared); V3 gravity() const;
    double kineticEnergyJoules(Body) const; double potentialEnergyJoules(Body) const;
    // Explicit intervals are internally subdivided at the configured stable step.
    // force()/torque() last for this interval; continuousForce() persists until replaced.
    void step(float seconds,unsigned collisionSteps=1);
    // Frame-time accumulator. The callback runs once before each fixed simulation tick.
    AdvanceResult advance(double frameSeconds,const std::function<void(PhysicsWorld &,float)> &beforeTick={});
    void resetClock(); PhysicsStatistics statistics() const;
    std::vector<Contact> contacts(); static const char *backend();
};
struct KinematicMotorSettings { float maxSpeedMps=4, accelerationMps2=8, arrivalToleranceMetres=.001f; };
class KinematicMotor {
    V3 velocity_{};
public:
    void reset(){velocity_={};}
    V3 velocity() const{return velocity_;}
    MovementResult moveTowards(PhysicsWorld &,Body,V3 goalMetres,float seconds,
                              const KinematicMotorSettings &settings={},MoveOptions options={});
};
}
