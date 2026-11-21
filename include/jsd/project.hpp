#pragma once
#include "assets.hpp"
#include "physics.hpp"
#include "particles.hpp"
#include "audio.hpp"
#include "detail/json.hpp"
#include <set>
#include <span>
namespace jsd::project {
using namespace jsd;
using Json=nlohmann::json;using Id=uint64_t;
struct UV {float x=0,y=0;};
struct Vertex {V3 position;UV uv;Color color{};V3 normal{};};
struct Face {std::vector<unsigned> vertices;unsigned material=0;};
struct Mesh {std::vector<Vertex> vertices;std::vector<Face> faces;bool smooth=false;};
struct Transform {V3 position{},rotationDegrees{},scale{1,1,1};std::optional<Quat> quaternion;};
struct Key {float time=0;Transform transform;int interpolation=1;};
enum class Kind {Mesh,PointLight,SpotLight,Camera,Empty,Emitter,Audio,Overlay,Collider};
enum class Behavior {None,Spin,Oscillate,Orbit};
struct Motion {Behavior behavior=Behavior::None;V3 axis{0,1,0};float speedDegrees=45,amplitude=1,period=4,phase=0;std::vector<Key> keys;bool relative=false;std::string clipFile;std::shared_ptr<std::vector<Key>> sharedKeys;};
struct Surface {
 std::string name="Material";Color color{.56f,.65f,.74f,1},emissive{0,0,0,1};
 float roughness=.45f,metallic=0,emission=1,clearcoat=0,textureScale=1,normalScale=1;
 bool doubleSided=false,triplanar=false;int alphaMode=0;
 std::string albedo,normal,orm,emissiveMap;Json engine=Json::object();
};
struct Physical {int mode=0,shape=0;bool densityMode=false,continuous=true;float mass=10,density=500,friction=.6f,restitution=.1f,damping=.05f,angularDamping=.05f;V3 velocity{},angularVelocity{};};
struct Object {
 Id id=0,parent=0;std::string name="Object";Kind kind=Kind::Mesh;Transform transform;
 Mesh mesh;std::string meshRef;std::shared_ptr<Mesh> sharedMesh;int materialOverride=-1;Json components=Json::object();bool visible=true,locked=false;int mirrorAxis=-1,subdivision=0;Motion motion;Physical physics;
 Color lightColor{1,.9f,.75f,1};float lumens=1200,lightRange=20,innerCone=.3f,outerCone=.6f;
 float cameraFov=55;bool physicalLens=false;gfx::CameraOptics lens;
};
uint64_t nextTerrainRevision();
struct TerrainData {
 bool enabled=false;std::string generator="flat";unsigned resolution=129,seed=38004,material=0;float width=320,depth=260,originX=-160,originZ=-80,gain=1,offset=0;
 std::vector<float> heights;std::vector<Color> colors;uint64_t revision=nextTerrainRevision();
 float baseHeight(float x,float z)const;float height(float x,float z)const;Color color(float x,float z)const;
 void resize(unsigned);void brush(float x,float z,float radius,float strength,int mode,float target=0,Color tint={});
 Json json()const;static TerrainData fromJson(const Json&);void validate()const;
};
struct Document {
 TerrainData terrain;Json environment=Json::object(),projectInfo={{"units","SI"},{"loop",true},{"quality","high"},{"entryCamera",0},{"tickRate",120},{"physicsPreview",false}};std::map<std::string,std::string> meshAssets;
 std::string name="Untitled";std::vector<Object> objects;std::vector<Surface> materials;
 float duration=10,fps=30;V3 gravity{0,-9.80665f,0};
 Color sunColor{1,.93f,.82f,1},skyColor{.13f,.18f,.25f,1};V3 sunDirection{-.4f,.8f,-.5f};float sunIntensity=3,ambient=.3f;
 std::filesystem::path path;Id nextId=1;uint64_t revision=1;
 Object *find(Id);const Object *find(Id)const;
 Id add(Kind,const std::string &,Mesh={});
 void erase(std::span<const Id>);Id duplicate(Id);
 void reparent(Id,Id,bool keepWorld=true);void validate()const;
 std::map<Id,Mat4> world(float seconds=0,bool animate=false)const;
 Json json()const;static Document fromJson(const Json &);
 void save(const std::filesystem::path &);static Document load(const std::filesystem::path &);
 static Document example();
};
const std::vector<Key>& motionKeys(const Object&);void editableKeys(Object&);std::shared_ptr<std::vector<Key>> readClip(const std::filesystem::path&);void writeClip(std::span<const Key>,const std::filesystem::path&);
const Mesh& geometry(const Object&);void makeUnique(Object&);bool active(const Document&,const Object&,float);unsigned materialAt(const Object&,unsigned,float);
std::shared_ptr<Mesh> readMesh(const std::filesystem::path&);void writeMesh(const Mesh&,const std::filesystem::path&);
std::vector<gfx::Vertex> meshVertices(const Mesh&);
Quat orientation(const Transform&);Quat rotation(V3 degrees);V3 euler(Quat);Mat4 matrix(const Transform &);Transform decompose(const Mat4 &);
Transform evaluate(const Object &,float seconds);Mesh primitive(int type,unsigned detail=24);
std::vector<std::array<unsigned,3>> triangulate(const Mesh &,const Face &);
std::map<unsigned,std::vector<gfx::Vertex>> triangles(const Mesh &);
std::pair<V3,V3> bounds(const Mesh &);void validate(const Mesh &);
void normals(Mesh &);void subdivide(Mesh &);void mirror(Mesh &,int axis);Mesh evaluated(const Object &,int subdivisionCap=3);
void extrude(Mesh &,std::span<const unsigned> faces,float distance);void inset(Mesh &,std::span<const unsigned>,float amount);
void eraseFaces(Mesh &,std::span<const unsigned>);void flipFaces(Mesh &,std::span<const unsigned>);
void weld(Mesh &,float tolerance=.0001f);void projectUV(Mesh &,int mode,float scale=1);void weldNormals(Mesh &);
void atomicWrite(const std::filesystem::path &,std::string_view);
struct ImportResult {std::vector<Id> ids;std::vector<std::string> warnings;};
ImportResult importModel(Document &,const std::filesystem::path &,const std::filesystem::path &cache);
void exportModel(const Document &,const std::filesystem::path &,std::span<const Id> selected={},bool modifiers=true);
void writePng(const Image &,const std::filesystem::path &);std::vector<unsigned char> png(const Image &);
struct History {
 std::vector<std::string> undoStack,redoStack;std::string before,label;
 void begin(const Document &,std::string);bool commit(const Document &);void cancel();
 bool undo(Document &);bool redo(Document &);void clear();
};
struct Ray {V3 origin,direction;};
struct Pick {Id object=0;unsigned face=0;float distance=1e30f;V3 position;};
std::optional<Pick> pick(const Document &,const std::map<Id,Mat4> &,Ray);
}
