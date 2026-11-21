#pragma once
#include <array>
#include <filesystem>
#include <functional>
#include <jsd/engine.hpp>
#include <map>
#include "rover_motion.hpp"
#include <jsd/project_capture.hpp>
namespace sundog {
using namespace jsd;
namespace g = jsd::gfx;
struct ModelPart {
  g::MeshId mesh;
  g::Material material;
};
struct Model {
  V3 halfExtent{.5f, .5f, .5f};
  std::vector<ModelPart> parts;
  void draw(g::Renderer &, const Mat4 &root, uint64_t id) const;
};
class Showcase {
  friend struct ShowcasePhysicsProbe;
  g::Renderer &graphics;
  std::filesystem::path assetRoot;
  jsd::project::ProjectCapture *authoring=nullptr;float skyGain=1;std::map<uint64_t,BodySettings> colliderRecords;
  std::vector<jsd::project::Id> emitterNodes;jsd::project::Id cameraNode=0;
  std::vector<g::MeshId> ownedMeshes;
  std::vector<g::TextureId> ownedTextures;
  std::map<std::string, g::Material> materials;
  std::vector<g::Draw> scenery;
  g::MeshId bevel, cube, cylinder, sphere, torus;
  std::array<g::MeshId, 8> rocks;
  Model rover, wheel, rim, cargo, drone, craneHead, radar;
  std::array<Model, 2> scanBoulder, scanRock, scanCliff;
  struct StaticModel {
    const Model *model;
    Mat4 root;
    uint64_t id;
  };
  std::vector<StaticModel> staticModels;
  std::map<uint64_t, g::TextureId> imageCache;
  Model importModel(const std::string &);
  void place(const Model &, V3, V3, float yaw = 0);
  V3 sceneSun{-.48f, .59f, .65f};
  float skyRotation = 0;

  g::EnvironmentId daylight, underground;
  g::TextureId title, mission, credits;
  PhysicsWorld physics;
  RoverMotion roverMotion;
  std::vector<Body> crates;
  Body roverBody{}, liftedCargo{};
  bool cargoReleased = false, roverBlocked = false;
  VfxEmitter dustLeft, dustRight, steam, steam2, sparks, sand, gravel;
  double missionTime = 0;
  float elapsed = 0;
  unsigned cycle = 0;
  uint64_t nextId = 1;
  std::vector<g::Draw> caveScenery;
  std::map<uint64_t,std::string> caveNames;
  struct Rotor {g::Draw draw;V3 position;float speed;};
  std::vector<Rotor> caveRotors;
  std::vector<VfxEmitter> caveEffects;
  std::vector<g::Light> caveLights;
  void buildCave();void drawCave(bool particles);void updateCave(float);
  std::vector<VfxEmitter*> projectEffects();
  void buildMaterials();
  void buildModels();
  void buildScenery();
  void drawRover(float);
  void resetSimulation();
  g::MeshId mesh(std::vector<g::Vertex>);
  g::TextureId texture(const std::string &, bool srgb = true);
  void add(g::MeshId, const g::Material &, V3 position, V3 scale = {1, 1, 1},
           float yaw = 0, float pitch = 0, float roll = 0);

public:
  explicit Showcase(g::Renderer &, const std::filesystem::path &,jsd::project::ProjectCapture *capture=nullptr);
  void projectMetadata();void projectSample(float time);
  ~Showcase();
  Showcase(const Showcase &) = delete;
  Showcase &operator=(const Showcase &) = delete;
  void update(float seconds);
  void reset();
  void render(const g::Camera &, bool hud = true, bool particles = true,
              double fps = -1, const std::function<void()> &overlay = {}, float deltaSeconds = 1.f / 60, float aspect = 16.f/9);
  g::Camera camera() const;
  float time() const { return elapsed; }
  unsigned activeParticles() const;
  static float height(float x, float z);
  static constexpr float Duration = 160;
  static g::Camera cameraAt(float seconds);
  static float caveBlend(float seconds);
  static bool insideCave(V3 position);
  static bool enclosedView(const g::Camera &camera,float aspect=16.f/9);
};
} // namespace sundog
