#pragma once
#include <frontier/renderer.hpp>
namespace jsd {
struct CameraHit {float fraction=1;rf::V3 normal{};};
// Two-sided continuous sphere/triangle tests, accelerated by a static BVH.
// Render geometry can be registered independently of gameplay collision proxies.
class CameraCollision {
    struct Triangle{rf::V3 a,b,c,lo,hi;};
    struct Node{rf::V3 lo,hi;unsigned first=0,count=0,left=0,right=0;};
    std::vector<Triangle> triangles_;
    std::vector<Node> nodes_;
    unsigned partition(unsigned,unsigned);
public:
    void add(std::span<const rf::gfx::Vertex>,const rf::Mat4&);
    void build();
    std::optional<CameraHit> cast(rf::V3 from,rf::V3 displacement,float radius)const;
    using ExtraCast=std::function<std::optional<CameraHit>(rf::V3,rf::V3)>;
    rf::V3 move(rf::V3 from,rf::V3 desired,float radius,const ExtraCast& extra={})const;
};
}
