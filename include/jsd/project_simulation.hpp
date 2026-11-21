#pragma once
#include "project.hpp"
namespace jsd::project {
class Simulation {
 struct Binding {Body body;V3 center,scale;int mode;};
 std::unique_ptr<PhysicsWorld> physics;
 std::map<Id,Binding> bodies;std::map<Id,Mat4> matrices;
 float elapsed=0;uint64_t revision=0;
 void tick(const Document &,float);
public:
 bool enabled=false;bool blocked=false;
 void reset(const Document &);void seek(const Document &,float);void advance(const Document &,float);
 const std::map<Id,Mat4>&pose()const{return matrices;}
 float time()const{return elapsed;}
 void bake(Document &,std::span<const Id>,float duration,float fps);
};
}
