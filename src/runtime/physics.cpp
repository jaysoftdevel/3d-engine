#include "jsd/physics.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <frontier/jobs.hpp>
#include <map>
#include <mutex>
#include <stdexcept>
namespace jsd {
namespace {
bool finite(V3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
void vectorCheck(V3 v){if(!finite(v))throw std::invalid_argument("Nonfinite SI physics vector");}
void positive(double v){if(!std::isfinite(v)||v<=0)throw std::invalid_argument("Physics dimensions/masses must be finite and positive");}
Quat rotationCheck(Quat q){
    double n=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
    if(!std::isfinite(n)||n<1e-12)throw std::invalid_argument("Invalid physics quaternion");
    return rf::normalize(q);
}
JPH::Vec3 vec(V3 p){return {p.x,p.y,p.z};} V3 vec(JPH::Vec3 p){return {p.GetX(),p.GetY(),p.GetZ()};}
JPH::Quat quat(Quat q){return {q.x,q.y,q.z,q.w};}
JPH::EMotionType mode(MotionType m){return m==MotionType::Static?JPH::EMotionType::Static:m==MotionType::Kinematic?JPH::EMotionType::Kinematic:JPH::EMotionType::Dynamic;}
struct Runtime {
    Runtime(){JPH::RegisterDefaultAllocator();JPH::Factory::sInstance=new JPH::Factory;JPH::RegisterTypes();}
    ~Runtime(){JPH::UnregisterTypes();delete JPH::Factory::sInstance;JPH::Factory::sInstance=nullptr;}
};
void initialize(){static Runtime runtime;}
struct Broadphase:JPH::BroadPhaseLayerInterface{
    JPH::uint GetNumBroadPhaseLayers()const override{return 2;}
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l)const override{return JPH::BroadPhaseLayer(l);}
#if defined(JPH_EXTERNAL_PROFILE)||defined(JPH_PROFILE_ENABLED)
    const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer l)const override{return l.GetValue()==0?"Static":"Moving";}
#endif
};
struct PairFilter:JPH::ObjectLayerPairFilter{bool ShouldCollide(JPH::ObjectLayer a,JPH::ObjectLayer b)const override{return a!=0||b!=0;}};
struct ObjectFilter:JPH::ObjectVsBroadPhaseLayerFilter{bool ShouldCollide(JPH::ObjectLayer a,JPH::BroadPhaseLayer b)const override{return a!=0||b.GetValue()!=0;}};
struct Filter:JPH::BodyFilter{
    Body self,other;bool sensors;
    Filter(Body a,QueryFilter f):self(a),other(f.ignore),sensors(f.includeSensors){}
    bool ShouldCollide(const JPH::BodyID &id)const override{return id.GetIndexAndSequenceNumber()!=self.value&&id.GetIndexAndSequenceNumber()!=other.value;}
    bool ShouldCollideLocked(const JPH::Body &b)const override{return sensors||!b.IsSensor();}
};
struct SweepCollector:JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>{
    JPH::Vec3 displacement;
    explicit SweepCollector(JPH::Vec3 d):displacement(d){}
    void AddHit(const JPH::ShapeCastResult &hit)override{
        // A touching floor must not block motion parallel to or away from it.
        if(hit.mFraction<=0&&hit.mPenetrationDepth<.002f&&hit.mPenetrationAxis.Dot(displacement)<=1e-7f)return;
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector>::AddHit(hit);
    }
};
}
struct PhysicsWorld::Impl:JPH::ContactListener{
    struct Record{
        BodySettings settings;MotionType motion;JPH::RefConst<JPH::Shape> shape;
        MassProperties mass;BodyState previous;
        V3 force{},torque{},persistentForce{};
        bool moving=false;V3 target{};Quat targetRotation{};float remaining=0;
    };
    Broadphase broad;PairFilter pairs;ObjectFilter objects;WorldSettings config;
    JPH::PhysicsSystem system;JPH::TempAllocatorImplWithMallocFallback temporary;
    JPH::JobSystemThreadPool jobs;std::map<uint32_t,Record> bodies;
    std::mutex mutex;std::vector<Contact> events;double accumulator=0;
    PhysicsStatistics stats;
    explicit Impl(const WorldSettings &c):config(c),temporary(16*1024*1024),
        jobs(JPH::cMaxPhysicsJobs,JPH::cMaxPhysicsBarriers,c.workers?std::min(c.workers,256u):std::clamp(rf::availableHardwareThreads()-1,1u,8u)){
        system.Init(c.maxBodies,0,c.maxBodies*4,c.maxBodies*2,broad,objects,pairs);
        system.SetGravity(vec(c.gravityMps2));system.SetContactListener(this);
    }
    void OnContactAdded(const JPH::Body &a,const JPH::Body &b,const JPH::ContactManifold &m,JPH::ContactSettings &)override{
        std::lock_guard lock(mutex);
        auto p=m.GetWorldSpaceContactPointOn1(0);
        events.push_back({{a.GetID().GetIndexAndSequenceNumber()},{b.GetID().GetIndexAndSequenceNumber()},true,
                          {float(p.GetX()),float(p.GetY()),float(p.GetZ())},vec(m.mWorldSpaceNormal),a.IsSensor()||b.IsSensor()});
    }
    void OnContactRemoved(const JPH::SubShapeIDPair &p)override{
        std::lock_guard lock(mutex);events.push_back({{p.GetBody1ID().GetIndexAndSequenceNumber()},{p.GetBody2ID().GetIndexAndSequenceNumber()},false,{},{},false});
    }
    Record &record(Body b){auto i=bodies.find(b.value);if(i==bodies.end())throw std::invalid_argument("Stale physics body");return i->second;}
    const Record &record(Body b)const{auto i=bodies.find(b.value);if(i==bodies.end())throw std::invalid_argument("Stale physics body");return i->second;}
    JPH::BodyID id(Body b)const{record(b);return JPH::BodyID(b.value);}
    Record &dynamic(Body b){auto &r=record(b);if(r.motion!=MotionType::Dynamic)throw std::invalid_argument("Force/velocity operation requires a dynamic body");return r;}
    BodyState state(Body b)const{
        auto id=this->id(b);const auto &api=system.GetBodyInterface();auto p=api.GetPosition(id);auto q=api.GetRotation(id);
        return {{float(p.GetX()),float(p.GetY()),float(p.GetZ())},vec(api.GetLinearVelocity(id)),
                {q.GetX(),q.GetY(),q.GetZ(),q.GetW()},api.IsActive(id),vec(api.GetAngularVelocity(id)),record(b).motion};
    }
    Body create(const BodySettings &s,JPH::RefConst<JPH::Shape> shape){
        vectorCheck(s.position);Quat q=rotationCheck(s.rotation);
        if(s.massMode!=MassMode::Density&&s.massMode!=MassMode::ExplicitKilograms)throw std::invalid_argument("Invalid mass mode");
        positive(s.massMode==MassMode::Density?s.densityKgPerM3:s.mass);
        for(float v:{s.friction,s.linearDampingPerSecond,s.angularDampingPerSecond,s.dragCoefficient,s.dragAreaM2})
            if(!std::isfinite(v)||v<0)throw std::invalid_argument("Negative/nonfinite physical material coefficient");
        if(!std::isfinite(s.restitution)||s.restitution<0||s.restitution>1||!std::isfinite(s.gravityScale))throw std::invalid_argument("Invalid restitution or gravity scale");
        positive(s.maxLinearSpeedMps);positive(s.maxAngularSpeedRadPerSecond);
        MotionType motion=s.motion==MotionType::Legacy?(s.dynamic?MotionType::Dynamic:MotionType::Static):s.motion;
        if(motion<MotionType::Static||motion>MotionType::Dynamic)throw std::invalid_argument("Invalid body motion type");
        double volume=shape->GetVolume(),mass=s.massMode==MassMode::Density?volume*s.densityKgPerM3:s.mass;
        positive(mass);if(!std::isfinite(float(mass)))throw std::invalid_argument("Mass exceeds solver range");
        auto props=shape->GetMassProperties();props.ScaleToMass(float(mass));
        V3 inertia{props.mInertia(0,0),props.mInertia(1,1),props.mInertia(2,2)};
        vectorCheck(inertia);
        JPH::BodyCreationSettings c(shape,JPH::RVec3(s.position.x,s.position.y,s.position.z),quat(q),mode(motion),motion==MotionType::Static?0:1);
        c.mFriction=s.friction;c.mRestitution=s.restitution;c.mIsSensor=s.sensor;c.mUserData=s.userData;
        c.mMotionQuality=s.continuous?JPH::EMotionQuality::LinearCast:JPH::EMotionQuality::Discrete;
        c.mAllowDynamicOrKinematic=true;
        c.mOverrideMassProperties=JPH::EOverrideMassProperties::MassAndInertiaProvided;c.mMassPropertiesOverride=props;
        c.mLinearDamping=c.mAngularDamping=0; // exponential damping below, in inverse seconds
        c.mGravityFactor=s.gravityScale;c.mMaxLinearVelocity=s.maxLinearSpeedMps;c.mMaxAngularVelocity=s.maxAngularSpeedRadPerSecond;
        auto id=system.GetBodyInterface().CreateAndAddBody(c,motion==MotionType::Static?JPH::EActivation::DontActivate:JPH::EActivation::Activate);
        if(id.IsInvalid())throw std::runtime_error("Physics body capacity exhausted");
        Body b{id.GetIndexAndSequenceNumber()};
        Record r;r.settings=s;r.motion=motion;r.shape=shape;r.mass={mass,volume,inertia,motion==MotionType::Dynamic?1/mass:0};
        bodies.emplace(b.value,std::move(r));record(b).previous=state(b);stats.bodies=bodies.size();return b;
    }
    std::optional<RayHit> cast(Body b,V3 delta,QueryFilter filter,float inflation=0)const{
        auto id=this->id(b);auto &r=record(b);const auto &api=system.GetBodyInterface();
        JPH::RShapeCast cast(r.shape,JPH::Vec3::sReplicate(1),api.GetCenterOfMassTransform(id),vec(delta));
        JPH::ShapeCastSettings settings;settings.mReturnDeepestPoint=true;settings.mExtraConvexRadius=inflation;
        SweepCollector collector(vec(delta));Filter bodyFilter(b,filter);
        system.GetNarrowPhaseQuery().CastShape(cast,settings,JPH::RVec3::sZero(),collector,{},{},bodyFilter);
        if(!collector.HadHit())return {};
        auto &h=collector.mHit;
        return RayHit{{h.mBodyID2.GetIndexAndSequenceNumber()},std::clamp(h.mFraction,0.f,1.f),
                      vec(h.mContactPointOn2),vec(-h.mPenetrationAxis.NormalizedOr(JPH::Vec3::sAxisY()))};
    }
    void tick(float h,unsigned collisionSteps){
        auto &api=system.GetBodyInterface();
        for(auto &[key,r]:bodies){
            if(r.motion==MotionType::Static)continue; // no per-tick state/lock work for static scenery
            Body b{key};r.previous=state(b);JPH::BodyID id(key);
            if(r.motion==MotionType::Kinematic){
                if(r.moving)api.MoveKinematic(id,JPH::RVec3(r.target.x,r.target.y,r.target.z),quat(r.targetRotation),std::max(h,r.remaining));
                continue;
            }
            if(r.motion!=MotionType::Dynamic)continue;
            auto velocity=api.GetLinearVelocity(id),angular=api.GetAngularVelocity(id);
            velocity*=float(std::exp(-double(r.settings.linearDampingPerSecond)*h));
            angular*=float(std::exp(-double(r.settings.angularDampingPerSecond)*h));
            // Exact isolated quadratic-drag decay, split from external forces. Unlike
            // explicit force integration this cannot reverse velocity at large Cd*A/m.
            V3 relative=vec(velocity)-config.windMps;
            double drag=.5*config.airDensityKgM3*r.settings.dragCoefficient*r.settings.dragAreaM2;
            if(drag>0) velocity=vec(config.windMps+relative*float(1/(1+drag*rf::length(relative)*h/r.mass.massKg)));
            api.SetLinearAndAngularVelocity(id,velocity,angular);
            V3 force=r.force+r.persistentForce;
            if(rf::dot(force,force)>0||rf::dot(r.torque,r.torque)>0)api.AddForceAndTorque(id,vec(force),vec(r.torque));
        }
        auto error=system.Update(h,collisionSteps,&temporary,&jobs);
        if(error!=JPH::EPhysicsUpdateError::None)throw std::runtime_error("Physics solver budget exhausted");
        for(auto &[key,r]:bodies)if(r.moving){
            r.remaining=std::max(0.f,r.remaining-h);
            if(r.remaining<=1e-7f){r.moving=false;api.SetLinearAndAngularVelocity(JPH::BodyID(key),JPH::Vec3::sZero(),JPH::Vec3::sZero());}
        }
        ++stats.simulationSteps;stats.simulatedSeconds+=h;
    }
};
PhysicsWorld::PhysicsWorld(V3 g,unsigned workers):PhysicsWorld([&]{WorldSettings c;c.gravityMps2=g;c.workers=workers;return c;}()){}
PhysicsWorld::PhysicsWorld(const WorldSettings &c){
    vectorCheck(c.gravityMps2);vectorCheck(c.windMps);
    if(!std::isfinite(c.fixedStepSeconds)||c.fixedStepSeconds<.0005||c.fixedStepSeconds>.1||c.maxStepsPerAdvance<1||c.maxStepsPerAdvance>1024||c.maxBodies<1||c.maxBodies>262144||!std::isfinite(c.airDensityKgM3)||c.airDensityKgM3<0)throw std::invalid_argument("Invalid physics world configuration");
    initialize();impl=std::make_unique<Impl>(c);
}
PhysicsWorld::~PhysicsWorld(){while(!impl->bodies.empty())remove({impl->bodies.begin()->first});}
Body PhysicsWorld::box(const BodySettings &s){vectorCheck(s.halfExtent);positive(s.halfExtent.x);positive(s.halfExtent.y);positive(s.halfExtent.z);return impl->create(s,new JPH::BoxShape(vec(s.halfExtent),std::min(.02f,std::min({s.halfExtent.x,s.halfExtent.y,s.halfExtent.z})*.1f)));}
Body PhysicsWorld::sphere(const BodySettings &s,float r){positive(r);return impl->create(s,new JPH::SphereShape(r));}
Body PhysicsWorld::capsule(const BodySettings &s,float h,float r){positive(h);positive(r);return impl->create(s,new JPH::CapsuleShape(h,r));}
Body PhysicsWorld::cylinder(const BodySettings &s,float h,float r){positive(h);positive(r);return impl->create(s,new JPH::CylinderShape(h,r,std::min(.02f,std::min(h,r)*.1f)));}
void PhysicsWorld::remove(Body b){auto id=impl->id(b);auto &api=impl->system.GetBodyInterface();api.RemoveBody(id);api.DestroyBody(id);impl->bodies.erase(b.value);impl->stats.bodies=impl->bodies.size();}
BodyState PhysicsWorld::state(Body b)const{return impl->state(b);}
BodyState PhysicsWorld::interpolatedState(Body b,float alpha)const{
    if(!std::isfinite(alpha)||alpha<0||alpha>1)throw std::invalid_argument("Invalid interpolation fraction");
    auto current=state(b);const auto &old=impl->record(b).previous;
    current.position=rf::mix(old.position,current.position,alpha);current.rotation=rf::slerp(old.rotation,current.rotation,alpha);return current;
}
MassProperties PhysicsWorld::massProperties(Body b)const{return impl->record(b).mass;}
void PhysicsWorld::impulse(Body b,V3 p){vectorCheck(p);impl->dynamic(b);impl->system.GetBodyInterface().AddImpulse(impl->id(b),vec(p));}
void PhysicsWorld::impulseAt(Body b,V3 p,V3 point){vectorCheck(p);vectorCheck(point);impl->dynamic(b);impl->system.GetBodyInterface().AddImpulse(impl->id(b),vec(p),JPH::RVec3(point.x,point.y,point.z));}
void PhysicsWorld::angularImpulse(Body b,V3 p){vectorCheck(p);impl->dynamic(b);impl->system.GetBodyInterface().AddAngularImpulse(impl->id(b),vec(p));}
void PhysicsWorld::force(Body b,V3 f){vectorCheck(f);auto &r=impl->dynamic(b);vectorCheck(r.force+f);r.force+=f;}
void PhysicsWorld::torque(Body b,V3 f){vectorCheck(f);auto &r=impl->dynamic(b);vectorCheck(r.torque+f);r.torque+=f;}
void PhysicsWorld::forceAt(Body b,V3 f,V3 point){vectorCheck(f);vectorCheck(point);auto &r=impl->dynamic(b);V3 t=rf::cross(point-state(b).position,f);vectorCheck(r.force+f);vectorCheck(r.torque+t);r.force+=f;r.torque+=t;}
void PhysicsWorld::continuousForce(Body b,V3 f){vectorCheck(f);impl->dynamic(b).persistentForce=f;}
void PhysicsWorld::clearForces(Body b){auto &r=impl->dynamic(b);r.force=r.torque=r.persistentForce={};}
void PhysicsWorld::velocity(Body b,V3 v){vectorCheck(v);impl->dynamic(b);impl->system.GetBodyInterface().SetLinearVelocity(impl->id(b),vec(v));}
void PhysicsWorld::angularVelocity(Body b,V3 v){vectorCheck(v);impl->dynamic(b);impl->system.GetBodyInterface().SetAngularVelocity(impl->id(b),vec(v));}
void PhysicsWorld::motionType(Body b,MotionType m){
    if(m<MotionType::Static||m>MotionType::Dynamic)throw std::invalid_argument("Choose an explicit motion type");
    auto &r=impl->record(b);auto id=impl->id(b);auto &api=impl->system.GetBodyInterface();
    auto previous=state(b);
    api.SetMotionType(id,mode(m),m==MotionType::Static?JPH::EActivation::DontActivate:JPH::EActivation::Activate);
    api.SetObjectLayer(id,m==MotionType::Static?0:1);r.motion=m;r.mass.inverseMassPerKg=m==MotionType::Dynamic?1/r.mass.massKg:0;r.moving=false;r.force=r.torque=r.persistentForce={};
    if(m!=MotionType::Static)api.SetLinearAndAngularVelocity(id,m==MotionType::Dynamic?vec(previous.velocity):JPH::Vec3::sZero(),m==MotionType::Dynamic?vec(previous.angularVelocity):JPH::Vec3::sZero());
    r.previous=state(b);
}
void PhysicsWorld::teleport(Body b,V3 p,Quat q,bool clear){vectorCheck(p);q=rotationCheck(q);auto &r=impl->record(b);auto &api=impl->system.GetBodyInterface();api.SetPositionAndRotation(impl->id(b),JPH::RVec3(p.x,p.y,p.z),quat(q),r.motion==MotionType::Static?JPH::EActivation::DontActivate:JPH::EActivation::Activate);r.moving=false;if(clear&&r.motion!=MotionType::Static)api.SetLinearAndAngularVelocity(impl->id(b),JPH::Vec3::sZero(),JPH::Vec3::sZero());r.previous=state(b);}
std::optional<RayHit> PhysicsWorld::sweep(Body b,V3 d,QueryFilter f)const{vectorCheck(d);return impl->cast(b,d,f);}
MovementResult PhysicsWorld::moveKinematic(Body b,V3 target,Quat rotation,float seconds,MoveOptions options){
    vectorCheck(target);rotation=rotationCheck(rotation);positive(seconds);
    if(!std::isfinite(options.skinMetres)||options.skinMetres<0||options.skinMetres>.1f)throw std::invalid_argument("Invalid collision skin");
    auto &r=impl->record(b);if(r.motion!=MotionType::Kinematic)throw std::invalid_argument("Controlled motion requires a kinematic body");
    auto old=state(b);V3 delta=target-old.position;float length=rf::length(delta);
    MovementResult result{target,target,{},rotation};
    if(options.block){
        double dot=std::abs(double(old.rotation.x)*rotation.x+double(old.rotation.y)*rotation.y+double(old.rotation.z)*rotation.z+double(old.rotation.w)*rotation.w);
        float angle=float(2*std::acos(std::clamp(dot,0.,1.)));
        // Inflation bounds every vertex's angular travel, conservatively preventing rotation clipping.
        float inflation=2*impl->record(b).shape->GetLocalBounds().GetExtent().Length()*std::sin(angle*.5f);
        auto hit=impl->cast(b,delta,{options.ignore,false},inflation);
        if(hit){result.blocked=true;result.blocker=hit->body;result.normal=hit->normal;
            result.fraction=std::max(0.f,hit->fraction-(length>1e-6f?options.skinMetres/length:0));
            result.acceptedPosition=old.position+delta*result.fraction;result.acceptedRotation=rf::slerp(old.rotation,rotation,result.fraction);
        }
    }
    r.target=result.acceptedPosition;r.targetRotation=result.acceptedRotation;r.remaining=seconds;r.moving=true;return result;
}
std::optional<RayHit> PhysicsWorld::rayCast(V3 from,V3 to,QueryFilter filter)const{
    vectorCheck(from);vectorCheck(to);if(rf::distance(from,to)<1e-8f)return {};
    JPH::RRayCast ray(JPH::RVec3(from.x,from.y,from.z),vec(to-from));JPH::RayCastResult hit;Filter bodyFilter({},filter);
    if(!impl->system.GetNarrowPhaseQuery().CastRay(ray,hit,{},{},bodyFilter))return {};
    auto p=ray.GetPointOnRay(hit.mFraction);auto shape=impl->system.GetBodyInterface().GetTransformedShape(hit.mBodyID);
    return RayHit{{hit.mBodyID.GetIndexAndSequenceNumber()},hit.mFraction,from+(to-from)*hit.mFraction,vec(shape.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,p))};
}
void PhysicsWorld::gravity(V3 g){vectorCheck(g);impl->config.gravityMps2=g;impl->system.SetGravity(vec(g));for(auto &[id,r]:impl->bodies)if(r.motion==MotionType::Dynamic)impl->system.GetBodyInterface().ActivateBody(JPH::BodyID(id));}
V3 PhysicsWorld::gravity()const{return impl->config.gravityMps2;}
double PhysicsWorld::kineticEnergyJoules(Body b)const{
    auto s=state(b);auto p=massProperties(b);auto q=rf::compose({},s.rotation);V3 w=rf::transformVector(rf::inverse(q),s.angularVelocity);
    return .5*p.massKg*rf::dot(s.velocity,s.velocity)+.5*(double(p.inertiaDiagonalKgM2.x)*w.x*w.x+double(p.inertiaDiagonalKgM2.y)*w.y*w.y+double(p.inertiaDiagonalKgM2.z)*w.z*w.z);
}
double PhysicsWorld::potentialEnergyJoules(Body b)const{return -massProperties(b).massKg*rf::dot(gravity(),state(b).position)*impl->record(b).settings.gravityScale;}
void PhysicsWorld::step(float seconds,unsigned collisionSteps){
    if(!std::isfinite(seconds)||seconds<0||seconds>.25f||collisionSteps<1||collisionSteps>8)throw std::invalid_argument("Invalid physics interval");
    if(seconds==0)return;
    unsigned count=unsigned(std::ceil(double(seconds)/impl->config.fixedStepSeconds-1e-6));count=std::max(1u,count);
    for(unsigned i=0;i<count;++i)impl->tick(seconds/count,collisionSteps);
    for(auto &[id,r]:impl->bodies)r.force=r.torque={};
}
AdvanceResult PhysicsWorld::advance(double seconds,const std::function<void(PhysicsWorld &,float)> &beforeTick){
    if(!std::isfinite(seconds)||seconds<0||!std::isfinite(impl->accumulator+seconds))throw std::invalid_argument("Invalid physics frame duration");
    impl->accumulator+=seconds;AdvanceResult result;double h=impl->config.fixedStepSeconds;
    while(impl->accumulator+1e-12>=h&&result.steps<impl->config.maxStepsPerAdvance){
        if(beforeTick)beforeTick(*this,float(h));step(float(h));impl->accumulator=std::max(0.,impl->accumulator-h);++result.steps;
    }
    result.simulatedSeconds=result.steps*h;result.pendingSeconds=impl->accumulator;
    result.interpolationAlpha=float(std::clamp(impl->accumulator/h,0.,1.));result.behind=impl->accumulator>=h;return result;
}
void PhysicsWorld::resetClock(){impl->accumulator=0;}
PhysicsStatistics PhysicsWorld::statistics()const{return impl->stats;}
std::vector<Contact> PhysicsWorld::contacts(){std::lock_guard lock(impl->mutex);std::vector<Contact> result;result.swap(impl->events);return result;}
const char *PhysicsWorld::backend(){return "Jolt Physics 5.7.0 / SI rigid-body layer";}
MovementResult KinematicMotor::moveTowards(PhysicsWorld &world,Body body,V3 goal,float seconds,const KinematicMotorSettings &s,MoveOptions options){
    positive(seconds);positive(s.maxSpeedMps);positive(s.accelerationMps2);positive(s.arrivalToleranceMetres);vectorCheck(goal);
    auto current=world.state(body);V3 delta=goal-current.position;float distance=rf::length(delta);
    float speed=distance<=s.arrivalToleranceMetres?0:std::min(s.maxSpeedMps,std::sqrt(2*s.accelerationMps2*distance));
    V3 desired=rf::normalized(delta)*speed,change=desired-velocity_;float maxChange=s.accelerationMps2*seconds;
    velocity_+=rf::length(change)>maxChange?rf::normalized(change)*maxChange:change;
    V3 move=velocity_*seconds;if(rf::length(move)>distance)move=delta;
    auto result=world.moveKinematic(body,current.position+move,current.rotation,seconds,options);
    if(result.blocked)velocity_={};return result;
}
}
