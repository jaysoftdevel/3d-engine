#pragma once
#include <jsd/camera_collision.hpp>
#include <frontier/detail/mesh_topology.hpp>
template<class Check>void cameraCollisionTests(Check check){
 using namespace rf;using namespace rf::gfx;jsd::CameraCollision wall;
 std::vector<Vertex> vertices={{{0,-2,-2},{1,0,0},{}},{{0,2,-2},{1,0,0},{}},{{0,2,2},{1,0,0},{}},{{0,-2,-2},{1,0,0},{}},{{0,2,2},{1,0,0},{}},{{0,-2,2},{1,0,0},{}}};
 wall.add(vertices,Mat4::identity());wall.build();auto hit=wall.cast({-2,0,0},{4,0,0},.3f);
 check(hit&&std::abs(hit->fraction-.425f)<.0001f&&hit->normal.x<-.99f,"continuous camera sweep stops before a thin two-sided panel");
 auto p=wall.move({-2,0,0},{2,1,0},.3f);check(p.x<-.299f&&std::abs(p.y-1)<.02f,"camera slides along geometry without passing through it");
 check(wall.cast({-2,2.15f,2.15f},{4,0,0},.3f).has_value(),"camera volume catches triangle corners missed by a center ray");
 check(!wall.cast({-2,2.4f,2.4f},{4,0,0},.3f),"camera sweep keeps clear paths unobstructed");
 auto rock=makeDetailedRock(412,16);check(detail::closedSurface(rock),"generated rock has closed, consistently oriented positional seams");
 auto open=rock;open.erase(open.begin(),open.begin()+3);check(!detail::closedSurface(open),"topology validation detects a missing rock face");
}
