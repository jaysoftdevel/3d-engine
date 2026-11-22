#pragma once
#include "physics.hpp"
#include <frontier/renderer.hpp>
namespace jsd {
// Pinned-root secondary motion for guide hairs, flexible leads and antennae.
// Positions are metres; acceleration and damping use SI units.
class StrandChain {
    std::vector<V3> points_,velocity_;
    std::vector<float> rest_;
public:
    void reset(std::span<const V3> points);
    void advance(float seconds,V3 root,V3 acceleration={0,-9.80665f,0},
                 float dampingPerSecond=3,const PhysicsWorld* collisionWorld=nullptr,
                 Body ignore={},float radiusMetres=.005f);
    std::span<const V3> points()const{return points_;}
};
// Tapered camera-facing ribbons. Use double-sided, anisotropic/sheen material.
std::vector<gfx::Vertex> strandRibbon(std::span<const V3>,V3 eye,float widthMetres,
                                     Color root={.04f,.025f,.015f,1},Color tip={.12f,.07f,.03f,1});
}
