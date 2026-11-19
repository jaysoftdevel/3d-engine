#include "showcase.hpp"
#include <cmath>
#include <cstdio>
#include <iostream>
namespace sundog {
namespace {
float smooth(float a, float b, float t) {
  t = rf::clamp((t - a) / (b - a));
  return t * t * (3 - 2 * t);
}
rf::Quat axis(V3 v, float a) {
  return {v.x * std::sin(a * .5f), v.y * std::sin(a * .5f),
          v.z * std::sin(a * .5f), std::cos(a * .5f)};
}
Mat4 matrix(V3 p, V3 scale = {1, 1, 1}, float yaw = 0, float pitch = 0,
            float roll = 0) {
  return rf::compose(p) * rf::compose({}, axis({0, 0, 1}, roll)) *
         rf::compose({}, axis({0, 1, 0}, yaw)) *
         rf::compose({}, axis({1, 0, 0}, pitch), scale);
}
float random(unsigned n) { return rf::random01(rf::hash(n + 8341)); }
std::vector<g::Vertex> mesa(unsigned seed) {
  std::vector<g::Vertex> out;
  auto point = [&](float a, float t) {
    float ridge = .065f * std::sin(a * 7 + seed) +
                  .04f * std::sin(a * 17 + seed * .7f) +
                  .018f * std::sin(a * 43 + seed * .3f);
    float r = .5f * (1 - .32f * t - .22f * smooth(.78f, 1, t) + ridge +
                     .025f * std::sin(t * 65 + a * 3) +
                     .013f * std::sin(t * 127 + a * 11));
    float y =
        t * (.94f + .045f * std::sin(a * 5 + seed) + .025f * std::sin(a * 13));
    return V3{std::cos(a) * r, y, std::sin(a) * r};
  };
  auto vertex = [&](float a, float t) {
    V3 p = point(a, t), da = (point(a + .001f, t) - point(a - .001f, t)) * 500,
       dy = (point(a, t + .001f) - point(a, t - .001f)) * 500;
    V3 n = rf::normalized(rf::cross(dy, da));
    float c = .76f + .16f * t + .045f * std::sin(t * 85);
    return g::Vertex{p, n, {c, c * .97f, c * .91f, 1}, a / (2 * rf::pi), t};
  };
  for (int y = 0; y < 48; y++)
    for (int x = 0; x < 96; x++) {
      float a = x * 2 * rf::pi / 96, b = (x + 1) * 2 * rf::pi / 96,
            t = y / 48.f, u = (y + 1) / 48.f;
      auto A = vertex(a, t), B = vertex(b, t), C = vertex(b, u),
           D = vertex(a, u);
      out.insert(out.end(), {A, C, B, A, D, C});
    }
  for (int i = 0; i < 96; i++) {
    auto a = vertex(i * 2 * rf::pi / 96, 1),
         b = vertex((i + 1) * 2 * rf::pi / 96, 1);
    a.n = b.n = {0, 1, 0};
    g::Vertex c{{0, .94f, 0}, {0, 1, 0}, {.9f, .87f, .8f, 1}, .5f, .5f};
    out.insert(out.end(), {a, c, b});
  }
  return out;
}
std::vector<g::Vertex> plane(float w, float h) {
  V3 n{0, 0, -1};
  return {
      {{-w / 2, h / 2, 0}, n, {}, 1, 0}, {{w / 2, h / 2, 0}, n, {}, 0, 0},
      {{w / 2, -h / 2, 0}, n, {}, 0, 1}, {{-w / 2, h / 2, 0}, n, {}, 1, 0},
      {{w / 2, -h / 2, 0}, n, {}, 0, 1}, {{-w / 2, -h / 2, 0}, n, {}, 1, 1}};
}
} // namespace
void Model::draw(g::Renderer &r, const Mat4 &root, uint64_t id) const {
  for (size_t i = 0; i < parts.size(); i++) {
    g::Draw draw;
    draw.mesh = parts[i].mesh;
    draw.material = parts[i].material;
    draw.modelMatrix = root;
    draw.objectId = id + i;
    r.submit(draw);
  }
}
g::MeshId Showcase::mesh(std::vector<g::Vertex> v) {
  auto id = graphics.createMesh(v);
  ownedMeshes.push_back(id);
  return id;
}
g::TextureId Showcase::texture(const std::string &name, bool srgb) {
  auto data = loadImage(assetRoot / "showcase" / name);
  auto id = graphics.createTexture(data.width, data.height, data.rgba, srgb);
  ownedTextures.push_back(id);
  return id;
}
Showcase::Showcase(g::Renderer &r, const std::filesystem::path &a)
    : graphics(r), assetRoot(a), dustLeft(VfxSettings::dust()), dustRight([] {
        auto s = VfxSettings::dust();
        s.seed = 849;
        return s;
      }()),
      steam(VfxSettings::steam()), steam2([] {
        auto s = VfxSettings::steam();
        s.seed = 532;
        s.rate = 19;
        s.size = .12f;
        return s;
      }()),
      sparks(VfxSettings::sparks()), sand(VfxSettings::sand()),
      gravel(VfxSettings::gravel()) {
  bevel = mesh(g::makeBeveledBox(.075f, 3));
  cube = mesh(g::makeBox());
  cylinder = mesh(g::makeCylinder(40));
  sphere = mesh(g::makeSphere(32, 20));
  torus = mesh(g::makeTorus(.4f, .11f, 40, 12));
  for (unsigned i = 0; i < rocks.size(); i++)
    rocks[i] = mesh(g::makeDetailedRock(321 + i * 733, i < 4 ? 16 : 7));
  buildMaterials();
  buildModels();
  for (int i = 0; i < 2; i++) {
    std::string suffix = i == 0 ? "near" : "far";
    scanBoulder[i] = importModel("namaqualand_boulder_02-" + suffix + ".gltf");
    scanRock[i] = importModel("rock_07-" + suffix + ".gltf");
    scanCliff[i] = importModel("namaqualand_cliff_01-" + suffix + ".gltf");
  }
  buildScenery();
  auto hdr = loadHdr(assetRoot / "showcase/scans/kloofendal-4k.hdr");
  double logLuma = 0, totalWeight = 0;
  float peak = 0;
  int brightest = 0;
  for (int y = 0; y < hdr.height / 2; y++)
    for (int x = 0; x < hdr.width; x++) {
      int index = (y * hdr.width + x) * 3;
      float l = .2126f * hdr.rgb[index] + .7152f * hdr.rgb[index + 1] +
                .0722f * hdr.rgb[index + 2];
      if (l > peak) {
        peak = l;
        brightest = y * hdr.width + x;
      }
      float w = std::sin((y + .5) * rf::pi / hdr.height);
      if (l > .001f) {
        logLuma += std::log(l) * w;
        totalWeight += w;
      }
    }
  float scale = .35f / std::exp(logLuma / std::max(1., totalWeight));
  for (auto &value : hdr.rgb)
    value *= scale;
  float phi = (brightest % hdr.width + .5f) / hdr.width * 2 * rf::pi - rf::pi,
        theta = (brightest / hdr.width + .5f) / hdr.height * rf::pi;
  V3 sourceSun{std::cos(phi) * std::sin(theta), std::cos(theta),
               std::sin(phi) * std::sin(theta)};
  skyRotation = std::atan2(-.48f, .65f) - std::atan2(sourceSun.x, sourceSun.z);
  sceneSun = rf::rotate(sourceSun, skyRotation);
  for (int y = hdr.height / 2; y < hdr.height; y++)
    for (int x = 0; x < hdr.width; x++) {
      int k = (y * hdr.width + x) * 3;
      hdr.rgb[k] = .13f;
      hdr.rgb[k + 1] = .105f;
      hdr.rgb[k + 2] = .075f;
    }
  daylight = graphics.createEnvironment(hdr.width, hdr.height, hdr.rgb, 3.0f);
  title = texture("title.png", false);
  mission = texture("mission.png", false);
  credits = texture("credits.png", false);
  BodySettings floor;
  floor.dynamic = false;
  floor.position = {0, -.35f, 8};
  floor.halfExtent = {25, .35f, 35};
  physics.box(floor);
  dustLeft.setSurface(height);
  dustRight.setSurface(height);
  sand.setOrigin({-5.9f, 2.9f, 8});
  sand.setVelocity({.35f, -.1f, 0});
  gravel.setOrigin({-5.8f, 2.8f, 8});
  gravel.setVelocity({.15f, -.2f, .1f});
  steam.setOrigin({6.3f, 2.5f, 11});
  steam2.setOrigin({-9.2f, 3.4f, 13});
  sparks.setOrigin({2.4f, 2.4f, 10.3f});
  sparks.setVelocity({1.4f, .8f, -1.1f});
  resetSimulation();
  std::cout << "SUNDOG scene: " << scenery.size() << " objects / "
            << ownedMeshes.size() << " meshes / original PBR assets\n";
}
Showcase::~Showcase() {
  graphics.destroyEnvironment(daylight);
  for (auto m : ownedMeshes)
    graphics.destroyMesh(m);
  for (auto t : ownedTextures)
    graphics.destroyTexture(t);
}
Model Showcase::importModel(const std::string &name) {
  auto asset = loadGltf(assetRoot / "showcase/scans" / name);
  for (auto &m : asset.materials)
    if (m.orm >= 0) {
      auto &pixels = asset.images.at(m.orm).rgba;
      for (size_t i = 1; i < pixels.size(); i += 4)
        pixels[i] = std::max<unsigned char>(190, pixels[i]);
    }
  Model result;
  std::map<std::pair<int, bool>, g::TextureId> local;
  auto image = [&](int index, bool srgb) {
    if (index < 0)
      return g::TextureId{};
    auto key = std::pair{index, srgb};
    if (local.contains(key))
      return local[key];
    auto &im = asset.images.at(index);
    uint64_t hash = 1469598103934665603ull;
    for (auto c : im.rgba)
      hash = (hash ^ c) * 1099511628211ull;
    hash ^= uint64_t(srgb) << 63;
    hash ^= uint64_t(im.width) << 24;
    auto it = imageCache.find(hash);
    if (it != imageCache.end())
      return local[key] = it->second;
    auto id = graphics.createTexture(im.width, im.height, im.rgba, srgb);
    ownedTextures.push_back(id);
    imageCache[hash] = id;
    return local[key] = id;
  };
  std::vector<g::Material> mats;
  for (auto &m : asset.materials) {
    auto v = m.material;
    v.albedoTexture = image(m.albedo, true);
    v.normalTexture = image(m.normal, false);
    v.ormTexture = image(m.orm, false);
    v.emissiveTexture = image(m.emissive, true);
    v.normalScale = .65f;
    v.occlusionStrength = .35f;
    mats.push_back(v);
  }
  auto pose = sampleAnimation(asset, -1, 0);
  std::vector<std::pair<std::vector<g::Vertex>, int>> parts;
  V3 low{1e9, 1e9, 1e9}, high{-1e9, -1e9, -1e9};
  for (size_t i = 0; i < asset.nodes.size(); i++)
    for (auto index : asset.nodes[i].meshes) {
      auto &source = asset.meshes.at(index);
      std::vector<g::Vertex> v;
      g::appendMesh(v, source.vertices, pose[i]);
      for (auto &p : v) {
        low = {std::min(low.x, p.p.x), std::min(low.y, p.p.y),
               std::min(low.z, p.p.z)};
        high = {std::max(high.x, p.p.x), std::max(high.y, p.p.y),
                std::max(high.z, p.p.z)};
      }
      parts.push_back({std::move(v), source.material});
    }
  float extent = std::max({high.x - low.x, high.y - low.y, high.z - low.z});
  V3 center = (low + high) * .5f;
  result.halfExtent = (high - low) / (extent * 2);
  for (auto &[vertices, material] : parts) {
    for (auto &v : vertices)
      v.p = (v.p - center) / extent;
    result.parts.push_back({mesh(std::move(vertices)),
                            material >= 0 ? mats.at(material) : g::Material{}});
  }
  return result;
}
void Showcase::place(const Model &m, V3 p, V3 scale, float yaw) {
  staticModels.push_back({&m, matrix(p, scale, yaw), nextId++ * 100});
}
void Showcase::buildMaterials() {
  for (auto name : {"sand", "rock", "paint", "steel", "ivory", "dark", "rubber",
                    "concrete"}) {
    g::Material m;
    m.albedoTexture = texture(std::string(name) + "-color.png");
    m.normalTexture = texture(std::string(name) + "-normal.png", false);
    m.ormTexture = texture(std::string(name) + "-orm.png", false);
    m.roughness = 1;
    m.metallic = 1;
    m.projection = g::TextureProjection::Triplanar;
    m.textureScale = (std::string(name) == "sand"   ? .42f
                      : std::string(name) == "rock" ? .35f
                                                    : 1.1f);
    m.macroVariation = std::string(name) == "rock" ? .7f : .15f;
    m.normalScale = std::string(name) == "rock" ? .55f : .38f;
    if (std::string(name) == "paint" || std::string(name) == "ivory") {
      m.clearcoat = .25f;
      m.clearcoatRoughness = .24f;
    }
    materials[name] = m;
  }
  auto &ground = materials.at("sand");
  ground.albedoTexture = texture("scans/ground/diff.jpg");
  ground.normalTexture = texture("scans/ground/nor_gl.jpg", false);
  ground.ormTexture = texture("scans/ground/arm.jpg", false);
  ground.textureScale = .4f;
  ground.normalScale = .5f;
  ground.macroVariation = .25f;
  g::Material glass;
  glass.albedo = {.035f, .075f, .09f, 1};
  glass.roughness = .15f;
  glass.metallic = .7f;
  glass.clearcoat = .8f;
  materials["glass"] = glass;
  g::Material cyan;
  cyan.albedo = {.08f, .3f, .35f, 1};
  cyan.emissive = {.3f, 3.2f, 3.8f, 1};
  cyan.roughness = .28f;
  cyan.castShadow = false;
  materials["cyan"] = cyan;
  auto amber = cyan;
  amber.albedo = {.6f, .2f, .025f, 1};
  amber.emissive = {5, 1.7f, .2f, 1};
  materials["amber"] = amber;
  auto white = cyan;
  white.albedo = {.7f, .75f, .65f, 1};
  white.emissive = {4, 3.5f, 2.8f, 1};
  materials["white"] = white;
}
void Showcase::buildModels() {
  using Group = std::map<std::string, std::vector<g::Vertex>>;
  Group data;
  auto put = [&](const char *mat, const std::vector<g::Vertex> &shape, V3 p,
                 V3 size, float yaw = 0, float pitch = 0, float roll = 0) {
    g::appendMesh(data[mat], shape, matrix(p, size, yaw, pitch, roll));
  };
  auto B = g::makeBeveledBox(.07f, 3), C = g::makeCylinder(32),
       S = g::makeSphere(24, 12), T = g::makeTorus(.4f, .11f, 40, 12),
       Q = g::makeBox();
  auto bake = [&](Model &model) {
    for (auto &[name, vertices] : data)
      if (!vertices.empty())
        model.parts.push_back({mesh(std::move(vertices)), materials.at(name)});
    data.clear();
  };
  // Six-wheel expedition vehicle: layered panels, glazed cab, bolts, exposed
  // suspension and cargo bed.
  put("dark", B, {0, 1.05f, 0}, {2.7f, .45f, 4.9f});
  put("ivory", B, {0, 1.47f, 0}, {2.52f, .58f, 4.68f});
  put("paint", B, {0, 2.17f, 1.02f}, {2.16f, 1.16f, 1.92f});
  put("ivory", B, {0, 2.82f, 1.02f}, {2.38f, .16f, 2.12f});
  put("glass", B, {0, 2.27f, 2.02f}, {1.89f, .79f, .09f}, 0, -.12f);
  put("glass", B, {-1.105f, 2.3f, 1.05f}, {.05f, .72f, 1.39f});
  put("glass", B, {1.105f, 2.3f, 1.05f}, {.05f, .72f, 1.39f});
  put("dark", B, {0, 2.31f, 2.085f}, {.075f, .87f, .065f});
  put("steel", B, {0, 1.69f, 2.43f}, {1.86f, .39f, .13f});
  put("dark", B, {0, 1.69f, 2.505f}, {1.55f, .25f, .03f});
  for (int i = -6; i <= 6; i++)
    put("steel", Q, {i * .105f, 1.68f, 2.53f}, {.035f, .24f, .025f});
  put("steel", B, {0, 1.07f, 2.59f}, {2.92f, .2f, .33f});
  put("steel", B, {0, 1.07f, -2.6f}, {2.92f, .22f, .32f});
  put("dark", B, {0, 1.84f, -1.07f}, {2.21f, .14f, 2.52f});
  for (float x : {-1.2f, 1.2f}) {
    put("steel", B, {x, 1.98f, -1.1f}, {.1f, .42f, 2.5f});
    put("white", B, {x * .85f, 1.91f, 2.49f}, {.32f, .18f, .07f});
    put("amber", B, {x, 1.43f, -2.45f}, {.18f, .17f, .05f});
    for (int axle = -1; axle <= 1; axle++) {
      float z = axle * 1.64f;
      put("steel", C, {x, 1.12f, z}, {.13f, 1.0f, .13f}, 0, 0,
          x > 0 ? .5f : -.5f);
      put("paint", B, {x * 1.08f, 1.45f, z}, {.34f, .18f, 1.25f});
    }
    for (int i = 0; i < 6; i++)
      put("steel", C, {x * .96f, 1.51f, -1.9f + i * .65f},
          {.052f, .04f, .052f});
    put("dark", B, {x * .89f, 2.98f, 1.0f}, {.05f, .13f, 1.8f});
    put("steel", C, {x * .92f, 2.58f, .07f}, {.055f, .65f, .055f});
  }
  for (float z : {-.8f, 1.5f})
    put("steel", B, {0, .82f, z}, {3.05f, .12f, .14f});
  put("dark", C, {-.75f, 2.99f, .75f}, {.3f, .08f, .3f});
  put("amber", S, {-.75f, 3.10f, .75f}, {.22f, .2f, .22f});
  put("steel", C, {.85f, 3.22f, .2f}, {.035f, .9f, .035f});
  put("dark", B, {.85f, 3.69f, .2f}, {.18f, .19f, .06f});
  for (float x : {-.78f, .78f})
    for (int z = 0; z < 2; z++)
      put("steel", B, {x, 1.43f, -2.51f}, {.17f, .28f, .06f});
  bake(rover);
  // Wheels have actual individual tread blocks, bead rings, spokes and
  // fastening geometry.
  put("rubber", T, {0, 0, 0}, {1.25f, 1.55f, 1.25f});
  for (int i = 0; i < 40; i++) {
    float a = i * 2 * rf::pi / 40;
    for (float offset : {-.13f, .13f})
      put("rubber", Q, {std::cos(a) * .654f, offset, std::sin(a) * .654f},
          {.12f, .16f, .065f}, -a, 0, 0);
  }
  bake(wheel);
  put("steel", C, {0, 0, 0}, {.81f, .36f, .81f});
  put("dark", C, {0, .19f, 0}, {.61f, .04f, .61f});
  put("paint", C, {0, .23f, 0}, {.28f, .1f, .28f});
  for (int i = 0; i < 8; i++) {
    float a = i * 2 * rf::pi / 8;
    put("steel", B, {std::cos(a) * .25f, .225f, std::sin(a) * .25f},
        {.34f, .06f, .055f}, -a);
    put("steel", C, {std::cos(a) * .17f, .29f, std::sin(a) * .17f},
        {.055f, .035f, .055f});
  }
  put("steel", T, {0, .16f, 0}, {.94f, .35f, .94f});
  bake(rim);
  // A reusable sealed cargo module with reinforcement, latches and inset lid.
  put("dark", B, {0, 0, 0}, {1.14f, .95f, 1.14f});
  put("ivory", B, {0, .02f, 0}, {1.08f, .89f, 1.08f});
  put("paint", B, {0, .49f, 0}, {1.18f, .1f, 1.18f});
  for (float x : {-.44f, .44f}) {
    put("steel", B, {x, 0, 0}, {.09f, 1.0f, 1.18f});
    for (float z : {-.57f, .57f})
      put("dark", B, {x, .13f, z}, {.13f, .2f, .06f});
  }
  put("dark", B, {0, .56f, 0}, {.45f, .08f, .15f});
  bake(cargo);
  // Articulated maintenance drone. It approaches the relay and welds the
  // damaged junction.
  put("dark", S, {0, 0, 0}, {.95f, .48f, .78f});
  put("paint", B, {0, .13f, 0}, {.73f, .22f, .57f});
  put("glass", B, {0, -.04f, -.37f}, {.43f, .18f, .05f});
  put("cyan", S, {0, -.04f, -.40f}, {.2f, .13f, .04f});
  for (float x : {-.75f, .75f})
    for (float z : {-.56f, .56f}) {
      put("steel", B, {x * .6f, 0, z * .6f}, {.9f, .06f, .07f}, x * z);
      put("dark", T, {x, .04f, z}, {.7f, .5f, .7f});
      put("steel", C, {x, .04f, z}, {.1f, .18f, .1f});
    }
  std::array<V3, 4> arm{
      {{0, -.1f, .25f}, {0, -.45f, .5f}, {0, -.8f, .35f}, {0, -.82f, 0}}};
  g::appendMesh(data["steel"], g::makeTube(arm, .045f, 10));
  put("dark", B, {0, -.82f, -.03f}, {.13f, .13f, .25f});
  bake(drone);
  put("steel", B, {0, 0, 0}, {1.1f, .28f, .8f});
  for (float x : {-.45f, .45f}) {
    put("dark", B, {x, -.46f, 0}, {.1f, .7f, .14f});
    put("steel", B, {x, -.83f, 0}, {.25f, .09f, .17f});
  }
  put("amber", B, {0, .17f, -.43f}, {.36f, .07f, .03f});
  bake(craneHead);
  // Dish: concave paraboloid, circular rim and braced feed horn.
  std::vector<g::Vertex> dish;
  auto v = [&](float r, float a) {
    V3 p{std::cos(a) * r, .16f * r * r, std::sin(a) * r},
        n = rf::normalized({-.32f * p.x, 1, -.32f * p.z});
    return g::Vertex{
        p, n, {}, std::cos(a) * r * .25f + .5f, std::sin(a) * r * .25f + .5f};
  };
  for (int i = 0; i < 48; i++)
    for (int j = 0; j < 12; j++) {
      float a = i * 2 * rf::pi / 48, b = (i + 1) * 2 * rf::pi / 48,
            r0 = j * .15f, r1 = (j + 1) * .15f;
      auto A = v(r0, a), B = v(r0, b), C = v(r1, b), D = v(r1, a);
      dish.insert(dish.end(), {A, C, B, A, D, C});
    }
  data["ivory"] = std::move(dish);
  put("steel", T, {0, .53f, 0}, {4.5f, .55f, 4.5f});
  put("dark", C, {0, .7f, 0}, {.18f, 1.4f, .18f});
  put("cyan", S, {0, 1.4f, 0}, {.16f, .2f, .16f});
  bake(radar);
}
void Showcase::add(g::MeshId shape, const g::Material &m, V3 position, V3 scale,
                   float yaw, float pitch, float roll) {
  g::Draw d;
  d.mesh = shape;
  d.material = m;
  d.modelMatrix = matrix(position, scale, yaw, pitch, roll);
  d.objectId = nextId++;
  scenery.push_back(d);
}
float Showcase::height(float x, float z) {
  float local = rf::clamp((std::sqrt(x * x + (z - 3) * (z - 3)) - 23) / 18);
  float base =
      (std::sin(x * .11f + std::sin(z * .07f)) * .4f +
       std::cos(z * .08f) * .5f + std::sin(x * .043f + z * .031f) * 1.4f) *
      local;
  struct Mesa {
    float x, z, r, h;
  };
  static const Mesa mesas[] = {{-47, 40, 25, 29}, {-23, 92, 30, 42},
                               {30, 113, 33, 47}, {72, 64, 28, 38},
                               {-92, 86, 34, 39}, {105, 133, 35, 49},
                               {-69, 142, 34, 48}};
  for (auto m : mesas) {
    float dx = x - m.x, dz = z - m.z;
    float d = std::sqrt(dx * dx + dz * dz);
    float edge =
        m.r * (1 + .045f * std::sin(x * .4f) + .035f * std::sin(z * .3f));
    base += m.h * .12f * (1 - smooth(edge * .58f, edge, d));
  }
  float road = (1 - smooth(2.2f, 6.0f, std::abs(x + 1))) *
               (1 - smooth(45.f, 60.f, std::abs(z + 7)));
  base *= 1 - road;
  float rut = std::exp(-std::pow((x + 2.48f) / .23f, 2)) +
              std::exp(-std::pow((x - .48f) / .23f, 2));
  base -= rut * .028f * road;
  return base;
}
void Showcase::buildScenery() {
  struct Massif {
    V3 p, scale;
    unsigned seed;
  };
  const Massif massifs[] = {
      {{-47, 0, 48}, {44, 32, 37}, 41}, {{-22, 0, 95}, {48, 43, 42}, 77},
      {{30, 0, 113}, {53, 48, 42}, 93}, {{72, 0, 64}, {47, 41, 37}, 114},
      {{-92, 0, 86}, {51, 43, 45}, 53}, {{100, 0, 135}, {62, 55, 48}, 199}};
  for (auto &m : massifs) {
    V3 scale{m.scale.x, m.scale.y * 1.15f, m.scale.z};
    place(scanCliff[0],
          {m.p.x,
           height(m.p.x, m.p.z) + scanCliff[0].halfExtent.y * scale.y - 3,
           m.p.z},
          scale, 0);
  }
  // Service compound: layered modular structure, ventilation, exposed pipework,
  // roof equipment.
  auto dark = materials.at("dark"), steel = materials.at("steel"),
       ivory = materials.at("ivory"), paint = materials.at("paint"),
       concrete = materials.at("concrete");
  add(bevel, concrete, {0, -.07f, 9}, {24, .22f, 22});
  add(bevel, dark, {-7, 3.0f, 17}, {8, 6, 7});
  for (int i = 0; i < 7; i++) {
    add(bevel, ivory, {-10.35f + i * 1.1f, 3.2f, 13.4f}, {1.04f, 5.4f, .16f});
    add(bevel, steel, {-10.5f + i * 1.13f, 5.94f, 17}, {.12f, .18f, 7.4f});
  }
  add(bevel, steel, {-7, 6.16f, 17}, {8.8f, .25f, 7.8f});
  add(bevel, paint, {-7, 1.2f, 13.25f}, {5.2f, .18f, .19f});
  // Door and warning trim, with a recessed inner frame.
  add(bevel, steel, {-7, 2.27f, 13.18f}, {2.5f, 4.4f, .2f});
  add(bevel, dark, {-7, 2.24f, 13.03f}, {2.19f, 4.0f, .12f});
  for (float x : {-7.98f, -6.02f})
    add(bevel, paint, {x, 2.24f, 12.94f}, {.12f, 4, .11f});
  for (int i = 0; i < 3; i++) {
    add(cylinder, steel, {-10 + i * 2.4f, 6.6f, 17.4f}, {1.55f, .8f, 1.55f});
    add(torus, dark, {-10 + i * 2.4f, 7.03f, 17.4f}, {1.9f, .5f, 1.9f});
    for (int j = 0; j < 8; j++)
      add(cube, dark, {-10 + i * 2.4f + j * .12f - .45f, 7.05f, 17.4f},
          {.035f, .035f, .9f});
  }
  // Main gateway: proper flanges and cross-bracing rather than a single block.
  for (float x : {-4.3f, 5.7f}) {
    for (float dz : {-.29f, .29f})
      add(bevel, steel, {x, 4.5f, 11 + dz}, {.92f, 9, .18f});
    add(bevel, paint, {x, 4.5f, 11}, {.28f, 8.9f, .62f});
    add(bevel, concrete, {x, .25f, 11}, {1.6f, .5f, 1.7f});
    for (float y : {1.6f, 3.2f, 4.8f, 6.4f, 8.f})
      add(bevel, steel, {x, y, 10.59f}, {.65f, .12f, .08f});
  }
  add(bevel, steel, {.7f, 9.07f, 11}, {11.6f, .55f, .85f});
  add(bevel, paint, {.7f, 8.5f, 10.92f}, {10.8f, .18f, .75f});
  for (int i = 0; i < 7; i++)
    add(bevel, dark, {-4.2f + i * 1.63f, 8.72f, 10.82f}, {1.85f, .08f, .11f}, 0,
        0, (i % 2 ? 1 : -1) * .24f);
  g::Material sign;
  sign.albedoTexture = texture("relay-sign.png");
  sign.roughness = .8f;
  add(mesh(plane(6.4f, 1.6f)), sign, {.7f, 7.4f, 10.49f});
  // Junction/control cabinets, service ducts and a bank of ribbed compressor
  // cylinders.
  add(bevel, dark, {2.7f, 1.7f, 10.9f}, {2.5f, 3.35f, .78f});
  add(bevel, ivory, {2.7f, 1.75f, 10.43f}, {2.26f, 3.03f, .1f});
  add(bevel, steel, {2.7f, 1.35f, 10.33f}, {1.8f, 1.8f, .12f});
  for (int i = 0; i < 10; i++)
    add(bevel, dark, {2.7f, .65f + i * .14f, 10.25f}, {1.6f, .055f, .04f});
  add(bevel, materials.at("glass"), {2.7f, 2.65f, 10.30f}, {.8f, .42f, .06f});
  for (int i = 0; i < 3; i++) {
    add(cylinder, ivory, {6.6f + i * .85f, 1.25f, 12.5f}, {.65f, 2.5f, .65f});
    for (int j = 0; j < 5; j++)
      add(torus, steel, {6.6f + i * .85f, .35f + j * .43f, 12.5f},
          {.8f, .3f, .8f});
  }
  std::array<V3, 6> pipe{{{6.3f, .3f, 12.5f},
                          {6.3f, 2.7f, 12.5f},
                          {6.3f, 3.1f, 12},
                          {6.3f, 3.1f, 11.4f},
                          {6.3f, 2.9f, 11},
                          {6.3f, 2.5f, 11}}};
  add(mesh(g::makeTube(pipe, .15f, 16)), steel, {});
  for (int k = 0; k < 3; k++) {
    std::array<V3, 4> cable{{{-10.1f + k * .24f, 1, 13},
                             {-10.1f + k * .24f, 4.3f, 13},
                             {-9.8f + k * .24f, 4.6f, 12.9f},
                             {-6.3f, 4.6f + k * .15f, 12.9f}}};
    add(mesh(g::makeTube(cable, .035f, 8)), dark, {});
  }
  // Radar tower and ballast, ladder/railing and a tensioned service cable.
  for (float x : {10.f, 12.1f})
    for (float z : {17.f, 19.1f}) {
      add(bevel, steel, {x, 5.5f, z}, {.17f, 11, .17f});
      add(bevel, concrete, {x, .2f, z}, {.75f, .4f, .75f});
    }
  for (int i = 0; i < 5; i++) {
    add(bevel, paint, {11.05f, 1 + i * 2.1f, 17.0f}, {2.5f, .12f, .12f}, 0, 0,
        i % 2 ? .55f : -.55f);
    add(bevel, steel, {11.05f, 2.1f + i * 2.1f, 18}, {2.6f, .12f, 2.6f});
  }
  for (int i = 0; i < 24; i++)
    add(cylinder, steel, {12.35f, .45f + i * .43f, 18}, {.065f, 1.1f, .065f}, 0,
        0, rf::pi / 2);
  add(cylinder, steel, {11.05f, 12.2f, 18}, {.3f, 2.8f, .3f});
  // Hopper, screened conveyor, exposed rollers and loose material pile.
  add(bevel, steel, {-5.9f, 2.4f, 8}, {2.2f, .18f, 2.1f});
  for (float x : {-6.7f, -5.1f})
    for (float z : {7.2f, 8.8f})
      add(bevel, steel, {x, 1.35f, z}, {.12f, 2.7f, .12f});
  for (float x : {-6.7f, -5.1f})
    add(bevel, paint, {x, 3.25f, 8}, {.15f, 1.5f, 2.05f}, 0, 0,
        x < -5.9f ? -.25f : .25f);
  add(bevel, dark, {-5.9f, 3.25f, 8.85f}, {1.6f, 1.5f, .1f}, 0, .25f);
  add(bevel, dark, {-5.9f, 3.25f, 7.15f}, {1.6f, 1.5f, .1f}, 0, -.25f);
  add(bevel, dark, {-5.8f, .45f, 5.7f}, {1.25f, .22f, 4});
  for (float x : {-6.5f, -5.1f})
    add(bevel, steel, {x, .6f, 5.7f}, {.1f, .35f, 4.3f});
  for (int j = 0; j < 10; j++)
    add(cylinder, steel, {-5.8f, .42f, 3.9f + j * .4f}, {.18f, 1.36f, .18f}, 0,
        0, rf::pi / 2);
  // Crane upright, rail, bracing, hydraulic housings, hanging warning lights.
  add(bevel, steel, {7.3f, 3.5f, 5}, {.55f, 7, .55f});
  add(bevel, paint, {7.3f, 7.15f, 3.7f}, {.6f, .55f, 4});
  add(bevel, steel, {3.4f, 7.25f, 3}, {8.6f, .3f, .4f});
  add(bevel, dark, {3.4f, 7.6f, 3}, {8.6f, .06f, .1f});
  add(cylinder, paint, {7.3f, 1, 5}, {.9f, 1.6f, .9f});
  add(bevel, dark, {7.5f, 1.1f, 4.4f}, {.6f, .65f, .2f});
  // Small detail clusters: cable reels, tanks, pipe elbows, barrels and
  // equipment cases.
  for (int i = 0; i < 8; i++) {
    float x = 8.5f + (i % 3) * 1.05f, z = 9.8f + (i / 3) * 1.15f;
    auto m = i % 3 == 0 ? paint : steel;
    add(cylinder, m, {x, .65f, z}, {.78f, 1.3f, .78f});
    for (float y : {.15f, 1.12f})
      add(torus, dark, {x, y, z}, {.96f, .35f, .96f});
  }
  for (int i = 0; i < 6; i++) {
    float x = -11.5f + i * .5f;
    add(bevel, steel, {x, .25f, 10.5f}, {.34f, .3f, 1.5f});
  }
  // Dense, irregular rock and gravel distribution. Instancing keeps draw counts
  // bounded.
  for (unsigned i = 0; i < 1450; i++) {
    float x = (random(i * 8) - .5f) * 145, z = (random(i * 8 + 1) - .28f) * 145;
    float near = std::sqrt(x * x + (z - 7) * (z - 7));
    if (near < 18 && z > 0)
      continue;
    if (std::abs(x + 1) < 2.1f && z < 5 && z > -40)
      continue;
    float size = .12f + std::pow(random(i * 8 + 2), 4) * 2.1f;
    unsigned variant = size < 1.1f ? 4 + i % 4 : i % 4;
    auto mat = materials.at("rock");
    float tint = .74f + random(i * 8 + 3) * .4f;
    mat.albedo = {tint, tint * .98f, tint * .94f, 1};
    float y = height(x, z);
    if (size > .5f && near < 65) {
      auto &scan = i % 2 ? scanBoulder[1] : scanRock[1];
      V3 scale{size * 1.4f, size * 1.4f, size * 1.4f};
      place(scan, {x, y + scan.halfExtent.y * scale.y - .01f, z}, scale,
            random(i * 8 + 4) * 6.28f);
    } else
      add(rocks[variant], mat, {x, y + size * .25f, z},
          {size * (1 + random(i)), size * .7f,
           size * (.8f + random(i + 7) * .6f)},
          random(i * 8 + 4) * 6.28f, random(i * 8 + 5) * .5f);
  }
  // Eroded shoulder boulders frame the drive without repeating the same
  // low-poly primitive.
  for (unsigned i = 0; i < 38; i++) {
    float side = i % 2 ? 1.f : -1.f, x = side * (17 + random(i * 5) * 21),
          z = -15 + random(i * 5 + 1) * 62;
    float size = 4 + random(i * 5 + 2) * 7;
    auto &scan = i % 2 ? scanBoulder[0] : scanCliff[0];
    V3 scale{size, size, size};
    place(scan, {x, height(x, z) + scan.halfExtent.y * size - .08f, z}, scale,
          random(i * 5 + 4) * 6.28f);
  }
  // Wind-shaped tufts. Every blade is real geometry with roots and curved tips.
  std::vector<g::Vertex> grass;
  for (unsigned i = 0; i < 2400; i++) {
    float x = (random(17000 + i * 5) - .5f) * 90,
          z = (random(17001 + i * 5) - .2f) * 95;
    if (std::abs(x) < 15 && z > 0 && z < 22)
      continue;
    float y = height(x, z), angle = random(i + 33) * 6.28f,
          height = .18f + random(i + 35) * .47f;
    for (int b = 0; b < 3; b++) {
      V3 p{x + std::cos(b * 2.1f) * .09f, y, z + std::sin(b * 2.1f) * .09f},
          R{std::cos(angle + b) * .019f, 0, std::sin(angle + b) * .019f},
          tip = p + V3{.12f, height, .04f};
      V3 n = rf::normalized(rf::cross(R, tip - p));
      Color root{.17f, .14f, .06f, 1}, top{.48f, .39f, .19f, 1};
      grass.insert(grass.end(), {{p - R, n, root, 0, 0},
                                 {p + R, n, root, 1, 0},
                                 {tip, n, top, .5f, 1},
                                 {p - R, n * -1, root, 0, 0},
                                 {tip, n * -1, top, .5f, 1},
                                 {p + R, n * -1, root, 1, 0}});
    }
  }
  g::Material dryGrass;
  dryGrass.roughness = .9f;
  dryGrass.doubleSided = true;
  dryGrass.windStrength = .12f;
  dryGrass.temporalReactive = true;
  add(mesh(std::move(grass)), dryGrass, {});
}
void Showcase::resetSimulation() {
  for (auto b : crates)
    physics.remove(b);
  crates.clear();
  cargoReleased = false;
  for (auto *e :
       {&dustLeft, &dustRight, &steam, &steam2, &sparks, &sand, &gravel})
    e->clear();
}
void Showcase::reset() {
  elapsed = 0;
  cycle = 0;
  resetSimulation();
  graphics.resetHistory();
}
void Showcase::update(float dt) {
  float before = elapsed;
  elapsed += dt;
  if (elapsed >= Duration) {
    elapsed = std::fmod(elapsed, Duration);
    cycle++;
    resetSimulation();
    graphics.resetHistory();
  }
  float motion = 1 - smooth(19, 24, elapsed);
  float z = -35 + 38 * smooth(0, 24, elapsed);
  dustLeft.setOrigin({-2.45f, .22f + height(-2.45f, z - 1.5f), z - 1.5f});
  dustRight.setOrigin({.45f, .22f + height(.45f, z - 1.5f), z - 1.5f});
  dustLeft.setVelocity({-.35f, .4f, -1.5f * motion});
  dustRight.setVelocity({.4f, .35f, -1.6f * motion});
  dustLeft.setEmitting(elapsed < 24);
  dustRight.setEmitting(elapsed < 24);
  steam.setEmitting(std::sin(elapsed * .8f) > -.55f);
  steam2.setEmitting(true);
  sparks.setEmitting(elapsed > 34 && elapsed < 47 &&
                     std::sin(elapsed * 12) > -.3f);
  sand.setEmitting(elapsed > 16 && elapsed < 43);
  gravel.setEmitting(elapsed > 18 && elapsed < 42);
  for (auto *e :
       {&dustLeft, &dustRight, &steam, &steam2, &sparks, &sand, &gravel})
    e->update(dt);
  if (!cargoReleased && elapsed >= 36) {
    BodySettings box;
    box.position = {4.8f, 1.8f, 4.2f};
    box.halfExtent = {.58f, .55f, .58f};
    box.mass = 25;
    box.friction = .8f;
    box.restitution = .06f;
    crates.push_back(physics.box(box));
    cargoReleased = true;
  }
  physics.step(std::max(.0001f, dt), 2);
  (void)physics.contacts();
  (void)before;
}
g::Camera Showcase::camera() const {
  static CameraTrack track({{0, {9.0f, 4.1f, -48}, {-1, 1.9f, -29}, 55},
                            {8, {8.0f, 3.4f, -35}, {-1, 2.0f, -19}, 55},
                            {16, {8.0f, 3.6f, -19}, {-1, 1.9f, -3}, 54},
                            {24, {9.0f, 4.0f, -8}, {0, 2.2f, 5}, 55},
                            {32, {8.0f, 3.6f, -2}, {1, 2.5f, 6}, 58},
                            {40, {-5.6f, 3.2f, 5}, {2.4f, 2.3f, 10.4f}, 55},
                            {48, {-9.5f, 6.4f, -.5f}, {3.5f, 5.3f, 14}, 55},
                            {56, {-15.2f, 9.1f, -16}, {.2f, 4.5f, 10}, 54},
                            {64, {9.0f, 4.1f, -48}, {-1, 1.9f, -29}, 55}});
  return track.sample(elapsed);
}
void Showcase::drawRover(float t) {
  float z = -35 + 38 * smooth(0, 24, t);
  float bob = t < 24 ? std::sin(t * 7) * .025f : 0;
  Mat4 root =
      matrix({-1, height(-1, z) + bob + .035f, z}, {1, 1, 1}, 0,
             -std::atan2(height(-1, z + 1.5f) - height(-1, z - 1.5f), 3.f),
             std::atan2(height(.5f, z) - height(-2.5f, z), 3.f));
  rover.draw(graphics, root, 2000000);
  for (float x : {-1.48f, 1.48f})
    for (float offset : {-1.65f, 0.f, 1.65f}) {
      Mat4 W =
          root *
          matrix({x, .72f, offset}, {1, 1, 1}, 0, 0,
                 x > 0 ? -rf::pi / 2 : rf::pi / 2) *
          matrix({}, {1, 1, 1},
                 (x > 0 ? -1 : 1) * (t < 24 ? (z + 35) / .66f : 38 / .66f));
      wheel.draw(graphics, W,
                 3000000 + uint64_t((x + 2) * 100 + offset * 10) * 100);
      rim.draw(graphics, W,
               4000000 + uint64_t((x + 2) * 100 + offset * 10) * 100);
    }
  for (int i = 0; i < 2; i++) {
    if (i == 0 && t >= 27)
      continue;
    cargo.draw(graphics, root * matrix({i == 0 ? -.59f : .59f, 2.42f, -1.05f}),
               5000000 + i * 100);
  }
}
unsigned Showcase::activeParticles() const {
  return dustLeft.active().size() + dustRight.active().size() +
         steam.active().size() + steam2.active().size() +
         sparks.active().size() + sand.active().size() + gravel.active().size();
}
void Showcase::render(const g::Camera &camera, bool hud, bool particles,
                      double fps) {
  g::Environment env;
  env.sunDirection = sceneSun;
  env.imageRotation = skyRotation;
  env.sunColor = {1, .78f, .51f, 1};
  env.sunIntensity = 4.2f;
  env.skyColor = {.34f, .46f, .62f, 1};
  env.groundColor = {.23f, .17f, .11f, 1};
  env.ambientIntensity = 1.2f;
  env.fogDensity = .1f;
  env.image = daylight;
  env.deltaSeconds = 1.f / 60;
  env.shadowDistance = 43;
  env.atmosphere.enabled = true;
  env.atmosphere.clouds = g::Quality::High;
  env.atmosphere.time = elapsed;
  env.atmosphere.cloudCoverage = .20f;
  env.atmosphere.cloudDensity = .9f;
  graphics.begin(camera, env);
  g::Terrain terrain;
  terrain.height = height;
  terrain.material = materials.at("sand");
  terrain.seed = 38004;
  terrain.revision = 4;
  graphics.terrain(terrain);
  for (auto &draw : scenery)
    graphics.submit(draw);
  for (auto &instance : staticModels)
    instance.model->draw(graphics, instance.root, instance.id + 100000000);
  drawRover(elapsed);
  float lift = smooth(26, 30, elapsed), travel = smooth(30, 35, elapsed);
  V3 hook{rf::mix(-1.f, 4.8f, travel),
          rf::mix(3.0f, 5.7f, lift) - 2.7f * smooth(35, 36, elapsed),
          3.0f + travel * 1.2f};
  if (elapsed > 36)
    hook = {4.8f, 5.7f, 3};
  craneHead.draw(graphics, matrix(hook), 6100000);
  float cableHeight = 7.1f - hook.y;
  g::Material cable = materials.at("dark");
  graphics.primitive(g::Primitive::Cylinder,
                     {{hook.x, hook.y + cableHeight / 2, hook.z},
                      {.035f, cableHeight, .035f},
                      0},
                     cable);
  if (elapsed >= 27 && elapsed < 36)
    cargo.draw(graphics, matrix(hook + V3{0, -1.1f, 0}), 6200000);
  for (auto body : crates) {
    auto p = physics.state(body);
    cargo.draw(graphics, rf::compose(p.position, p.rotation),
               6300000 + body.value * 100);
  }
  float turn = elapsed * .12f;
  radar.draw(graphics, matrix({11.05f, 13.7f, 18}, {1, 1, 1}, turn, .65f),
             6400000);
  V3 dronePosition =
      elapsed < 32
          ? V3{5 + std::sin(elapsed * .5f), 4.5f, 8 + std::cos(elapsed * .5f)}
      : elapsed < 47 ? V3{2.4f, 3.2f + std::sin(elapsed * 2) * .05f, 10.4f}
                     : V3{2.4f + (elapsed - 47) * .55f,
                          3.2f + (elapsed - 47) * .28f, 10.4f};
  drone.draw(
      graphics,
      matrix(dronePosition, {1, 1, 1}, elapsed < 32 ? elapsed * .35f : rf::pi),
      6500000);
  if (particles) {
    dustLeft.draw(graphics);
    dustRight.draw(graphics);
    steam.draw(graphics);
    steam2.draw(graphics);
    sparks.draw(graphics);
    sand.draw(graphics);
    gravel.draw(graphics, rocks[6]);
  }
  auto indicator = elapsed > 47 ? materials.at("cyan") : materials.at("amber");
  graphics.primitive(g::Primitive::Box,
                     {{2.7f, 2.65f, 10.23f}, {.68f, .28f, .035f}, 0},
                     indicator);
  for (float x : {-4.3f, 5.7f})
    graphics.primitive(g::Primitive::Box,
                       {{x, 6, 10.57f}, {.08f, 1.1f, .05f}, 0}, indicator);
  g::Light light;
  light.position = {2.4f, 2.5f, 9.5f};
  light.color = elapsed > 47 ? Color{.1f, .6f, .8f, 1} : Color{1, .45f, .1f, 1};
  light.radius = 7;
  light.intensity =
      elapsed > 34 && elapsed < 47 ? 30 + 15 * std::sin(elapsed * 23) : 6;
  graphics.light(light);
  light.position = {-.8f, 2.2f, -33 + 38 * smooth(0, 24, elapsed)};
  light.color = {1, .83f, .58f, 1};
  light.radius = 12;
  light.intensity = 16;
  graphics.light(light);
  if (hud) {
    graphics.rect(0, 0, 1280, 33, {.012f, .02f, .022f, 1});
    graphics.rect(0, 687, 1280, 33, {.012f, .02f, .022f, 1});
    graphics.text(35, 13, "JSD / REALTIME EXPEDITION", 1.1f,
                  {.7f, .73f, .69f, 1});
    graphics.text(950, 13, "SPACE PAUSE / TAB EXPLORE", 1.1f,
                  {.56f, .6f, .57f, 1});
    float fade = 1 - smooth(3.5f, 6, elapsed);
    if (fade > 0)
      graphics.image(title, 48, 516, 650, 132, {1, 1, 1, fade});
    if (elapsed > 48)
      graphics.image(
          credits, 48, 526, 570, 116,
          {1, 1, 1, smooth(48, 50, elapsed) * (1 - smooth(60, 63, elapsed))});
    std::string phase = elapsed < 24   ? "01 / APPROACH"
                        : elapsed < 36 ? "02 / RECOVER CARGO"
                        : elapsed < 47 ? "03 / REPAIR UPLINK"
                                       : "04 / RELAY ONLINE";
    graphics.text(225, 700, phase, 1.2f, {.75f, .71f, .59f, 1});
    graphics.rect(400, 704, 565, 2, {.17f, .2f, .2f, 1});
    graphics.rect(400, 704, 565 * elapsed / Duration, 2, {.77f, .46f, .18f, 1});
  }
  if (fps >= 0) {
    if (!hud)
      graphics.rect(8, 686, 175, 28, {.008f, .012f, .014f, .72f});
    char label[48];
    if (fps > 0)
      std::snprintf(label, sizeof(label), "%.1f FPS", fps);
    else
      std::snprintf(label, sizeof(label), "-- FPS");
    graphics.text(18, 698, label, 1.6f, {.79f, .94f, .81f, 1});
  }
  graphics.end();
}
} // namespace sundog
