#include "jsd/project.hpp"
#include <fstream>
#include <numbers>
#include <unordered_map>
namespace jsd::project {
namespace {
constexpr float rad=rf::pi/180;
bool finite(V3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
Json v(V3 p){return {p.x,p.y,p.z};}Json c(Color p){return {p.r,p.g,p.b,p.a};}
V3 rv(const Json &j){if(!j.is_array()||j.size()!=3)throw std::runtime_error("Expected three SI coordinates");return {j[0],j[1],j[2]};}
Color rc(const Json &j){if(!j.is_array()||j.size()!=4)throw std::runtime_error("Expected RGBA color");return {j[0],j[1],j[2],j[3]};}
Json t(const Transform &p){Json j={v(p.position),v(p.rotationDegrees),v(p.scale)};if(p.quaternion)j.push_back({p.quaternion->x,p.quaternion->y,p.quaternion->z,p.quaternion->w});return j;}
Transform rt(const Json &j){Transform t{rv(j.at(0)),rv(j.at(1)),rv(j.at(2))};if(j.size()>3)t.quaternion=Quat{j[3][0],j[3][1],j[3][2],j[3][3]};return t;}
}
Quat rotation(V3 p){
 p=p*(rad*.5f);Quat a{std::sin(p.x),0,0,std::cos(p.x)},b{0,std::sin(p.y),0,std::cos(p.y)},c{0,0,std::sin(p.z),std::cos(p.z)};
 auto mul=[](Quat a,Quat b){return Quat{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};};
 return rf::normalize(mul(c,mul(b,a)));
}
V3 euler(Quat q){q=rf::normalize(q);return {std::atan2(2*(q.w*q.x+q.y*q.z),1-2*(q.x*q.x+q.y*q.y))/rad,std::asin(std::clamp(2*(q.w*q.y-q.z*q.x),-1.f,1.f))/rad,std::atan2(2*(q.w*q.z+q.x*q.y),1-2*(q.y*q.y+q.z*q.z))/rad};}
Quat orientation(const Transform&t){return t.quaternion.value_or(rotation(t.rotationDegrees));}
Mat4 matrix(const Transform &t){return rf::compose(t.position,orientation(t),t.scale);}
Transform decompose(const Mat4 &m){
 Transform t;t.position={m.m[12],m.m[13],m.m[14]};V3 x{m.m[0],m.m[1],m.m[2]},y{m.m[4],m.m[5],m.m[6]},z{m.m[8],m.m[9],m.m[10]};
 t.scale={rf::length(x),rf::length(y),rf::length(z)};if(std::min({t.scale.x,t.scale.y,t.scale.z})<1e-6f)throw std::runtime_error("Transform is singular");
 if(rf::dot(rf::cross(x,y),z)<0){t.scale.x=-t.scale.x;x=x*-1;}
 x=x/std::abs(t.scale.x);y=y/t.scale.y;z=z/t.scale.z;
 if(std::abs(rf::dot(x,y))+std::abs(rf::dot(x,z))+std::abs(rf::dot(y,z))>.003f)throw std::runtime_error("This operation would introduce shear. Apply parent scale first.");
 float trace=x.x+y.y+z.z;Quat q;
 if(trace>0){float s=std::sqrt(trace+1)*2;q={(y.z-z.y)/s,(z.x-x.z)/s,(x.y-y.x)/s,.25f*s};}
 else if(x.x>y.y&&x.x>z.z){float s=std::sqrt(1+x.x-y.y-z.z)*2;q={.25f*s,(y.x+x.y)/s,(z.x+x.z)/s,(y.z-z.y)/s};}
 else if(y.y>z.z){float s=std::sqrt(1+y.y-x.x-z.z)*2;q={(y.x+x.y)/s,.25f*s,(z.y+y.z)/s,(z.x-x.z)/s};}
 else{float s=std::sqrt(1+z.z-x.x-y.y)*2;q={(z.x+x.z)/s,(z.y+y.z)/s,.25f*s,(x.y-y.x)/s};}
 t.quaternion=rf::normalize(q);t.rotationDegrees=euler(q);return t;
}
Transform evaluate(const Object &o,float seconds){
 Transform t=o.transform;const auto &keys=motionKeys(o);
 if(!keys.empty()){
  if(seconds<=keys.front().time)t=keys.front().transform;
  else if(seconds>=keys.back().time)t=keys.back().transform;
  else{auto b=std::upper_bound(keys.begin(),keys.end(),seconds,[](float t,const Key &k){return t<k.time;});const auto &a=*(b-1);float u=(seconds-a.time)/(b->time-a.time);if(a.interpolation==0)u=0;else if(a.interpolation==2)u=u*u*(3-2*u);
   t.position=rf::mix(a.transform.position,b->transform.position,u);t.scale=rf::mix(a.transform.scale,b->transform.scale,u);t.quaternion=rf::slerp(orientation(a.transform),orientation(b->transform),u);t.rotationDegrees=euler(*t.quaternion);}
 }
 auto &m=o.motion;V3 axis=rf::normalized(m.axis);float phase=2*rf::pi*seconds/std::max(.01f,m.period)+m.phase*rad;
 if(m.relative&&!keys.empty())t=decompose(matrix(o.transform)*rf::inverse(matrix(keys.front().transform))*matrix(t));
 if(m.behavior==Behavior::Spin){float angle=m.speedDegrees*seconds*rad*.5f;Quat q{axis.x*std::sin(angle),axis.y*std::sin(angle),axis.z*std::sin(angle),std::cos(angle)},b=orientation(t);Quat result{q.w*b.x+q.x*b.w+q.y*b.z-q.z*b.y,q.w*b.y-q.x*b.z+q.y*b.w+q.z*b.x,q.w*b.z+q.x*b.y-q.y*b.x+q.z*b.w,q.w*b.w-q.x*b.x-q.y*b.y-q.z*b.z};t.quaternion=result;t.rotationDegrees=euler(result);}
 if(m.behavior==Behavior::Oscillate)t.position+=axis*(m.amplitude*std::sin(phase));
 if(m.behavior==Behavior::Orbit){V3 u=rf::normalized(rf::cross(axis,std::abs(axis.y)<.95f?V3{0,1,0}:V3{1,0,0})),v=rf::cross(axis,u);t.position+=(u*std::cos(phase)+v*std::sin(phase))*m.amplitude;}
 return t;
}
Object *Document::find(Id id){auto p=std::find_if(objects.begin(),objects.end(),[&](auto &o){return o.id==id;});return p==objects.end()?nullptr:&*p;}
const Object *Document::find(Id id)const{return const_cast<Document*>(this)->find(id);}
Id Document::add(Kind kind,const std::string &name,Mesh mesh){Object o;o.id=nextId++;o.kind=kind;o.name=name;o.mesh=std::move(mesh);objects.push_back(std::move(o));++revision;return objects.back().id;}
Id Document::duplicate(Id id){if(!find(id))throw std::runtime_error("Cannot duplicate a missing object");std::vector<Object>copies;std::set<Id>group{id};bool more=true;while(more){more=false;for(auto &o:objects)if(group.contains(o.parent)&&group.insert(o.id).second)more=true;}std::map<Id,Id>remap;for(auto &o:objects)if(group.contains(o.id)){copies.push_back(o);remap[o.id]=nextId++;}for(auto o:copies){Id old=o.id;o.id=remap.at(old);o.name+=" copy";if(remap.contains(o.parent))o.parent=remap.at(o.parent);if(old==id)o.transform.position.x+=.5f;objects.push_back(std::move(o));}++revision;return remap.at(id);}
void Document::erase(std::span<const Id> ids){std::set<Id> removed(ids.begin(),ids.end());bool more=true;while(more){more=false;for(auto &o:objects)if(removed.contains(o.parent)&&removed.insert(o.id).second)more=true;}std::erase_if(objects,[&](auto &o){return removed.contains(o.id);});++revision;}
std::map<Id,Mat4> Document::world(float seconds,bool animate)const{
 std::map<Id,Mat4> result;std::set<Id> visiting;std::map<Id,const Object*> index;for(auto &o:objects)index[o.id]=&o;
 std::function<void(Id)> visit=[&](Id id){if(result.contains(id))return;if(!visiting.insert(id).second)throw std::runtime_error("Hierarchy cycle");auto p=index.find(id);if(p==index.end())throw std::runtime_error("Missing object parent");const auto &o=*p->second;Mat4 local=matrix(animate?evaluate(o,seconds):o.transform);if(o.parent){visit(o.parent);local=result.at(o.parent)*local;}result[id]=local;visiting.erase(id);};
 for(auto &o:objects)visit(o.id);return result;
}
void Document::reparent(Id id,Id parent,bool keep){auto *o=find(id);if(!o||id==parent||(parent&&!find(parent)))throw std::runtime_error("Invalid parent");for(Id p=parent;p;p=find(p)->parent)if(p==id)throw std::runtime_error("Cannot parent an object to its descendant");auto worlds=world();Transform next=o->transform;if(keep)next=decompose((parent?rf::inverse(worlds.at(parent)):Mat4::identity())*worlds.at(id));o->parent=parent;o->transform=next;++revision;}
void Document::validate()const{
 if(objects.size()>10000||materials.empty()||materials.size()>4096)throw std::runtime_error("Project object/material limit exceeded");
 if(!std::isfinite(duration)||duration<=0||duration>600||!std::isfinite(fps)||fps<1||fps>240||!finite(gravity)||!finite(sunDirection)||rf::length(sunDirection)<.001f||!std::isfinite(sunIntensity)||sunIntensity<0||!std::isfinite(ambient)||ambient<0)throw std::runtime_error("Invalid scene timing or gravity");
 auto checkTransform=[](const Transform &t){if(!finite(t.position)||!finite(t.rotationDegrees)||!finite(t.scale)||std::min({std::abs(t.scale.x),std::abs(t.scale.y),std::abs(t.scale.z)})<.0001f)throw std::runtime_error("Invalid transform");if(t.quaternion){auto q=*t.quaternion;if(!std::isfinite(q.x+q.y+q.z+q.w)||q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w<1e-8f)throw std::runtime_error("Invalid transform quaternion");}};
 for(auto &m:materials){for(float f:{m.color.r,m.color.g,m.color.b,m.color.a,m.emissive.r,m.emissive.g,m.emissive.b,m.roughness,m.metallic,m.emission,m.clearcoat,m.textureScale,m.normalScale})if(!std::isfinite(f))throw std::runtime_error("Nonfinite material");if(m.roughness<.04f||m.roughness>1||m.metallic<0||m.metallic>1||m.textureScale<=0||m.alphaMode<0||m.alphaMode>3)throw std::runtime_error("Invalid PBR material");}
 std::set<Id> ids;size_t vertices=0;
 for(auto &o:objects){if(!o.id||!ids.insert(o.id).second||o.id>=nextId)throw std::runtime_error("Invalid object ID");checkTransform(o.transform);if(o.mirrorAxis< -1||o.mirrorAxis>2||o.subdivision<0||o.subdivision>3||int(o.kind)<0||int(o.kind)>8)throw std::runtime_error("Invalid object type/modifier");if(o.materialOverride< -1||o.materialOverride>=int(materials.size()))throw std::runtime_error("Invalid shared material override");if(o.kind==Kind::Mesh&&!o.sharedMesh){jsd::project::validate(o.mesh);vertices+=o.mesh.vertices.size();for(auto &f:o.mesh.faces)if(f.material>=materials.size())throw std::runtime_error("Missing mesh material");}
  if(!finite(o.motion.axis)||!std::isfinite(o.motion.period)||o.motion.period<=0||!std::isfinite(o.motion.speedDegrees)||!std::isfinite(o.motion.amplitude)||!std::isfinite(o.motion.phase)||int(o.motion.behavior)<0||int(o.motion.behavior)>3)throw std::runtime_error("Invalid movement behavior");
  float previous=-1;for(auto &k:motionKeys(o)){checkTransform(k.transform);if(!std::isfinite(k.time)||k.time<0||k.time<=previous||k.interpolation<0||k.interpolation>2)throw std::runtime_error("Invalid animation key");previous=k.time;}
  if(!o.components.is_object())throw std::runtime_error("Object components must be a map");
  auto track=[&](const Json&keys,int width,bool boolean=false,bool material=false){if(!keys.is_array()||keys.size()>100000)throw std::runtime_error("Invalid property track");float last=-1;for(auto&k:keys){if(!k.is_array()||k.size()<2||!k[0].is_number())throw std::runtime_error("Invalid property key");float time=k[0];if(!std::isfinite(time)||time<0||time<=last)throw std::runtime_error("Invalid property key time");last=time;if(boolean){if(!k[1].is_boolean())throw std::runtime_error("Invalid visibility key");}else if(material){if(!k[1].is_number_unsigned()&&!k[1].is_number_integer())throw std::runtime_error("Invalid material key");int value=k[1];if(value<0||value>=int(materials.size()))throw std::runtime_error("Missing animated material");}else if(width){if(!k[1].is_array()||k[1].size()!=unsigned(width))throw std::runtime_error("Invalid property vector");for(auto&v:k[1])if(!v.is_number()||!std::isfinite(v.get<float>()))throw std::runtime_error("Invalid property vector value");}else if(!k[1].is_number()||!std::isfinite(k[1].get<float>()))throw std::runtime_error("Invalid property scalar");}};
  for(auto key:{"intensityKeys","fovKeys"})if(o.components.contains(key))track(o.components[key],0);
  if(o.components.contains("visibility"))track(o.components["visibility"],0,true);if(o.components.contains("materialKeys"))track(o.components["materialKeys"],0,false,true);if(o.components.contains("colorKeys"))track(o.components["colorKeys"],4);
  if(o.kind==Kind::Emitter){auto &e=o.components.at("emitter");if(!e.is_object())throw std::runtime_error("Emitter settings must be a map");if(e.value("capacity",4096u)>65536||e.value("capacity",4096u)==0||e.value("lifetime",1.f)<=0||e.value("size",.1f)<=0||e.value("rate",0.f)<0)throw std::runtime_error("Invalid emitter budget");for(auto key:{"velocity","wind","gravity"})if(e.contains(key))rv(e[key]);if(e.contains("velocityKeys"))track(e["velocityKeys"],3);if(e.contains("enabledKeys"))track(e["enabledKeys"],0);if(e.contains("meshRef")&&!meshAssets.contains(e["meshRef"].get<std::string>()))throw std::runtime_error("Missing emitter mesh");}
  if(o.kind==Kind::Audio){auto &a=o.components.at("audio");a.at("file").get<std::string>();if(a.value("gain",1.f)<0)throw std::runtime_error("Invalid audio gain");}
  if(o.kind==Kind::Overlay){auto &a=o.components.at("overlay");auto type=a.value("type",std::string("rect"));if(type!="rect"&&type!="text"&&type!="image"&&type!="fps"&&type!="progress")throw std::runtime_error("Invalid overlay type");if(a.contains("rect")){if(a["rect"].size()!=4)throw std::runtime_error("Invalid overlay rectangle");for(float v:a["rect"])if(!std::isfinite(v))throw std::runtime_error("Nonfinite overlay rectangle");}if(a.contains("color"))rc(a["color"]);if(a.contains("fadeKeys"))track(a["fadeKeys"],0);for(auto key:{"fadeIn","fadeOut"})if(a.contains(key)&&(a[key].size()!=2||a[key][0].get<float>()>=a[key][1].get<float>()))throw std::runtime_error("Invalid overlay fade window");if(type=="image")a.at("file").get<std::string>();}
  const auto &p=o.physics;if(p.mode<0||p.mode>3||p.shape<0||p.shape>3||!std::isfinite(p.mass)||p.mass<=0||!std::isfinite(p.density)||p.density<=0||!std::isfinite(p.friction)||p.friction<0||!std::isfinite(p.restitution)||p.restitution<0||p.restitution>1||!std::isfinite(p.damping)||p.damping<0||!std::isfinite(p.angularDamping)||p.angularDamping<0||!finite(p.velocity)||!finite(p.angularVelocity))throw std::runtime_error("Invalid physical body settings");
  for(float f:{o.lightColor.r,o.lightColor.g,o.lightColor.b})if(!std::isfinite(f)||f<0)throw std::runtime_error("Invalid light color");
  if(!std::isfinite(o.lumens)||o.lumens<0||!std::isfinite(o.lightRange)||o.lightRange<=0||!std::isfinite(o.innerCone)||!std::isfinite(o.outerCone)||o.innerCone<0||o.outerCone<=o.innerCone||o.outerCone>=rf::pi/2||!std::isfinite(o.cameraFov)||o.cameraFov<10||o.cameraFov>150)throw std::runtime_error("Invalid light/camera");if(o.physicalLens)o.lens.validate();
 }
 terrain.validate();if(terrain.enabled&&terrain.material>=materials.size())throw std::runtime_error("Invalid terrain material");
 if(vertices>8000000)throw std::runtime_error("Editable scene exceeds eight million vertices");world();
}
Json Document::json()const{
 Json j={{"format","jsd-studio"},{"version",1},{"name",name},{"nextId",nextId},{"duration",duration},{"fps",fps},{"gravity",v(gravity)},{"sunColor",c(sunColor)},{"skyColor",c(skyColor)},{"sunDirection",v(sunDirection)},{"sunIntensity",sunIntensity},{"ambient",ambient}};
 j["terrain"]=terrain.json();j["environment"]=environment;j["project"]=projectInfo;j["meshLibrary"]=meshAssets;
 j["materials"]=Json::array();for(auto &s:materials)j["materials"].push_back({{"name",s.name},{"color",c(s.color)},{"emissive",c(s.emissive)},{"roughness",s.roughness},{"metallic",s.metallic},{"emission",s.emission},{"clearcoat",s.clearcoat},{"textureScale",s.textureScale},{"normalScale",s.normalScale},{"doubleSided",s.doubleSided},{"triplanar",s.triplanar},{"alphaMode",s.alphaMode},{"albedo",s.albedo},{"normal",s.normal},{"orm",s.orm},{"emissiveMap",s.emissiveMap},{"engine",s.engine}});
 j["objects"]=Json::array();for(auto &o:objects){Json a={{"id",o.id},{"parent",o.parent},{"name",o.name},{"kind",int(o.kind)},{"transform",t(o.transform)},{"visible",o.visible},{"locked",o.locked},{"mirrorAxis",o.mirrorAxis},{"subdivision",o.subdivision},{"smooth",o.mesh.smooth}};
 a["meshRef"]=o.meshRef;a["materialOverride"]=o.materialOverride;a["components"]=o.components;
 a["vertices"]=Json::array();for(auto &p:o.mesh.vertices)a["vertices"].push_back({v(p.position),{p.uv.x,p.uv.y},c(p.color),v(p.normal)});
 a["faces"]=Json::array();for(auto &f:o.mesh.faces)a["faces"].push_back({f.vertices,f.material});
 a["motion"]={{"behavior",int(o.motion.behavior)},{"axis",v(o.motion.axis)},{"speedDegrees",o.motion.speedDegrees},{"amplitude",o.motion.amplitude},{"period",o.motion.period},{"phase",o.motion.phase},{"keys",Json::array()}};for(auto &k:o.motion.keys)a["motion"]["keys"].push_back({k.time,t(k.transform),k.interpolation});
 a["motion"]["relative"]=o.motion.relative;a["motion"]["clipFile"]=o.motion.clipFile;auto &p=o.physics;a["physics"]={{"mode",p.mode},{"shape",p.shape},{"densityMode",p.densityMode},{"continuous",p.continuous},{"mass",p.mass},{"density",p.density},{"friction",p.friction},{"restitution",p.restitution},{"damping",p.damping},{"angularDamping",p.angularDamping},{"velocity",v(p.velocity)},{"angularVelocity",v(p.angularVelocity)}};
 a["light"]={c(o.lightColor),o.lumens,o.lightRange,o.innerCone,o.outerCone};a["camera"]={o.cameraFov,o.physicalLens,o.lens.focalLengthMetres,o.lens.sensorHeightMetres,o.lens.fNumber,o.lens.shutterSeconds,o.lens.iso,o.lens.physicalExposure};j["objects"].push_back(std::move(a));}
 return j;
}
Document Document::fromJson(const Json &j){
 if(j.at("format")!="jsd-studio"||j.at("version")!=1)throw std::runtime_error("Unsupported editor project format");Document d;if(j.contains("terrain"))d.terrain=TerrainData::fromJson(j["terrain"]);d.environment=j.value("environment",Json::object());d.projectInfo=j.value("project",d.projectInfo);d.meshAssets=j.value("meshLibrary",std::map<std::string,std::string>{});d.name=j.at("name");d.nextId=j.at("nextId");d.duration=j.at("duration");d.fps=j.at("fps");d.gravity=rv(j.at("gravity"));d.sunColor=rc(j.at("sunColor"));d.skyColor=rc(j.at("skyColor"));d.sunDirection=rv(j.at("sunDirection"));d.sunIntensity=j.at("sunIntensity");d.ambient=j.at("ambient");
 for(auto &a:j.at("materials")){Surface s;s.name=a.at("name");s.color=rc(a.at("color"));s.emissive=rc(a.at("emissive"));s.roughness=a.at("roughness");s.metallic=a.at("metallic");s.emission=a.at("emission");s.clearcoat=a.at("clearcoat");s.textureScale=a.at("textureScale");s.normalScale=a.at("normalScale");s.doubleSided=a.at("doubleSided");s.triplanar=a.at("triplanar");s.alphaMode=a.at("alphaMode");s.albedo=a.at("albedo");s.normal=a.at("normal");s.orm=a.at("orm");s.emissiveMap=a.at("emissiveMap");s.engine=a.value("engine",Json::object());d.materials.push_back(s);}
 for(auto &a:j.at("objects")){Object o;o.id=a.at("id");o.parent=a.at("parent");o.name=a.at("name");o.kind=Kind(int(a.at("kind")));o.transform=rt(a.at("transform"));o.visible=a.at("visible");o.locked=a.at("locked");o.mirrorAxis=a.at("mirrorAxis");o.subdivision=a.at("subdivision");o.mesh.smooth=a.at("smooth");o.meshRef=a.value("meshRef",std::string{});o.materialOverride=a.value("materialOverride",-1);o.components=a.value("components",Json::object());if(!o.meshRef.empty()){auto path=d.meshAssets.at(o.meshRef);o.sharedMesh=readMesh(path);}
  for(auto &x:a.at("vertices"))o.mesh.vertices.push_back({rv(x.at(0)),{x.at(1).at(0),x.at(1).at(1)},rc(x.at(2)),x.size()>3?rv(x.at(3)):V3{}});for(auto &x:a.at("faces"))o.mesh.faces.push_back({x.at(0).get<std::vector<unsigned>>(),x.at(1)});
  auto &m=a.at("motion");o.motion.relative=m.value("relative",false);o.motion.clipFile=m.value("clipFile",std::string{});if(!o.motion.clipFile.empty())o.motion.sharedKeys=readClip(o.motion.clipFile);o.motion.behavior=Behavior(int(m.at("behavior")));o.motion.axis=rv(m.at("axis"));o.motion.speedDegrees=m.at("speedDegrees");o.motion.amplitude=m.at("amplitude");o.motion.period=m.at("period");o.motion.phase=m.at("phase");for(auto &k:m.at("keys"))o.motion.keys.push_back({k.at(0),rt(k.at(1)),k.at(2)});
  auto &p=a.at("physics");o.physics={p.at("mode"),p.at("shape"),p.at("densityMode"),p.at("continuous"),p.at("mass"),p.at("density"),p.at("friction"),p.at("restitution"),p.at("damping"),p.at("angularDamping"),rv(p.at("velocity")),rv(p.at("angularVelocity"))};
  auto &l=a.at("light");o.lightColor=rc(l.at(0));o.lumens=l.at(1);o.lightRange=l.at(2);o.innerCone=l.at(3);o.outerCone=l.at(4);auto &c=a.at("camera");o.cameraFov=c.at(0);o.physicalLens=c.at(1);o.lens={c.at(2),c.at(3),c.at(4),c.at(5),c.at(6),c.at(7)};d.objects.push_back(std::move(o));
 }
 d.validate();return d;
}
void atomicWrite(const std::filesystem::path &path,std::string_view data){auto parent=std::filesystem::absolute(path).parent_path();std::filesystem::create_directories(parent);auto temporary=path;temporary+=".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());try{std::ofstream f(temporary,std::ios::binary);if(!f||!f.write(data.data(),data.size())||!f.flush())throw std::runtime_error("Cannot write file");f.close();std::filesystem::rename(temporary,path);}catch(...){std::error_code e;std::filesystem::remove(temporary,e);throw;}}
void Document::save(const std::filesystem::path &file){
 validate();auto scene=json();auto output=std::filesystem::absolute(file);auto assets=output.parent_path()/(output.stem().string()+".assets");
 std::map<std::string,std::string>bundled;auto bundle=[&](const std::string &name){if(name.empty())return name;if(bundled.contains(name))return bundled.at(name);std::filesystem::path from(name);if(!std::filesystem::is_regular_file(from))throw std::runtime_error("Missing project resource: "+name);std::ifstream f(from,std::ios::binary);uint64_t h=1469598103934665603ull;char buffer[65536];while(f){f.read(buffer,sizeof buffer);for(std::streamsize i=0;i<f.gcount();i++)h=(h^uint8_t(buffer[i]))*1099511628211ull;}auto to=assets/(std::to_string(h)+from.extension().string());std::filesystem::create_directories(assets);if(std::filesystem::weakly_canonical(from)!=std::filesystem::weakly_canonical(to)&&!std::filesystem::exists(to))std::filesystem::copy_file(from,to);return bundled[name]=std::filesystem::relative(to,output.parent_path()).generic_string();};
 for(auto &m:scene["materials"])for(auto key:{"albedo","normal","orm","emissiveMap"})m[key]=bundle(m[key]);
 for(auto &[key,value]:scene["meshLibrary"].items())value=bundle(value);
 if(scene["environment"].contains("image"))scene["environment"]["image"]=bundle(scene["environment"]["image"]);
 std::function<void(Json&)> resources=[&](Json &j){if(j.is_object())for(auto &[key,value]:j.items()){if(key=="file"&&value.is_string())value=bundle(value);else resources(value);}else if(j.is_array())for(auto &v:j)resources(v);};for(auto &o:scene["objects"]){resources(o["components"]);o["motion"]["clipFile"]=bundle(o["motion"].value("clipFile",std::string{}));}resources(scene["project"]);
 Json result=output.extension()==".jsdscene"?scene:Json{{"format","jsd-engine-project"},{"version",1},{"scene",scene}};atomicWrite(output,result.dump(2));auto rebound=Document::load(output);for(size_t i=0;i<materials.size();i++){materials[i].albedo=rebound.materials[i].albedo;materials[i].normal=rebound.materials[i].normal;materials[i].orm=rebound.materials[i].orm;materials[i].emissiveMap=rebound.materials[i].emissiveMap;}meshAssets=std::move(rebound.meshAssets);environment=std::move(rebound.environment);projectInfo=std::move(rebound.projectInfo);for(size_t i=0;i<objects.size();i++){objects[i].components=std::move(rebound.objects[i].components);objects[i].motion.clipFile=std::move(rebound.objects[i].motion.clipFile);}path=output;
}
Document Document::load(const std::filesystem::path &file){
 if(std::filesystem::file_size(file)>512*1024*1024)throw std::runtime_error("Project manifest exceeds 512 MiB");std::ifstream f(file);Json root;f>>root;Json j=root.value("format","")=="jsd-engine-project"?root.at("scene"):root;
 if(root.value("format","")=="jsd-engine-project"&&root.at("version")!=1)throw std::runtime_error("Unsupported engine project version");auto directory=std::filesystem::absolute(file).parent_path();
 auto resolve=[&](Json &value){std::filesystem::path p=value.get<std::string>();if(!p.empty()&&p.is_relative())value=(directory/p).lexically_normal().string();};for(auto&m:j.at("materials"))for(auto key:{"albedo","normal","orm","emissiveMap"})resolve(m[key]);
 if(j.contains("meshLibrary"))for(auto &[key,value]:j["meshLibrary"].items())resolve(value);
 if(j.contains("environment")&&j["environment"].contains("image"))resolve(j["environment"]["image"]);
 std::function<void(Json&)>resources=[&](Json&j){if(j.is_object())for(auto &[key,value]:j.items()){if(key=="file"&&value.is_string())resolve(value);else resources(value);}else if(j.is_array())for(auto&v:j)resources(v);};for(auto&o:j["objects"]){if(o.contains("components"))resources(o["components"]);if(o["motion"].contains("clipFile")&&!o["motion"]["clipFile"].get<std::string>().empty())resolve(o["motion"]["clipFile"]);}if(j.contains("project"))resources(j["project"]);
 auto d=fromJson(j);d.path=std::filesystem::absolute(file);return d;
}
Document Document::example(){Document d;d.name="Motion and materials";Surface a;a.name="Ceramic / arctic";a.color={.12f,.56f,.65f,1};a.metallic=.28f;a.roughness=.25f;d.materials.push_back(a);a.name="Anodized copper";a.color={.72f,.24f,.08f,1};a.metallic=.8f;a.roughness=.28f;d.materials.push_back(a);a.name="Stage";a.color={.18f,.22f,.28f,1};a.metallic=.1f;a.roughness=.6f;d.materials.push_back(a);
 auto id=d.add(Kind::Mesh,"Ceramic housing",primitive(1));auto *o=d.find(id);o->transform.position={0,1.6f,0};o->transform.scale={2,2,2};o->motion.behavior=Behavior::Spin;o->motion.speedDegrees=24;
 id=d.add(Kind::Mesh,"Copper orbit",primitive(5));o=d.find(id);o->transform.position={0,1.8f,0};o->transform.scale={1.3f,1.3f,1.3f};o->transform.rotationDegrees={60,0,20};o->motion.behavior=Behavior::Orbit;o->motion.amplitude=2.8f;o->motion.period=6;for(auto &f:o->mesh.faces)f.material=1;
 id=d.add(Kind::Mesh,"Stage",primitive(0));o=d.find(id);o->transform.position={0,-.15f,0};o->transform.scale={12,.3f,12};o->physics.mode=1;for(auto &f:o->mesh.faces)f.material=2;
 id=d.add(Kind::PointLight,"Warm key");o=d.find(id);o->transform.position={-3,5,2};o->lumens=2400;
 id=d.add(Kind::PointLight,"Cool rim");o=d.find(id);o->transform.position={3,3,-3};o->lightColor={.3f,.65f,1,1};o->lumens=1800;
 return d;
}
void History::begin(const Document &d,std::string name){if(before.empty()){before=d.json().dump();label=std::move(name);}}
bool History::commit(const Document &d){if(before.empty())return false;auto after=d.json().dump();if(before==after){cancel();return false;}undoStack.push_back(std::move(before));redoStack.clear();size_t total=0;for(auto &s:undoStack)total+=s.size();while(undoStack.size()>64||(total>64*1024*1024&&undoStack.size()>1)){total-=undoStack.front().size();undoStack.erase(undoStack.begin());}cancel();return true;}
void History::cancel(){before.clear();label.clear();}
bool History::undo(Document &d){if(undoStack.empty())return false;auto path=d.path;redoStack.push_back(d.json().dump());auto next=Document::fromJson(Json::parse(undoStack.back()));undoStack.pop_back();next.path=path;next.revision=d.revision+1;d=std::move(next);cancel();return true;}
bool History::redo(Document &d){if(redoStack.empty())return false;auto path=d.path;undoStack.push_back(d.json().dump());auto next=Document::fromJson(Json::parse(redoStack.back()));redoStack.pop_back();next.path=path;next.revision=d.revision+1;d=std::move(next);cancel();return true;}
void History::clear(){undoStack.clear();redoStack.clear();cancel();}
std::optional<Pick> pick(const Document &d,const std::map<Id,Mat4> &world,Ray ray){Pick result;for(auto &o:d.objects){if(!o.visible||o.locked||o.kind!=Kind::Mesh)continue;auto inv=rf::inverse(world.at(o.id));V3 origin=rf::transformPoint(inv,ray.origin),direction=rf::transformVector(inv,ray.direction);const auto &base=geometry(o);if(base.vertices.empty())continue;auto[lo,hi]=bounds(base);float near=0,far=result.distance;for(int axis=0;axis<3;axis++){float org=axis==0?origin.x:axis==1?origin.y:origin.z,dir=axis==0?direction.x:axis==1?direction.y:direction.z,l=axis==0?lo.x:axis==1?lo.y:lo.z,h=axis==0?hi.x:axis==1?hi.y:hi.z;if(o.mirrorAxis==axis){h=std::max(std::abs(l),std::abs(h));l=-h;}if(std::abs(dir)<1e-9f){if(org<l||org>h)far=-1;}else{float a=(l-org)/dir,b=(h-org)/dir;if(a>b)std::swap(a,b);near=std::max(near,a);far=std::min(far,b);}}if(far<near)continue;std::optional<Mesh>modified;if(o.mirrorAxis>=0||o.subdivision)modified=evaluated(o);const Mesh&mesh=modified?*modified:base;for(unsigned fi=0;fi<mesh.faces.size();fi++)for(auto tri:triangulate(mesh,mesh.faces[fi])){auto a=mesh.vertices[tri[0]].position,b=mesh.vertices[tri[1]].position,c=mesh.vertices[tri[2]].position;V3 e1=b-a,e2=c-a,h=rf::cross(direction,e2);float det=rf::dot(e1,h);if(std::abs(det)<1e-8)continue;float u=rf::dot(origin-a,h)/det;if(u<0||u>1)continue;V3 q=rf::cross(origin-a,e1);float v=rf::dot(direction,q)/det;if(v<0||u+v>1)continue;float t=rf::dot(e2,q)/det;if(t>0&&t<result.distance)result={o.id,fi,t,ray.origin+ray.direction*t};}}
 return result.object?std::optional<Pick>(result):std::nullopt;}
}
