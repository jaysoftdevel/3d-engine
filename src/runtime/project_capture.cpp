#include "jsd/project_capture.hpp"
#include <fstream>
#include <cstring>
namespace jsd::project {
namespace {
Json c(Color x){return {x.r,x.g,x.b,x.a};}
Mat4 native(const gfx::Transform&t){auto axis=[](V3 a,float v){return Quat{a.x*std::sin(v*.5f),a.y*std::sin(v*.5f),a.z*std::sin(v*.5f),std::cos(v*.5f)};};return rf::compose(t.position)*rf::compose({},axis({0,0,1},t.roll))*rf::compose({},axis({1,0,0},t.pitch))*rf::compose({},axis({0,1,0},t.yaw),t.scale);}
}
struct ProjectCapture::Impl {
 std::filesystem::path output,assets;float time=0;uint64_t anonymous=0;
 struct State {Id id;size_t index;Mat4 matrix;unsigned material;gfx::Material cached;bool seen=false,visible=true;};
 std::map<unsigned,std::string>meshes,textures;std::map<uint64_t,State>states;std::map<unsigned,std::vector<Part>>split;std::map<uint64_t,std::string>hashes;std::map<std::string,unsigned>materials;std::map<uint64_t,unsigned>materialHash;
 std::map<uint64_t,Id>lights;unsigned lightIndex=0;
 explicit Impl(const std::filesystem::path&p):output(std::filesystem::absolute(p)),assets(output.parent_path()/(output.stem().string()+".assets")){std::filesystem::create_directories(assets);}
};
ProjectCapture::ProjectCapture(const std::filesystem::path&p):impl(std::make_unique<Impl>(p)){recordOnly=true;document.name="Captured scene";document.fps=120;document.materials.push_back(Surface{});for(auto name:{"Geometry","Lighting"})document.add(Kind::Empty,name);document.objects.reserve(10000);}
ProjectCapture::~ProjectCapture()=default;
void ProjectCapture::meshCreated(gfx::MeshId id,std::span<const gfx::Vertex>vertices){uint64_t hash=1469598103934665603ull;for(auto b:std::span((const unsigned char*)vertices.data(),vertices.size_bytes()))hash=(hash^b)*1099511628211ull;if(impl->hashes.contains(hash)){impl->meshes[id.value]=impl->hashes[hash];return;}std::string ref="mesh-"+std::to_string(hash);Mesh m;m.smooth=true;std::map<std::array<uint32_t,12>,unsigned>unique;Face face;for(auto&v:vertices){std::array<uint32_t,12>bits;static_assert(sizeof v==sizeof bits);std::memcpy(bits.data(),&v,sizeof bits);auto[it,fresh]=unique.try_emplace(bits,m.vertices.size());if(fresh)m.vertices.push_back({v.p,{v.u,v.v},v.c,v.n});face.vertices.push_back(it->second);if(face.vertices.size()==3){if(face.vertices[0]!=face.vertices[1]&&face.vertices[1]!=face.vertices[2]&&face.vertices[0]!=face.vertices[2])m.faces.push_back(face);face.vertices.clear();}}
 auto path=impl->assets/(ref+".jsdmesh");writeMesh(m,path);document.meshAssets[ref]=path.string();impl->meshes[id.value]=ref;impl->hashes[hash]=ref;}
void ProjectCapture::textureCreated(gfx::TextureId id,int w,int h,std::span<const unsigned char>pixels,bool){uint64_t hash=1469598103934665603ull;for(auto p:pixels)hash=(hash^p)*1099511628211ull;auto path=impl->assets/("image-"+std::to_string(hash)+".png");if(!std::filesystem::exists(path))writePng({w,h,4,{pixels.begin(),pixels.end()}},path);impl->textures[id.value]=path.string();}
std::string ProjectCapture::meshRef(gfx::MeshId id)const{return impl->meshes.at(id.value);}std::string ProjectCapture::texturePath(gfx::TextureId id)const{return id.value?impl->textures.at(id.value):"";}
unsigned ProjectCapture::material(const gfx::Material&m){uint64_t hash=1469598103934665603ull;for(auto b:std::span((const unsigned char*)&m,sizeof m))hash=(hash^b)*1099511628211ull;if(impl->materialHash.contains(hash))return impl->materialHash.at(hash);Surface s;s.color=m.albedo;s.roughness=m.roughness;s.metallic=m.metallic;s.emissive=m.emissive;s.clearcoat=m.clearcoat;s.textureScale=m.textureScale;s.normalScale=m.normalScale;s.doubleSided=m.doubleSided;s.triplanar=m.projection==gfx::TextureProjection::Triplanar;s.alphaMode=int(m.alphaMode);s.albedo=texturePath(m.albedoTexture);s.normal=texturePath(m.normalTexture);s.orm=texturePath(m.ormTexture);s.emissiveMap=texturePath(m.emissiveTexture);s.engine={{"surface",int(m.surface)},{"surfaceSeed",m.surfaceSeed},{"surfaceWorld",m.surfaceWorld},{"wetness",m.wetness},{"dust",m.dust},{"sheen",m.sheen},{"anisotropy",m.anisotropy},{"castShadow",m.castShadow},{"receiveShadow",m.receiveShadow},{"alphaCutoff",m.alphaCutoff},{"clearcoatRoughness",m.clearcoatRoughness},{"macroVariation",m.macroVariation},{"windStrength",m.windStrength},{"occlusionStrength",m.occlusionStrength},{"temporalReactive",m.temporalReactive}};
 Json j={c(s.color),s.roughness,s.metallic,c(s.emissive),s.clearcoat,s.textureScale,s.normalScale,s.doubleSided,s.triplanar,s.alphaMode,s.albedo,s.normal,s.orm,s.emissiveMap,s.engine};auto key=j.dump();if(impl->materials.contains(key))return impl->materialHash[hash]=impl->materials[key];unsigned index=document.materials.size();s.name="Captured material "+std::to_string(index);document.materials.push_back(s);impl->materials[key]=index;impl->materialHash[hash]=index;return index;}
void ProjectCapture::parts(gfx::MeshId id,std::vector<Part>parts){if(!parts.empty())impl->split[id.value]=std::move(parts);}
void ProjectCapture::frame(float time){impl->time=time;document.duration=std::max(document.duration,time);impl->anonymous=0;impl->lightIndex=0;for(auto&[id,state]:impl->states)state.seen=false;}
Id ProjectCapture::sourceObject(uint64_t id)const{auto it=impl->states.find(id);return it==impl->states.end()?0:it->second.id;}
void ProjectCapture::submitted(const gfx::Draw&draw){uint64_t source=draw.objectId?draw.objectId:0x100000000000ull+impl->anonymous++;auto model=draw.modelMatrix.value_or(native(draw.transform));auto it=impl->states.find(source);if(it!=impl->states.end()){auto &state=it->second;state.seen=true;if(state.visible&&!std::memcmp(state.matrix.m.data(),model.m.data(),sizeof(Mat4))&&!std::memcmp(&state.cached,&draw.material,sizeof(gfx::Material)))return;}unsigned mat=material(draw.material);
 if(it==impl->states.end()){
  bool split=impl->split.contains(draw.mesh.value);Id id=document.add(split?Kind::Empty:Kind::Mesh,"Captured object "+std::to_string(source));auto*o=document.find(id);o->parent=1;o->transform=decompose(model);o->components["sourceDrawId"]=source;o->motion.relative=true;o->motion.keys.push_back({impl->time,o->transform,1});o->materialOverride=mat;if(impl->time>0)o->components["visibility"]=Json::array({{0,false},{impl->time,true}});
  if(split){unsigned index=0;for(auto&p:impl->split.at(draw.mesh.value)){Id child=document.add(Kind::Mesh,p.name);auto*c=document.find(child);c->parent=id;c->transform=decompose(p.local);c->meshRef=meshRef(p.mesh);c->sharedMesh=readMesh(document.meshAssets.at(c->meshRef));c->materialOverride=material(p.material);c->components["sourcePart"]=index++;}}
  else{o=document.find(id);o->meshRef=meshRef(draw.mesh);o->sharedMesh=readMesh(document.meshAssets.at(o->meshRef));}
  impl->states[source]={id,size_t(std::find_if(document.objects.begin(),document.objects.end(),[&](auto&o){return o.id==id;})-document.objects.begin()),model,mat,draw.material,true,true};return;
 }
 auto &state=it->second;auto*o=&document.objects[state.index];state.cached=draw.material;state.seen=true;if(!state.visible){o->components["visibility"].push_back({impl->time,true});state.visible=true;}
 if(std::memcmp(state.matrix.m.data(),model.m.data(),sizeof(Mat4))){o->motion.keys.push_back({impl->time,decompose(model),1});state.matrix=model;}
 if(state.material!=mat){if(!o->components.contains("materialKeys"))o->components["materialKeys"]=Json::array({{0,state.material}});o->components["materialKeys"].push_back({impl->time,mat});state.material=mat;}
}
void ProjectCapture::lightSubmitted(const gfx::Light&l){
 uint64_t key=impl->lightIndex++;Id id;
 auto transform=Transform{};transform.position=l.position;
 if(l.type==gfx::LightType::Spot)transform=decompose(rf::inverse(rf::lookAt(l.position,l.position+rf::normalized(l.direction))));
 if(!impl->lights.contains(key)){
  id=document.add(l.type==gfx::LightType::Spot?Kind::SpotLight:Kind::PointLight,"Scene light "+std::to_string(key+1));impl->lights[key]=id;
  auto*o=document.find(id);o->parent=2;o->transform=transform;o->lightColor=l.color;o->lightRange=l.radius;o->lumens=l.intensity;o->innerCone=l.innerCone;o->outerCone=l.outerCone;
  o->components["legacyLight"]=l.falloff==gfx::LightFalloff::Legacy;o->components["photometricCandela"]=l.falloff==gfx::LightFalloff::InverseSquare;
  o->components["castShadow"]=l.castShadow;o->components["sourceRadiusMetres"]=l.sourceRadiusMetres;
  o->components["intensityKeys"]=Json::array({{0,l.intensity}});o->components["colorKeys"]=Json::array({{0,c(l.color)}});
  o->motion.relative=true;o->motion.keys.push_back({impl->time,transform,1});
 }else{
  id=impl->lights[key];auto*o=document.find(id);auto previous=o->motion.keys.back().transform;
  if(rf::distance(previous.position,transform.position)>.000001f||rf::distance(previous.rotationDegrees,transform.rotationDegrees)>.000001f)o->motion.keys.push_back({impl->time,transform,1});
  auto&keys=o->components["intensityKeys"];if(keys.back()[1].get<float>()!=l.intensity)keys.push_back({impl->time,l.intensity});
  auto&colors=o->components["colorKeys"];if(colors.back()[1]!=c(l.color))colors.push_back({impl->time,c(l.color)});
 }
}
void ProjectCapture::endFrame(){for(auto&[source,state]:impl->states)if(!state.seen&&state.visible){auto*o=document.find(state.id);if(!o->components.contains("visibility"))o->components["visibility"]=Json::array({{0,true}});o->components["visibility"].push_back({impl->time,false});state.visible=false;}}
std::string ProjectCapture::copyAsset(const std::filesystem::path&p){auto to=impl->assets/p.filename();if(std::filesystem::weakly_canonical(p)!=std::filesystem::weakly_canonical(to))std::filesystem::copy_file(p,to,std::filesystem::copy_options::overwrite_existing);return to.string();}
void ProjectCapture::finish(){
 for(auto&o:document.objects)if(o.motion.keys.size()>3){auto temp=impl->assets/("clip-"+std::to_string(o.id)+".jsdclip");writeClip(o.motion.keys,temp);o.motion.clipFile=temp.string();o.motion.sharedKeys=readClip(temp);o.motion.keys.clear();}
 std::set<std::string>meshRefs;for(auto&o:document.objects){if(!o.meshRef.empty())meshRefs.insert(o.meshRef);if(o.components.contains("emitter")&&o.components["emitter"].contains("meshRef"))meshRefs.insert(o.components["emitter"]["meshRef"]);}std::erase_if(document.meshAssets,[&](const auto&item){return !meshRefs.contains(item.first);});
 document.projectInfo["source"]="Native scene capture";
 document.projectInfo["textureInventory"]=Json::array();for(auto&[id,path]:impl->textures)document.projectInfo["textureInventory"].push_back({{"file",path}});
 document.save(impl->output);document=Document::load(impl->output);
 std::set<std::string>used;for(auto&[key,path]:document.meshAssets)used.insert(std::filesystem::weakly_canonical(path).string());for(auto&m:document.materials)for(auto path:{m.albedo,m.normal,m.orm,m.emissiveMap})if(!path.empty())used.insert(std::filesystem::weakly_canonical(path).string());if(document.environment.contains("image"))used.insert(std::filesystem::weakly_canonical(document.environment["image"].get<std::string>()).string());
 std::function<void(const Json&)>collect=[&](const Json&j){if(j.is_object())for(auto&[key,v]:j.items()){if(key=="file"&&v.is_string()&&!v.get<std::string>().empty())used.insert(std::filesystem::weakly_canonical(v.get<std::string>()).string());else collect(v);}else if(j.is_array())for(auto&v:j)collect(v);};collect(document.projectInfo);for(auto&o:document.objects){collect(o.components);if(!o.motion.clipFile.empty())used.insert(std::filesystem::weakly_canonical(o.motion.clipFile).string());}for(auto&file:std::filesystem::directory_iterator(impl->assets))if(file.is_regular_file()&&!used.contains(std::filesystem::weakly_canonical(file.path()).string()))std::filesystem::remove(file.path());
}
}
