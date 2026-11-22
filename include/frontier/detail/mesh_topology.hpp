#pragma once
#include "../renderer.hpp"
#include <map>
namespace rf::gfx::detail {
// Weld positional seams for topology checks without changing authored normals/UVs.
inline bool closedSurface(std::span<const Vertex> vertices){
 using Point=std::array<int64_t,3>;using Edge=std::array<Point,2>;
 std::map<Edge,std::pair<unsigned,int>> edges;
 auto point=[](V3 p){return Point{std::llround(p.x*100000.),std::llround(p.y*100000.),std::llround(p.z*100000.)};};
 for(size_t i=0;i+2<vertices.size();i+=3){Point p[]={point(vertices[i].p),point(vertices[i+1].p),point(vertices[i+2].p)};
  if(p[0]==p[1]||p[1]==p[2]||p[2]==p[0])continue;
  for(int j=0;j<3;j++){auto a=p[j],b=p[(j+1)%3];bool forward=a<b;auto &edge=edges[forward?Edge{a,b}:Edge{b,a}];edge.first++;edge.second+=forward?1:-1;}}
 if(edges.empty())return false;for(auto &[edge,count]:edges){(void)edge;if(count.first!=2||count.second!=0)return false;}return true;
}
}
