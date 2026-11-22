#include "internal.hpp"
#include "frontier/detail/mesh_topology.hpp"
#include <map>
#include <stdexcept>
namespace rf::gfx {
namespace {
std::vector<Vertex> clustered(std::span<const Vertex> vertices,float cell){
 struct Bin{V3 p{},n{};Color c{0,0,0,0};float u=0,v=0;unsigned count=0;};
 using Key=std::array<int,3>;std::map<Key,unsigned> bins;std::vector<Bin> data;std::vector<unsigned> indices;
 for(auto &v:vertices){
  Key key{int(std::floor(v.p.x/cell)),int(std::floor(v.p.y/cell)),int(std::floor(v.p.z/cell))};
  auto [it,added]=bins.emplace(key,unsigned(data.size()));if(added)data.emplace_back();auto &b=data[it->second];
  b.p=b.p+v.p;b.n=b.n+v.n;b.c.r+=v.c.r;b.c.g+=v.c.g;b.c.b+=v.c.b;b.c.a+=v.c.a;b.u+=v.u;b.v+=v.v;b.count++;indices.push_back(it->second);
 }
 std::vector<Vertex> points;points.reserve(data.size());for(auto &b:data){float k=1.f/b.count;points.push_back({b.p*k,normalized(b.n),{b.c.r*k,b.c.g*k,b.c.b*k,b.c.a*k},b.u*k,b.v*k});}
 std::vector<Vertex> out;out.reserve(vertices.size());
 for(size_t i=0;i+2<indices.size();i+=3){auto a=indices[i],b=indices[i+1],c=indices[i+2];if(a==b||a==c||b==c)continue;
  auto &A=points[a],&B=points[b],&C=points[c];auto normal=cross(B.p-A.p,C.p-A.p);if(length(normal)<1e-10f)continue;
  // Reject a level that folds a face over. Dropping that triangle would open
  // the surface; keeping its reversed winding would expose its culled back.
  if(dot(normal,cross(vertices[i+1].p-vertices[i].p,vertices[i+2].p-vertices[i].p))<=0)return {};
  out.insert(out.end(),{A,B,C});
 }return out;
}
}
MeshId Renderer::createLodMesh(std::span<const Vertex> vertices,float error){
 if(!std::isfinite(error)||error<.25f||error>8)throw std::invalid_argument("Invalid mesh screen error");
 auto id=createMesh(vertices);auto &m=impl->meshes.at(id.value);m.lodPixelError=error;
 if(vertices.size()<3000||m.radius<.001f)return id;
 bool protectClosure=detail::closedSurface(vertices);
 for(float divisor:{48.f,24.f,12.f}){
  float cell=m.radius/divisor;auto lod=clustered(vertices,cell);
  int previous=m.lods.empty()?m.count:m.lods.back().count;
  if(lod.size()<size_t(previous)*.85&&lod.size()>=12&&(!protectClosure||detail::closedSurface(lod))){auto gpu=impl->upload(lod);gpu.lodError=cell*1.733f;m.lods.push_back(std::move(gpu));}
 }
 return id;
}
}
