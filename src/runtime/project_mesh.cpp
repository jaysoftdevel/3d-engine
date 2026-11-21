#include "jsd/project.hpp"
#include <map>
#include <tuple>
namespace jsd::project {
namespace {
V3 faceNormal(const Mesh &m,const Face &f){V3 n{};for(size_t i=0;i<f.vertices.size();i++){auto a=m.vertices.at(f.vertices[i]).position,b=m.vertices.at(f.vertices[(i+1)%f.vertices.size()]).position;n.x+=(a.y-b.y)*(a.z+b.z);n.y+=(a.z-b.z)*(a.x+b.x);n.z+=(a.x-b.x)*(a.y+b.y);}return rf::normalized(n);}
Vertex midpoint(const Vertex&a,const Vertex&b){return {(a.position+b.position)*.5f,{(a.uv.x+b.uv.x)*.5f,(a.uv.y+b.uv.y)*.5f},{(a.color.r+b.color.r)*.5f,(a.color.g+b.color.g)*.5f,(a.color.b+b.color.b)*.5f,(a.color.a+b.color.a)*.5f}};}
std::set<unsigned> selection(const Mesh&m,std::span<const unsigned> faces){std::set<unsigned>s(faces.begin(),faces.end());for(auto i:s)if(i>=m.faces.size())throw std::runtime_error("Stale face selection");return s;}
}
void validate(const Mesh &m){if(m.vertices.size()>1000000||m.faces.size()>1000000)throw std::runtime_error("Mesh complexity limit exceeded");for(auto &v:m.vertices)for(float f:{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,v.uv.x,v.uv.y,v.color.r,v.color.g,v.color.b,v.color.a})if(!std::isfinite(f))throw std::runtime_error("Mesh contains nonfinite coordinates");for(auto &f:m.faces){if(f.vertices.size()<3||f.vertices.size()>4096)throw std::runtime_error("Invalid polygon size");std::set<unsigned>unique;for(auto i:f.vertices){if(i>=m.vertices.size()||!unique.insert(i).second)throw std::runtime_error("Invalid polygon vertex index");}if(f.vertices.size()>3)(void)triangulate(m,f);}}
std::pair<V3,V3> bounds(const Mesh &m){if(m.vertices.empty())return {{-.5f,-.5f,-.5f},{.5f,.5f,.5f}};V3 lo=m.vertices[0].position,hi=lo;for(auto &v:m.vertices){lo={std::min(lo.x,v.position.x),std::min(lo.y,v.position.y),std::min(lo.z,v.position.z)};hi={std::max(hi.x,v.position.x),std::max(hi.y,v.position.y),std::max(hi.z,v.position.z)};}return {lo,hi};}
std::vector<std::array<unsigned,3>> triangulate(const Mesh&m,const Face&f){
 if(f.vertices.size()==3)return {{f.vertices[0],f.vertices[1],f.vertices[2]}};
 V3 n=faceNormal(m,f);int axis=std::abs(n.x)>std::abs(n.y)?0:1;if(std::abs(n.z)>(axis==0?std::abs(n.x):std::abs(n.y)))axis=2;
 auto project=[&](unsigned i){V3 p=m.vertices[i].position;return axis==0?UV{p.y,p.z}:axis==1?UV{p.x,p.z}:UV{p.x,p.y};};
 auto cross=[](UV a,UV b,UV c){return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);};
 auto indices=f.vertices;float area=0;for(size_t i=0;i<indices.size();i++){auto a=project(indices[i]),b=project(indices[(i+1)%indices.size()]);area+=a.x*b.y-b.x*a.y;}float sign=area>=0?1:-1;
 std::vector<std::array<unsigned,3>>out;
 while(indices.size()>3){bool found=false;for(size_t i=0;i<indices.size();i++){auto a=indices[(i+indices.size()-1)%indices.size()],b=indices[i],c=indices[(i+1)%indices.size()];auto pa=project(a),pb=project(b),pc=project(c);if(sign*cross(pa,pb,pc)<=1e-10f)continue;bool inside=false;for(auto v:indices)if(v!=a&&v!=b&&v!=c){auto p=project(v);if(sign*cross(pa,pb,p)>=-1e-9f&&sign*cross(pb,pc,p)>=-1e-9f&&sign*cross(pc,pa,p)>=-1e-9f){inside=true;break;}}if(inside)continue;out.push_back({a,b,c});indices.erase(indices.begin()+i);found=true;break;}
  if(!found)throw std::runtime_error("Cannot triangulate degenerate/self-intersecting polygon. Repair or triangulate the face.");
 }
 out.push_back({indices[0],indices[1],indices[2]});return out;
}
std::map<unsigned,std::vector<gfx::Vertex>> triangles(const Mesh&m){
 std::vector<V3>normals(m.vertices.size());if(m.smooth)for(auto &f:m.faces){auto n=faceNormal(m,f);for(auto i:f.vertices)normals[i]+=n;}
 std::map<unsigned,std::vector<gfx::Vertex>>out;for(auto &f:m.faces){V3 n=faceNormal(m,f);if(rf::length(n)<.1f)continue;for(auto tri:triangulate(m,f))for(auto i:tri){auto &v=m.vertices[i];out[f.material].push_back({v.position,m.smooth?(rf::length(v.normal)>.1f?rf::normalized(v.normal):rf::normalized(normals[i])):n,v.color,v.uv.x,v.uv.y});}}
 return out;
}
Mesh primitive(int type,unsigned detail){
 Mesh m;detail=std::clamp(detail,8u,96u);
 if(type==0){for(auto p:std::array<V3,8>{{{-.5,-.5,-.5},{.5,-.5,-.5},{.5,.5,-.5},{-.5,.5,-.5},{-.5,-.5,.5},{.5,-.5,.5},{.5,.5,.5},{-.5,.5,.5}}})m.vertices.push_back({p,{p.x+.5f,p.z+.5f}});
  for(auto f:std::array<std::array<unsigned,4>,6>{{{0,3,2,1},{4,5,6,7},{0,4,7,3},{1,2,6,5},{3,7,6,2},{0,1,5,4}}})m.faces.push_back({{f[0],f[1],f[2],f[3]},0});return m;
 }
 if(type==6){m.vertices={{{-.5f,0,-.5f},{0,0}},{{.5f,0,-.5f},{1,0}},{{.5f,0,.5f},{1,1}},{{-.5f,0,.5f},{0,1}}};m.faces={{{0,3,2,1},0}};return m;}
 if(type==4){for(unsigned i=0;i<detail;i++){float a=2*rf::pi*i/detail;m.vertices.push_back({{.5f*std::cos(a),-.5f,.5f*std::sin(a)},{float(i)/detail,0}});}m.vertices.push_back({{0,.5f,0},{.5f,1}});Face base;for(unsigned i=0;i<detail;i++){base.vertices.push_back(i);m.faces.push_back({{i,detail,(i+1)%detail},0});}m.faces.push_back(base);return m;}
 std::vector<gfx::Vertex> v=type==1?gfx::makeBeveledBox(.1f,3):type==2?gfx::makeSphere(detail,detail/2):type==3?gfx::makeCylinder(detail):gfx::makeTorus(.7f,.17f,detail,detail/2);
 std::map<std::tuple<int64_t,int64_t,int64_t,int64_t,int64_t>,unsigned> unique;
 for(size_t i=0;i<v.size();i+=3){Face f;for(size_t k=0;k<3;k++){auto &p=v[i+k];auto key=std::tuple{int64_t(std::llround(p.p.x*1e6)),int64_t(std::llround(p.p.y*1e6)),int64_t(std::llround(p.p.z*1e6)),int64_t(std::llround(p.u*1e6)),int64_t(std::llround(p.v*1e6))};auto [it,inserted]=unique.try_emplace(key,unsigned(m.vertices.size()));if(inserted)m.vertices.push_back({p.p,{p.u,p.v},p.c,p.n});f.vertices.push_back(it->second);}if(std::set<unsigned>(f.vertices.begin(),f.vertices.end()).size()==3)m.faces.push_back(std::move(f));}
 m.smooth=type==2||type==3||type==5;return m;
}
void normals(Mesh&m){m.smooth=!m.smooth;for(auto &v:m.vertices)v.normal={};}void weldNormals(Mesh&m){m.smooth=true;}
void subdivide(Mesh&m){
 if(m.faces.size()>50000||m.vertices.size()>250000)throw std::runtime_error("Subdivision preview limit reached");Mesh result=m;result.faces.clear();std::map<std::pair<unsigned,unsigned>,unsigned>edges;
 for(auto &f:m.faces){Vertex center{};center.color={0,0,0,0};for(auto i:f.vertices){auto &v=m.vertices[i];center.position+=v.position;center.uv.x+=v.uv.x;center.uv.y+=v.uv.y;center.color.r+=v.color.r;center.color.g+=v.color.g;center.color.b+=v.color.b;center.color.a+=v.color.a;}float inv=1.f/f.vertices.size();center.position=center.position*inv;center.uv.x*=inv;center.uv.y*=inv;center.color={center.color.r*inv,center.color.g*inv,center.color.b*inv,center.color.a*inv};unsigned ci=result.vertices.size();result.vertices.push_back(center);
 auto edge=[&](unsigned a,unsigned b){auto key=std::minmax(a,b);auto [it,fresh]=edges.try_emplace({key.first,key.second},unsigned(result.vertices.size()));if(fresh)result.vertices.push_back(midpoint(m.vertices[a],m.vertices[b]));return it->second;};
 for(size_t i=0;i<f.vertices.size();i++)result.faces.push_back({{f.vertices[i],edge(f.vertices[i],f.vertices[(i+1)%f.vertices.size()]),ci,edge(f.vertices[(i+f.vertices.size()-1)%f.vertices.size()],f.vertices[i])},f.material});
 }
 for(auto &v:result.vertices)v.normal={};m=std::move(result);
}
void mirror(Mesh&m,int axis){if(axis<0||axis>2)return;unsigned count=m.vertices.size();auto verts=m.vertices;for(auto v:verts){if(axis==0){v.position.x*=-1;v.normal.x*=-1;}else if(axis==1){v.position.y*=-1;v.normal.y*=-1;}else{v.position.z*=-1;v.normal.z*=-1;}m.vertices.push_back(v);}auto faces=m.faces;for(auto f:faces){for(auto &i:f.vertices)i+=count;std::reverse(f.vertices.begin(),f.vertices.end());m.faces.push_back(std::move(f));}}
Mesh evaluated(const Object&o,int cap){Mesh m=geometry(o);mirror(m,o.mirrorAxis);for(int i=0;i<std::min(o.subdivision,cap);i++)subdivide(m);return m;}
void eraseFaces(Mesh&m,std::span<const unsigned> faces){auto selected=selection(m,faces);std::vector<Face>out;for(unsigned i=0;i<m.faces.size();i++)if(!selected.contains(i))out.push_back(m.faces[i]);m.faces=std::move(out);}
void flipFaces(Mesh&m,std::span<const unsigned> faces){for(auto i:selection(m,faces))std::reverse(m.faces[i].vertices.begin(),m.faces[i].vertices.end());}
void extrude(Mesh&m,std::span<const unsigned> faces,float distance){
 if(!std::isfinite(distance))throw std::runtime_error("Invalid extrusion distance");auto selected=selection(m,faces);if(selected.empty())return;for(auto &v:m.vertices)v.normal={};
 std::map<unsigned,V3>norm;std::map<std::pair<unsigned,unsigned>,int>edges;std::map<std::pair<unsigned,unsigned>,std::pair<unsigned,unsigned>>orientation;
 for(auto i:selected){auto &f=m.faces[i];auto n=faceNormal(m,f);for(auto v:f.vertices)norm[v]+=n;for(size_t k=0;k<f.vertices.size();k++){unsigned a=f.vertices[k],b=f.vertices[(k+1)%f.vertices.size()];auto ordered=std::minmax(a,b);std::pair<unsigned,unsigned>key{ordered.first,ordered.second};edges[key]++;orientation[key]={a,b};}}
 std::map<unsigned,unsigned>copy;for(auto [i,n]:norm){Vertex v=m.vertices[i];v.position+=rf::normalized(n)*distance;copy[i]=m.vertices.size();m.vertices.push_back(v);}
 std::vector<Face>out;for(unsigned i=0;i<m.faces.size();i++){auto f=m.faces[i];if(selected.contains(i))for(auto &v:f.vertices)v=copy.at(v);out.push_back(std::move(f));}
 unsigned material=m.faces[*selected.begin()].material;for(auto [key,count]:edges)if(count==1){auto [a,b]=orientation[key];out.push_back({{a,b,copy[b],copy[a]},material});}m.faces=std::move(out);
}
void inset(Mesh&m,std::span<const unsigned> faces,float amount){
 if(amount<=0||amount>=1||!std::isfinite(amount))throw std::runtime_error("Inset factor must be between zero and one");auto selected=selection(m,faces);std::vector<Face>out;
 for(unsigned i=0;i<m.faces.size();i++){auto f=m.faces[i];if(!selected.contains(i)){out.push_back(f);continue;}V3 center{};for(auto v:f.vertices)center+=m.vertices[v].position;center=center/float(f.vertices.size());std::vector<unsigned>inner;for(auto index:f.vertices){auto v=m.vertices[index];v.position=rf::mix(v.position,center,amount);inner.push_back(m.vertices.size());m.vertices.push_back(v);}for(size_t k=0;k<f.vertices.size();k++)out.push_back({{f.vertices[k],f.vertices[(k+1)%f.vertices.size()],inner[(k+1)%inner.size()],inner[k]},f.material});out.push_back({inner,f.material});}m.faces=std::move(out);
}
void weld(Mesh&m,float tolerance){if(!std::isfinite(tolerance)||tolerance<=0)throw std::runtime_error("Invalid weld tolerance");std::map<std::tuple<int64_t,int64_t,int64_t>,unsigned>map;Mesh out;out.smooth=m.smooth;std::vector<unsigned>remap;
 for(auto &v:m.vertices){auto key=std::tuple{int64_t(std::llround(v.position.x/tolerance)),int64_t(std::llround(v.position.y/tolerance)),int64_t(std::llround(v.position.z/tolerance))};auto [it,fresh]=map.try_emplace(key,unsigned(out.vertices.size()));if(fresh)out.vertices.push_back(v);remap.push_back(it->second);}for(auto f:m.faces){for(auto &i:f.vertices)i=remap.at(i);std::vector<unsigned>unique;for(auto i:f.vertices)if(std::find(unique.begin(),unique.end(),i)==unique.end())unique.push_back(i);if(unique.size()>=3){f.vertices=unique;out.faces.push_back(std::move(f));}}m=std::move(out);
}
void projectUV(Mesh&m,int mode,float scale){if(!std::isfinite(scale)||scale<=0)throw std::runtime_error("Invalid UV scale");if(mode==3){Mesh result;result.smooth=m.smooth;for(auto f:m.faces){auto n=faceNormal(m,f);int axis=std::abs(n.x)>std::abs(n.y)?0:1;if(std::abs(n.z)>(axis==0?std::abs(n.x):std::abs(n.y)))axis=2;for(auto &i:f.vertices){auto v=m.vertices[i];V3 p=v.position;v.uv=axis==0?UV{p.z*scale,p.y*scale}:axis==1?UV{p.x*scale,p.z*scale}:UV{p.x*scale,p.y*scale};i=result.vertices.size();result.vertices.push_back(v);}result.faces.push_back(std::move(f));}m=std::move(result);}
 else for(auto &v:m.vertices){auto p=v.position;v.uv=mode==0?UV{p.x*scale,p.z*scale}:mode==1?UV{p.x*scale,p.y*scale}:UV{(std::atan2(p.z,p.x)/(2*rf::pi)+.5f)*scale,p.y*scale};}}
}
