#pragma once
#include "math.hpp"
#include "optics.hpp"
#include "platform.hpp"
#include "settings.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace rf::gfx {
inline constexpr const char *Version = "0.5.25";
struct MeshId {
    unsigned value = 0;
    bool operator==(const MeshId &) const = default;
};
struct TextureId {
    unsigned value = 0;
    bool operator==(const TextureId &) const = default;
};
struct EnvironmentId {
    unsigned value = 0;
    bool operator==(const EnvironmentId &) const = default;
};
struct Vertex {
    V3 p, n;
    Color c;
    float u = 0, v = 0;
};
struct Transform {
    V3 position{}, scale{1, 1, 1};
    float yaw = 0;
    float pitch = 0, roll = 0;
};
enum class TextureProjection { UV, Triplanar };
enum class AlphaMode { Opaque, Mask, Blend, Additive };
enum class SurfaceKind { Textured, Rock, Soil, Concrete, PaintedMetal, BareMetal, Rubber, Regolith, HullPanels, InteriorPanels, RobotArmor, DeckPlating, FabricWeave, Console, PressureDoor, SolarShield, PhotovoltaicCells, CorridorDeck };
struct Material {
    Color albedo{};
    TextureId albedoTexture{};
    float roughness = .8f, metallic = 0;
    Color emissive{0, 0, 0, 1};
    bool castShadow = true, receiveShadow = true;
    TextureId normalTexture{}, ormTexture{}, emissiveTexture{};
    float normalScale = 1, alphaCutoff = .5f;
    AlphaMode alphaMode = AlphaMode::Opaque;
    bool doubleSided = false;
    bool temporalReactive = false;
    TextureProjection projection = TextureProjection::UV;
    float textureScale = 1, macroVariation = 0, clearcoat = 0, clearcoatRoughness = .18f, windStrength = 0,
          occlusionStrength = 1;
    SurfaceKind surface = SurfaceKind::Textured;
    unsigned surfaceSeed = 0;
    bool surfaceWorld = true;
    float wetness = 0, dust = 0, sheen = 0, anisotropy = 0;
    // Use metre-scaled mesh UVs for detail on world-batched or animated objects.
    bool surfaceUV = false;
};
// Screen-width, depth-tested engineering guides, composited after temporal AA.
struct LineSegment { V3 a{},b{}; Color color{}; float width=1; };
struct Draw {
    MeshId mesh;
    Transform transform;
    Material material;
    uint64_t objectId = 0;
    std::optional<Mat4> modelMatrix;
};
enum class Primitive { Box, Rock, Cylinder };
struct Camera {
    V3 position{12, 6, -20}, direction{-.3f, -.15f, 1};
    float fovDegrees = 70, nearPlane = .08f;
    std::optional<CameraOptics> optics;
};
struct Atmosphere {
    bool enabled = false;
    Quality clouds = Quality::Off;
    float time = 0, turbidity = 2, cloudCoverage = .48f, cloudDensity = .8f;
    float cloudBase = 95, cloudThickness = 90;
    V3 wind{3, 0, 1};
};
// Infinite, world-oriented sky sphere. Directional sampling avoids camera
// translation/parallax and automatically respects aspect ratio and vertical FOV.
struct Starfield {
    bool enabled=false;
    float intensity=1.0f;
    unsigned seed=73021;
    Mat4 orientation=Mat4::identity();
};
struct Environment {
    V3 sunDirection{-.45f, .8f, .3f};
    Color sunColor{1, .91f, .79f, 1};
    Color skyColor{.31f, .39f, .48f, 1}, groundColor{.15f, .12f, .09f, 1};
    float sunIntensity = 1.4f, ambientIntensity = .28f, fogDensity = 1, storm = 0;
    EnvironmentId image{};
    float deltaSeconds = 1.f / 60;
    Atmosphere atmosphere;
    // Zero selects the legacy distance-based shadow extent. Nonzero improves local texel density.
    float shadowDistance = 0;
    float imageRotation = 0;
    std::optional<float> sunIlluminanceLux;
    Starfield stars;
};
// Scene-rasterized radiance, box projected inside a bounded region.
struct ReflectionProbe {
    uint64_t id = 1;
    V3 position{}, halfExtent{20, 12, 20};
    unsigned revision = 0, refreshFrames = 60;
    float blendDistance = 2;
};
struct FogVolume {
    V3 position{}, halfExtent{4, 3, 4};
    Color albedo{.6f, .66f, .7f, 1}, emission{0, 0, 0, 1};
    float density = .04f, noiseScale = .6f, anisotropy = .35f;
    V3 velocity{.1f, .2f, 0};
};
enum class LightType { Point, Spot };
enum class LightFalloff { Legacy, InverseSquare };
struct Light {
    V3 position;
    float radius = 10;
    Color color{};
    float intensity = 10;
    V3 direction{0, -1, 0};
    float innerCone = .3f, outerCone = .6f;
    LightType type = LightType::Point;
    LightFalloff falloff = LightFalloff::Legacy;
    float sourceRadiusMetres = .05f;
    // Persistent shadow identity; supported for spotlights with outerCone < pi/2.
    bool castShadow = false;
    uint64_t shadowId = 0;
    unsigned shadowRevision = 0;
};
// Helpers accept luminous flux in lumens; returned intensity is candela.
Light pointLightLumens(V3 position,float lumens,float rangeMetres,Color color={});
Light spotLightLumens(V3 position,V3 direction,float lumens,float rangeMetres,
                      float innerHalfAngleRadians,float outerHalfAngleRadians,Color color={});
// Planckian emitter chromaticity in linear sRGB, normalized to unit luminance.
Color lightColorTemperature(float kelvin);
struct Terrain {
    bool enabled = true;
    unsigned seed = 101;
    Material material{Color{.5f, .29f, .18f, 1}};
    std::function<float(float, float)> height;
    // Increment revision after changing height generation while retaining the seed.
    unsigned revision = 0;
    std::function<Color(float, float)> color;
};
struct ShaderBuildProgress {
    std::string phase;
    unsigned completed{},cached{},compiled{};
    bool caching{};
};
struct RendererStartup {
    bool cacheShaders=true;
    std::filesystem::path cacheDirectory;
    std::function<void(const ShaderBuildProgress&)> progress;
};
struct ShaderCacheStatistics {
    bool supported{},enabled{};
    unsigned loaded{},compiled{},stored{},rejected{};
    double loadMilliseconds{},compileMilliseconds{};
};
struct Context {
    std::function<void *(const char *)> resolve;
    std::function<std::pair<int, int>()> dimensions;
    bool offscreen = false;
    // Optional application-owned output FBO; zero/default retains window output.
    std::function<unsigned()> outputFramebuffer;
};
struct ProceduralScatter {
    uint64_t id=1;
    MeshId mesh;
    Material material;
    V3 center{},halfExtent{20,0,20};
    unsigned count=1000,seed=1,revision=0;
    float minScale=.08f,maxScale=.3f,maxSlope=.7f,heightOffset=.2f;
    std::function<float(float,float)> height;
    std::array<AABB,2> exclusions{};
};

enum class ParticleStyle { Dust, Steam, Spark, Sand };
struct ParticleSprite {
    V3 position{}, velocity{};
    Color color{};
    float size = .1f, rotation = 0, stretch = 0, softness = .3f, age = 0;
    ParticleStyle style = ParticleStyle::Dust;
};
struct Statistics {
    unsigned lineSegments=0,linePreparationWorkers=1;
    std::size_t lineUploadBytes=0;
    unsigned drawCalls = 0, triangles = 0, visibleTerrainTiles = 0, residentTerrainTiles = 0;
    unsigned pendingTerrainJobs = 0, terrainWorkers = 0, postPasses = 0, textureBytes = 0;
    unsigned shadowResolution = 0, msaaSamples = 1, terrainSubdivisions = 1;
    bool hardwareTessellation = false;
    unsigned renderWidth = 0, renderHeight = 0, outputWidth = 0, outputHeight = 0, localLights = 0,
             lightTileOverflows = 0;
    double cpuMilliseconds = 0, gpuMilliseconds = 0;
    bool temporalHistoryValid = false;
    unsigned particleSprites = 0, particleDrawCalls = 0;
    unsigned preparationWorkers = 1;
    double preparationMilliseconds = 0;
    bool lightweightRendering = false;
    unsigned sceneColorAttachments = 4;
    unsigned reflectionProbeFilters = 0, reflectionProbeFaces = 0, activeReflectionProbes = 0, shadowCascades = 0;
    unsigned stochasticLightSamples = 0, fogVolumes = 0, lodInstances = 0, lodTrianglesSaved = 0;
    unsigned proceduralInstances = 0;
    unsigned localShadowMaps = 0, localShadowUpdates = 0;
};

struct SceneObserver {
    virtual ~SceneObserver()=default;
    bool recordOnly=false;
    virtual void meshCreated(MeshId,std::span<const Vertex>) {}
    virtual void textureCreated(TextureId,int,int,std::span<const unsigned char>,bool) {}
    virtual void submitted(const Draw &) {}
    virtual void lightSubmitted(const Light &) {}
};
class Renderer {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    explicit Renderer(Platform &, GraphicsSettings settings = {}, RendererStartup startup = {});
    explicit Renderer(Context, GraphicsSettings settings = {}, RendererStartup startup = {});
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    const Capabilities &capabilities() const;
    const ShaderCacheStatistics& shaderCacheStatistics() const;
    const SettingsReport &settings() const;
    SettingsReport setSettings(GraphicsSettings);
    const Statistics &statistics() const;
    MeshId createMesh(std::span<const Vertex> vertices);
    void updateMesh(MeshId, std::span<const Vertex> vertices);
    void destroyMesh(MeshId);
    TextureId createTexture(int width, int height, std::span<const unsigned char> rgba, bool srgb = true);
    void destroyTexture(TextureId);
    std::pair<int, int> textureDimensions(TextureId) const;
    EnvironmentId createEnvironment(int width, int height, std::span<const float> rgb);
    EnvironmentId createEnvironment(int width, int height, std::span<const float> rgb,
                                    float lightingHighlightLimit);
    void destroyEnvironment(EnvironmentId);
    void light(const Light &);
    void reflectionProbe(const ReflectionProbe &);
    void fogVolume(const FogVolume &);
    // Candidate placement, slope checks and wind run in the vertex shader.
    void scatter(const ProceduralScatter &);
    // Opt-in resident scenery LODs using an estimated screen-space error.
    MeshId createLodMesh(std::span<const Vertex> vertices, float pixelError = 1.5f);
    // Instanced, depth-faded HDR billboards. Positions/sizes use world units; color is linear.
    void particles(std::span<const ParticleSprite>);

    void observe(SceneObserver *);
    void resetHistory();
    bool reloadMaterialShaders(const std::string &vertex, const std::string &fragment, std::string &error);
    void begin(const Camera &, const Environment &environment = {});
    void submit(const Draw &);
    void lines(std::span<const LineSegment> segments,const Mat4& transform=Mat4::identity());
    void primitive(Primitive, const Transform &, const Material &material = {});
    void terrain(const Terrain &);
    void render();
    void image(TextureId, float x, float y, float width, float height, Color tint = {});
    void rect(float x, float y, float width, float height, Color color);
    void line(float x1, float y1, float x2, float y2, Color color, float thickness = 1);
    void text(float x, float y, std::string_view, float scale = 2, Color color = {.86f, .91f, .93f, 1});
    // UI uses a stable 1280x720 logical canvas and is drawn after all effects.
    void end();
    void screenshot(const std::string &ppmPath);
    void screenshot(const std::filesystem::path &ppmPath);
    void screenshot(const char *ppmPath) { screenshot(std::string(ppmPath)); }
    void validateFrame();
    void captureBuffers(const std::filesystem::path &directory);
    void check();
};
std::vector<Vertex> makeBox();
std::vector<Vertex> makeBeveledBox(float bevel = .08f, unsigned segments = 3);
std::vector<Vertex> makeSphere(unsigned segments = 32, unsigned rings = 16);
std::vector<Vertex> makeTorus(float majorRadius = .4f, float minorRadius = .1f, unsigned segments = 40,
                              unsigned sides = 12);
std::vector<Vertex> makeDetailedRock(unsigned seed = 101, unsigned divisions = 12);
std::vector<Vertex> makeTube(std::span<const V3> path, float radius, unsigned sides = 10);
void appendMesh(std::vector<Vertex> &, std::span<const Vertex>, const Mat4 & = Mat4::identity());

std::vector<Vertex> makeRock();
std::vector<Vertex> makeCylinder(unsigned sides = 16);
} // namespace rf::gfx
extern "C" const char *frontier_engine_version();
